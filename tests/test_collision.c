#include "game/npc_scene.h"
#include "game/npc_spatial.h"
#include "game/player_scene.h"
#include "game/player_spatial.h"
#include "world/ground.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void word(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (8 * i));
}
int main(void) {
  char error[256];
  uint8_t *atr = calloc(1, BK_COLLISION_ATR_SIZE);
  assert(atr);
  strcpy((char *)atr, "floor");
  strcpy((char *)atr + 256, "Mesh_Floor_1@ROOM.X");
  strcpy((char *)atr + 516, "Mesh_Floor_0@ROOM.X");
  word(atr + 0x8300, 2);
  word(atr + 0x418200, 1);
  BkModelVertex vertices[3] = {{.position = {-10, 0, -10}},
                               {.position = {-10, 0, 10}},
                               {.position = {10, 0, -10}}};
  uint16_t indices[3] = {0, 1, 2};
  BkModelSubmesh submeshes[2] = {{.name = "ignored padded  0",
                                  .vertex_count = 3,
                                  .index_count = 3,
                                  .vertices = vertices,
                                  .indices = indices},
                                 {.name = "ignored padded  1",
                                  .vertex_count = 3,
                                  .index_count = 3,
                                  .vertices = vertices,
                                  .indices = indices}};
  BkModelMesh mesh = {.name = "Mesh_Floor", .submesh_count = 2};
  BkModelFrame frame = {.name = "floor", .mesh_index = 0};
  BkModel model = {.meshes = &mesh,
                   .mesh_count = 1,
                   .submeshes = submeshes,
                   .submesh_count = 2,
                   .frames = &frame,
                   .frame_count = 1};
  float world[16] = {2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 10, 5, 20, 1};
  BkCollision *c = bk_collision_create(&model, world, 16, "ROOM.X", atr,
                                       BK_COLLISION_ATR_SIZE, error);
  assert(c && bk_collision_count(c) == 2);
  const BkCollisionMesh *first = bk_collision_mesh(c, 0);
  assert(bk_collision_begin_props(c, NULL, 0, error));
  assert(bk_collision_mesh(c, 0) == first && bk_collision_count(c) == 2);
  BkCollisionProp inactive = {.active = 0, .kind = 0};
  assert(bk_collision_begin_props(c, &inactive, 1, error));
  assert(bk_collision_mesh(c, 0) == first);
  assert(!strcmp(first->name, "Mesh_Floor_1@ROOM.X"));
  assert(!strcmp(bk_collision_mesh(c, 1)->name, "Mesh_Floor_0@ROOM.X"));
  assert(first->vertices[0][0] == 0 && first->vertices[0][1] == 5 &&
         first->vertices[0][2] == 10 && first->normals[0][1] == 1);
  float point[3] = {5, 5, 15}, height = 123;
  int hit = 123;
  assert(bk_collision_ground(first, point, &hit, &height, error));
  assert(hit == 1 && height == 5);
  point[1] = 21;
  assert(bk_collision_ground(first, point, &hit, &height, error));
  assert(hit == 1 && height == -99999);
  point[0] = 999;
  height = 123;
  assert(bk_collision_ground(first, point, &hit, &height, error));
  assert(hit == 0 && height == 123);
  point[0] = NAN;
  hit = 123;
  assert(!bk_collision_ground(first, point, &hit, &height, error));
  assert(hit == 123 && height == 123);
  BkNpcSceneState state = {.position = {5, 5, 15}, .behavior = 1};
  BkNpcSceneInput input = {.cone = {.actor_position = {5, 5, 15},
                                    .player_position = {5, 5, 25},
                                    .head_distance = 10,
                                    .facing = 0,
                                    .short_range_action = 1},
                           .sight = {{5, 25, 15}, {5, 25, 25}, 10},
                           .suppressed_actions = {18, 19, 20, 21, 23, 27},
                           .excluded_surface = ""};
  assert(bk_npc_scene_step(&state, c, &input, .25f, error));
  assert(state.visible == 1 && state.behavior == 4 && state.position[1] == 5);
  assert(!strcmp(state.surface_name, "Mesh_Floor_1@ROOM.X"));
  /* An excluded later surface clears the previous recorded name. */
  input.excluded_surface = "Mesh_Floor_0@ROOM.X";
  assert(bk_npc_scene_step(&state, c, &input, .25f, error));
  assert(!state.surface_name[0]);
  input.excluded_surface = "";
  input.cone.player_action = 18;
  state.behavior = 1;
  assert(bk_npc_scene_step(&state, c, &input, .25f, error));
  assert(state.visible == 1 && state.behavior == 1);
  /* Neither a rejected cone nor an occluder can skip ground/surface updates. */
  for (unsigned occluded = 0; occluded < 2; ++occluded) {
    BkNpcSceneState outside = state;
    BkNpcSceneInput query = input;
    outside.position[1] = 9;
    if (occluded) {
      query.sight.start[1] = query.sight.end[1] = 5;
      query.sight.end[2] = 45;
    } else
      query.cone.facing = 180;
    assert(bk_npc_scene_step(&outside, c, &query, .25f, error));
    assert(outside.visible == 0 && outside.behavior == 1 &&
           outside.position[1] == 8 &&
           !strcmp(outside.surface_name, "Mesh_Floor_1@ROOM.X"));
    BkNpcSceneState before_invalid = outside;
    query.sight.end[2] = NAN;
    assert(!bk_npc_scene_step(&outside, c, &query, .25f, error));
    assert(!memcmp(&outside, &before_invalid, sizeof(outside)));
  }
  BkNpcSceneState saved = state;
  input.sight.start[0] = INFINITY;
  assert(!bk_npc_scene_step(&state, c, &input, .25f, error));
  assert(!memcmp(&state, &saved, sizeof(state)));
  input.sight.start[0] = 5;
  for (unsigned i = 0; i < 3; i++) {
    const float bad[] = {NAN, INFINITY, -1};
    assert(!bk_npc_scene_step(&state, c, &input, bad[i], error));
    assert(!memcmp(&state, &saved, sizeof(state)));
  }
  state.hidden = 1;
  saved = state;
  assert(bk_npc_scene_step(&state, c, &input, 100, error));
  assert(!memcmp(&state, &saved, sizeof(state)));
  /* Combined stage ordering and rollback after AI has consumed randomness. */
  uint8_t route_bytes[60] = {0};
  const float route_points[2][4] = {{5, 7, 15, 0}, {5, 7, 20, 0}};
  for (unsigned i = 0; i < 2; i++)
    for (unsigned j = 0; j < 4; j++) {
      uint32_t bits;
      memcpy(&bits, &route_points[i][j], 4);
      word(route_bytes + i * 20 + j * 4, bits);
    }
  BkRoute *route = bk_route_decode(route_bytes, sizeof(route_bytes), error);
  assert(route);
  BkNpcSpatialState spatial = {
      .path = {.position = {5, 7, 15}},
      .ai = {.point = {.motion = {.action = 1, .behavior = 1, .mode = 1},
                       .fade_out = 1}},
      .alpha = 1};
  BkNpcSpatialInput inputs = {
      .group = 1,
      .suppressed_actions = {18, 19, 20, 21, 23, 27},
      .active_clip = 1,
      .short_range_action = 1,
      .player_position = {5, 7, 115},
      .player_direction = {0, 0, 1},
      .player_head = {5, 25, 115},
      .head_world = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 25, 15, 1},
      .head_local = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
      .torso_local = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
      .vertical_offset = 18,
      .excluded_surface = ""};
  BkNpcSpatialState initial_spatial = spatial;
  BkNpcInteractionState interaction = {1, 2, 3},
                        initial_interaction = interaction;
  uint32_t random_state = 123;
  BkNpcSpatialEffects effects, initial_effects;
  memset(&effects, 0x55, sizeof(effects));
  initial_effects = effects;
  for (unsigned failure = 0; failure < 2; failure++) {
    inputs.head_local[1] = failure ? 0 : 2;
    inputs.excluded_surface = failure ? NULL : "";
    assert(!bk_npc_spatial_step(&spatial, &interaction, &random_state, route, c,
                                &inputs, .5f, 0, &effects, error));
    assert(!memcmp(&spatial, &initial_spatial, sizeof(spatial)));
    assert(!memcmp(&interaction, &initial_interaction, sizeof(interaction)));
    assert(!memcmp(&effects, &initial_effects, sizeof(effects)));
    assert(random_state == 123);
  }
  inputs.excluded_surface = "";
  assert(bk_npc_spatial_step(&spatial, &interaction, &random_state, route, c,
                             &inputs, .5f, 0, &effects, error));
  assert(spatial.alpha == .99f && spatial.path.cursor == 1);
  assert(!spatial.ai.point.fade_out && spatial.ai.point.motion.behavior == 4);
  assert(spatial.head.sight.start[1] == 25 && spatial.path.position[1] == 6);
  assert(effects.placement.world[13] == 6 && effects.vertical_position == 24);
  assert(random_state != 123);
  bk_route_destroy(route);
  BkPlayerScene player = {.wall = {.position = {5, 7, 15},
                                   .camera_distance = 100,
                                   .normal = {1, 0, 0},
                                   .near_wall = 1},
                          .wall_heading = 123,
                          .wall_name = "old wall",
                          .surface_name = "old floor"};
  BkPlayerSceneInput player_in = {
      .wall = {.previous = {5, 7, 14}, .height = 25},
      .head_distance = 100,
      .projected_depth = .5f,
      .screen_scale = {1, 1},
      .screen_position = {1024, 768},
      .excluded_surface = ""};
  assert(bk_player_scene_step(&player, c, &player_in, error));
  assert(player.npc_in_view && !player.any_wall && !player.wall.near_wall);
  assert(player.wall.normal[0] == 1 && player.wall_heading == 0);
  assert(!player.wall_name[0]);
  assert(!strcmp(player.surface_name, "Mesh_Floor_1@ROOM.X"));
  assert(fabsf(player.wall.position[1] - 6.2f) < 1e-6f);
  player_in.excluded_surface = "Mesh_Floor_0@ROOM.X";
  assert(bk_player_scene_step(&player, c, &player_in, error));
  assert(!player.surface_name[0]);
  for (unsigned i = 0; i < 4; ++i) {
    BkPlayerSceneInput edge = player_in;
    if (i == 0)
      edge.projected_depth = 1;
    if (i == 1)
      edge.screen_position[0] = -1;
    if (i == 2)
      edge.npc_hidden = 1;
    if (i == 3)
      edge.head_distance = 100.01f;
    int visible = 1;
    assert(bk_player_scene_in_view(&visible, &edge) && !visible);
  }
  BkPlayerScene saved_player = player;
  BkPlayerSpatial spatial_player = {.movement = {.position = {5, 7, 15}},
                                    .scene = player};
  BkPlayerSpatialInput spatial_in = {
      .scene = player_in, .base_head_height = 18, .cached_head_height = 27};
  spatial_in.movement.controls_allowed = 1;
  spatial_in.movement.active_clip = 1;
  for (unsigned i = 0; i < 21; ++i)
    spatial_in.movement.actions[i] = (int32_t)i;
  BkActorPlacement player_root;
  assert(bk_player_spatial_step(&spatial_player, c, &spatial_in, &player_root,
                                error));
  assert(spatial_player.vertical_position == 25);
  assert(spatial_player.movement.position[1] == player_root.position[1]);
  assert(fabsf(player_root.position[1] - 6.2f) < 1e-6f);
  spatial_player.movement.action = 11;
  spatial_player.scene.wall.near_wall = 1;
  strcpy(spatial_player.scene.wall_name, "retained during special action");
  strcpy(spatial_player.scene.surface_name, "retained ground");
  float held_y = spatial_player.movement.position[1];
  assert(bk_player_spatial_step(&spatial_player, NULL, &spatial_in,
                                &player_root, error));
  assert(spatial_player.movement.position[1] == held_y);
  assert(!spatial_player.scene.wall.near_wall);
  assert(!strcmp(spatial_player.scene.surface_name, "retained ground"));
  spatial_player.movement.action = 8;
  assert(bk_player_spatial_step(&spatial_player, c, &spatial_in, &player_root,
                                error));
  assert(spatial_player.vertical_position == 27);
  player_in.wall.previous[0] = NAN;
  assert(!bk_player_scene_step(&player, c, &player_in, error));
  assert(!memcmp(&player, &saved_player, sizeof(player)));
  /* Dynamic props are a temporary suffix; replacement/failure must not
   * corrupt the static prefix or retain freed frame input pointers. */
  BkModelFrame prop_frame = frame;
  strcpy(prop_frame.name, "Kuruma_Atari_Layer1");
  BkModel prop_model = model;
  prop_model.frames = &prop_frame;
  BkCollisionProp props[2] = {{.active = 1,
                               .kind = 13,
                               .model = &prop_model,
                               .world = world,
                               .world_floats = 16,
                               .model_name = "CAR.X",
                               .position = {100, NAN, 200}}};
  const float *static_vertices = first->vertices[0];
  for (unsigned round = 0; round < 32; ++round) {
    assert(bk_collision_begin_props(c, props, 1, error));
    assert(bk_collision_count(c) == 4);
    assert(bk_collision_mesh(c, 0)->vertices[0] == static_vertices);
    const BkCollisionMesh *dynamic = bk_collision_mesh(c, 2);
    assert(!strcmp(dynamic->name, "Mesh_Floor_0@CAR.X"));
    assert(dynamic->vertices[0][0] == 90 && dynamic->vertices[0][1] == 0 &&
           dynamic->vertices[0][2] == 190);
    int8_t kind = 99;
    assert(bk_collision_kind(c, 0, &kind) && kind == -1);
    assert(bk_collision_kind(c, 2, &kind) && kind == 13);
    props[1] = props[0];
    props[1].world_floats = 15;
    assert(!bk_collision_begin_props(c, props, 2, error));
    assert(bk_collision_count(c) == 4 && bk_collision_mesh(c, 2) == dynamic);
    if (round % 2) {
      bk_collision_end_props(c);
      bk_collision_end_props(c);
      assert(bk_collision_count(c) == 2 && !bk_collision_kind(c, 2, &kind));
    }
  }
  props[0].kind = 11;
  props[0].model = NULL;
  props[0].world = NULL;
  assert(bk_collision_begin_props(c, props, 1, error));
  assert(bk_collision_count(c) == 2);
  assert(bk_collision_begin_props(c, NULL, 0, error));
  bk_collision_destroy(c);
  /* Decode failure cleanup, including after a preceding allocated mesh. */
  assert(!bk_collision_create(&model, world, 16, "ROOM.X", atr, 8, error));
  assert(!bk_collision_create(&model, world, 15, "ROOM.X", atr,
                              BK_COLLISION_ATR_SIZE, error));
  strcpy((char *)atr + 516, "missing");
  assert(!bk_collision_create(&model, world, 16, "ROOM.X", atr,
                              BK_COLLISION_ATR_SIZE, error));
  strcpy((char *)atr + 516, "Mesh_Floor_0@ROOM.X");
  indices[0] = 3;
  assert(!bk_collision_create(&model, world, 16, "ROOM.X", atr,
                              BK_COLLISION_ATR_SIZE, error));
  indices[0] = 0;
  word(atr + 0x418200, 129);
  assert(!bk_collision_create(&model, world, 16, "ROOM.X", atr,
                              BK_COLLISION_ATR_SIZE, error));
  word(atr + 0x418200, 1);
  word(atr + 0x8300, 129);
  assert(!bk_collision_create(&model, world, 16, "ROOM.X", atr,
                              BK_COLLISION_ATR_SIZE, error));
  word(atr + 0x8300, 2);
  memset(atr, 'x', 256);
  assert(!bk_collision_create(&model, world, 16, "ROOM.X", atr,
                              BK_COLLISION_ATR_SIZE, error));
  free(atr);
  BkSightSegment a = {{0, 0, 0}, {0, 0, 10}, 10},
                 b = {{0, 100, 5}, {0, 100, 15}, 10};
  assert(bk_sight_crossing(&hit, &a, &b) && !hit); /* collinear misses */
  b.start[0] = -5;
  b.end[0] = 5;
  b.end[2] = 5;
  assert(bk_sight_crossing(&hit, &a, &b) && hit); /* height ignored */
  float triangle[3][3] = {{-10, 5, -10}, {-10, 5, 10}, {10, 5, -10}};
  float p[3] = {0, 0, -5};
  height = 123;
  assert(bk_ground_triangle(&hit, &height, p, triangle) && hit && height == 5);
  triangle[0][0] = NAN;
  hit = 123;
  height = 123;
  assert(!bk_ground_triangle(&hit, &height, p, triangle) && hit == 123 &&
         height == 123);
  puts("PASS: collision ATR mapping/ownership, native ground window, scene "
       "ordering, atomic failures");
  return 0;
}
