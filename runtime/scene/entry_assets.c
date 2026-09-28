#include "scene/entry_assets.h"
#include "game/npc_visibility.h"
#include "game/player_animation.h"
#include "scene/npc_material.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkEntryAssets {
  BkEntryRequest request;
  int player_initialized, npc_initialized;
  BkEntrySelection selection;
  BkRoute *route;
  BkModel *models[3];
  BkClipSet *clips[3];
  BkActorPose *player, *actor;
  BkFollowCamera *camera;
  BkMaterialPose *actor_materials, *player_materials;
  BkFaceAssets *face;
  BkEyeAssets *eyes;
  BkNpcShadow *shadow, *player_shadow;
  float player_base_head_height, actor_base_head_height;
  uint32_t actor_kind, area, head_frame, torso_frame, actor_root, player_root,
      marks[4];
};
void bk_entry_assets_destroy(BkEntryAssets *a) {
  if (!a)
    return;
  bk_follow_camera_destroy(a->camera);
  bk_actor_pose_destroy(a->actor);
  bk_actor_pose_destroy(a->player);
  bk_material_pose_destroy(a->actor_materials);
  bk_material_pose_destroy(a->player_materials);
  bk_face_assets_destroy(a->face);
  bk_eye_assets_destroy(a->eyes);
  bk_npc_shadow_destroy(a->shadow);
  bk_npc_shadow_destroy(a->player_shadow);
  for (unsigned i = 0; i < 3; i++) {
    bk_clip_set_destroy(a->clips[i]);
    bk_model_destroy(a->models[i]);
  }
  bk_route_destroy(a->route);
  free(a);
}
static int load_actor_assets(BkResourceStore *resources, const char *pack,
                             const char *clip_name, BkClipSet **clips,
                             BkModel **model, uint32_t *root, char error[256]) {
  BkBlob data = {0};
  if (bk_resources_read(resources, pack, clip_name, &data, error) !=
      BK_RESOURCE_OK)
    return 0;
  *clips = bk_clip_set_decode(data.data, data.size, error);
  bk_blob_free(&data);
  if (!*clips)
    return 0;
  if (bk_resources_read(resources, pack, bk_clip_model_name(*clips), &data,
                        error) != BK_RESOURCE_OK)
    return 0;
  int ok = bk_model_decode(data.data, data.size, model, error) == BK_MODEL_OK;
  bk_blob_free(&data);
  if (!ok)
    return 0;
  *root = BK_MODEL_NONE;
  for (uint32_t i = 0; i < (*model)->frame_count; i++)
    if ((*model)->frames[i].parent_index == BK_MODEL_NONE) {
      if (*root != BK_MODEL_NONE) {
        snprintf(error, 256, "entry assets: multiple actor roots unsupported");
        return 0;
      }
      *root = i;
    }
  if (*root == BK_MODEL_NONE) {
    snprintf(error, 256, "entry assets: missing root");
    return 0;
  }
  return 1;
}
BkEntryAssets *bk_entry_assets_create(BkResourceStore *resources,
                                      const BkEntryRequest *request,
                                      char error[256]) {
  BkEntrySelection selection;
  if (!resources || !bk_game_entry_select(&selection, request)) {
    snprintf(error, 256, "entry assets: unsupported entry request");
    return NULL;
  }
  BkEntryAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    snprintf(error, 256, "entry assets: allocation failed");
    return NULL;
  }
  a->selection = selection;
  a->request = *request;
  a->actor_kind = request->group;
  a->area = request->area;
  a->torso_frame = BK_MODEL_NONE;
  BkBlob data = {0};
  if (bk_resources_read(resources, "routes", selection.route_file, &data,
                        error) != BK_RESOURCE_OK)
    goto bad;
  a->route = bk_route_decode(data.data, data.size, error);
  bk_blob_free(&data);
  if (!a->route)
    goto bad;
  BkActorPlacement spawn;
  if (!bk_route_placement(&spawn, a->route, selection.route_cursor,
                          selection.player_position[1])) {
    snprintf(error, 256, "entry assets: route cursor/placement invalid for %s",
             selection.route_file);
    goto bad;
  }
  uint32_t roots[3];
  if (!load_actor_assets(resources, "bk3_01", "h00_80.xan", &a->clips[0],
                         &a->models[0], &roots[0], error) ||
      !load_actor_assets(resources, "bk3_01", selection.actor_clip,
                         &a->clips[1], &a->models[1], &roots[1], error) ||
      !load_actor_assets(resources, "bk3_04", selection.camera_clip,
                         &a->clips[2], &a->models[2], &roots[2], error))
    goto bad;
  BkActorPlacement player_spawn;
  if (!bk_player_entry_placement(&player_spawn, request))
    goto bad;
  a->player = bk_actor_pose_create(a->models[0], a->clips[0], roots[0],
                                   "qqq21_atama", player_spawn.position,
                                   player_spawn.yaw_degrees, 0, 1, error);
  if (!a->player)
    goto bad;
  a->player_root = roots[0];
  a->player_materials = bk_material_pose_create(a->models[0], error);
  if (!a->player_materials)
    goto bad;
  a->player_base_head_height = bk_actor_pose_head(a->player)[1];
  a->actor = bk_actor_pose_create(a->models[1], a->clips[1], roots[1],
                                  selection.head_node, spawn.position,
                                  spawn.yaw_degrees, 1, 0, error);
  if (!a->actor)
    goto bad;
  a->actor_root = roots[1];
  a->actor_materials = bk_material_pose_create(a->models[1], error);
  if (!a->actor_materials)
    goto bad;
  char face_name[256];
  size_t clip_length = strlen(selection.actor_clip);
  if (clip_length < 4 || clip_length >= sizeof(face_name)) {
    snprintf(error, 256, "entry assets: invalid actor clip basename");
    goto bad;
  }
  memcpy(face_name, selection.actor_clip, clip_length + 1);
  memcpy(face_name + clip_length - 4, ".fam", 5);
  a->face = bk_face_assets_create(resources, "faces", face_name, "bk3_01",
                                  a->models[1], bk_clip_model_name(a->clips[1]),
                                  error);
  if (!a->face)
    goto bad;
  a->eyes = bk_eye_assets_create(resources, "bk3_01", a->models[1],
                                 bk_clip_model_name(a->clips[1]),
                                 bk_face_assets_config(a->face), error);
  if (!a->eyes)
    goto bad;
  static const char *const marks[4] = {"mark_02", "mark_01", "mark_00",
                                       "mark_03"};
  for (unsigned i = 0; i < 4; i++)
    if (!bk_model_find_frame(a->models[1], marks[i], &a->marks[i], error))
      goto bad;
  a->actor_base_head_height = bk_actor_pose_head(a->actor)[1];
  if (!bk_model_find_frame(a->models[1], selection.head_node, &a->head_frame,
                           error))
    goto bad;
  if ((a->actor_kind == 1 || a->actor_kind == 2) &&
      !bk_model_find_frame(a->models[1],
                           a->actor_kind == 1 ? "hara01" : "hara02",
                           &a->torso_frame, error))
    goto bad;
  uint32_t camera_node;
  BkCameraFollowPose initial;
  if (!bk_model_find_frame(a->models[2], "Cam_AUTO", &camera_node, error) ||
      !bk_game_entry_camera_pose(&initial, spawn.position))
    goto bad;
  a->camera = bk_follow_camera_create(a->models[2], a->clips[2], roots[2],
                                      camera_node, &initial, error);
  if (!a->camera)
    goto bad;
  return a;
bad:
  bk_blob_free(&data);
  bk_entry_assets_destroy(a);
  return NULL;
}
const BkEntrySelection *bk_entry_assets_selection(const BkEntryAssets *a) {
  return a ? &a->selection : NULL;
}
const BkEntryRequest *bk_entry_assets_request(const BkEntryAssets *a) {
  return a ? &a->request : NULL;
}
const BkRoute *bk_entry_assets_route(const BkEntryAssets *a) {
  return a ? a->route : NULL;
}
BkActorPose *bk_entry_assets_player(BkEntryAssets *a) {
  return a ? a->player : NULL;
}
BkActorPose *bk_entry_assets_actor(BkEntryAssets *a) {
  return a ? a->actor : NULL;
}
int bk_entry_assets_initialize_player(BkEntryAssets *a, BkPlayerControl *state,
                                      int32_t actions[21], uint8_t collected[5],
                                      char error[256]) {
  if (!a || a->player_initialized ||
      !bk_player_entry_reset(state, actions, collected, &a->request,
                             a->player_base_head_height)) {
    snprintf(error, 256, "entry player: invalid or already initialized");
    return 0;
  }
  a->player_initialized = 1;
  return 1;
}
int bk_entry_assets_initialize_npc(BkEntryAssets *a, BkNpcSpatialState *state,
                                   BkNpcFootstepActions *actions,
                                   BkNpcEntryRoute *metadata,
                                   uint32_t route_start, float *vertical,
                                   char error[256]) {
  if (!a || a->npc_initialized || !vertical) {
    snprintf(error, 256, "entry NPC: invalid or already initialized");
    return 0;
  }
  float height = (float)((double)a->actor_base_head_height +
                         a->selection.player_position[1]);
  if (!isfinite(height) ||
      !bk_npc_entry_reset(state, actions, metadata, a->route, &a->request,
                          route_start)) {
    snprintf(error, 256, "entry NPC: invalid route/state");
    return 0;
  }
  *vertical = height;
  a->npc_initialized = 1;
  return 1;
}
BkFaceAssets *bk_entry_assets_face(BkEntryAssets *a) {
  return a ? a->face : NULL;
}
const BkMaterialPose *bk_entry_assets_actor_materials(const BkEntryAssets *a) {
  return a ? a->actor_materials : NULL;
}
BkEyeAssets *bk_entry_assets_eyes(BkEntryAssets *a) {
  return a ? a->eyes : NULL;
}
BkFollowCamera *bk_entry_assets_camera(BkEntryAssets *a) {
  return a ? a->camera : NULL;
}
int bk_entry_assets_head_input(const BkEntryAssets *a, float yaw,
                               BkNpcHeadInput *input, char error[256]) {
  if (!a || !input || !isfinite(yaw)) {
    snprintf(error, 256, "entry head: invalid input");
    return 0;
  }
  BkNpcHeadInput next = {.actor_yaw = yaw,
                         .actor_kind = (int32_t)a->actor_kind};
  memcpy(next.actor_head_world, bk_actor_pose_frame(a->actor, a->head_frame),
         64);
  memcpy(next.actor_head_local, bk_actor_pose_local(a->actor, a->head_frame),
         64);
  if (a->torso_frame != BK_MODEL_NONE)
    memcpy(next.torso_local, bk_actor_pose_local(a->actor, a->torso_frame), 64);
  memcpy(next.player_head, bk_actor_pose_head(a->player), 12);
  memcpy(next.player_position, bk_actor_pose_placement(a->player)->position,
         12);
  *input = next;
  return 1;
}
int bk_entry_assets_step_camera(BkEntryAssets *a, BkGameCameraState *state,
                                const float *correction, float seconds,
                                char error[256]) {
  return bk_entry_assets_step_camera_distance(a, state, correction, seconds,
                                              NULL, error);
}
int bk_entry_assets_step_camera_distance(BkEntryAssets *a,
                                         BkGameCameraState *state,
                                         const float *correction, float seconds,
                                         float *distance, char error[256]) {
  if (!a || !state || !isfinite(seconds) || seconds < 0) {
    snprintf(error, 256, "entry camera: invalid step");
    return 0;
  }
  BkGameCameraRoute route = bk_game_camera_route(state);
  if (route == BK_GAME_CAMERA_HOLD)
    return 1;
  if (route == BK_GAME_CAMERA_PLAYER_INPUT) {
    snprintf(error, 256,
             "entry camera: phase1 requires step_player_view context");
    return 0;
  }
  if (route == BK_GAME_CAMERA_FOLLOW_ACTOR)
    return bk_follow_camera_step_distance(
        a->camera, bk_actor_pose_placement(a->actor)->position,
        bk_actor_pose_head(a->actor), correction, seconds, distance, error);
  BkPlayerCameraTarget target = {0};
  const BkActorPlacement *player = bk_actor_pose_placement(a->player);
  memcpy(target.origin, player->position, sizeof(target.origin));
  memcpy(target.head, bk_actor_pose_head(a->player), sizeof(target.head));
  target.base_head_height = a->player_base_head_height;
  target.yaw_degrees = player->yaw_degrees;
  int complete;
  if (!bk_follow_camera_handover(a->camera, &target,
                                 route == BK_GAME_CAMERA_TO_PLAYER_HEAD,
                                 seconds, &complete, error))
    return 0;
  return bk_game_camera_finish(state, complete);
}
int bk_entry_assets_project_actor_head(const BkEntryAssets *a,
                                       BkScreenPoint *point,
                                       const float view[16],
                                       const BkCameraLens *lens,
                                       const BkViewport *viewport) {
  return a && bk_camera_project_frame(
                  point, bk_actor_pose_frame(a->actor, a->head_frame), view,
                  lens, viewport);
}
int bk_entry_assets_step_player_spatial(BkEntryAssets *a,
                                        BkPlayerSpatial *state,
                                        const BkCollision *collision,
                                        const BkPlayerSpatialInput *input,
                                        char error[256]) {
  if (!a || !state || !input) {
    snprintf(error, 256, "entry player: invalid spatial input");
    return 0;
  }
  BkClipState clip;
  const float *head = bk_actor_pose_head(a->player);
  if (!head || !bk_actor_pose_state(a->player, &clip)) {
    snprintf(error, 256, "entry player: unavailable pose state");
    return 0;
  }
  BkPlayerSpatialInput in = *input;
  in.movement.active_clip = clip.slot;
  in.base_head_height = a->player_base_head_height;
  in.cached_head_height = head[1];
  BkPlayerSpatial next = *state;
  BkActorPlacement placement;
  if (!bk_player_spatial_step(&next, collision, &in, &placement, error) ||
      !bk_actor_pose_place(a->player, placement.position, placement.yaw_degrees,
                           error))
    return 0;
  *state = next;
  return 1;
}
int bk_entry_assets_step_player_idle(BkEntryAssets *a, BkPlayerControl *state,
                                     int32_t idle_action,
                                     const BkCollision *collision,
                                     const BkPlayerSceneInput *input,
                                     char error[256]) {
  if (!a || !state || !input || idle_action < 0 ||
      idle_action >= BK_CLIP_SLOTS) {
    snprintf(error, 256, "entry idle player: invalid context/action");
    return 0;
  }
  BkPlayerSpatial next = state->spatial;
  BkActorPlacement root;
  if (!bk_actor_pose_request(a->player, (unsigned)idle_action, error) ||
      !bk_player_idle_step(&next, idle_action, collision, input, &root,
                           error) ||
      !bk_actor_pose_place(a->player, root.position, root.yaw_degrees, error))
    return 0;
  state->spatial = next;
  return 1;
}
int bk_entry_assets_step_player_control(BkEntryAssets *a,
                                        BkPlayerControl *state,
                                        const BkCollision *collision,
                                        const BkPlayerControlInput *input,
                                        BkPlayerControlEffects *effects,
                                        char error[256]) {
  if (!a || !state || !input || !effects) {
    snprintf(error, 256, "entry player: invalid control input");
    return 0;
  }
  BkClipState clip;
  const float *head = bk_actor_pose_head(a->player);
  if (!head || !bk_actor_pose_state(a->player, &clip)) {
    snprintf(error, 256, "entry player: unavailable control pose");
    return 0;
  }
  _Static_assert(BK_PLAYER_CLIP_SLOTS == BK_CLIP_SLOTS, "player clip slots");
  BkPlayerControlInput in = *input;
  in.has_shadow = a->player_shadow != NULL || input->has_shadow;
  in.spatial.movement.active_clip = clip.slot;
  in.spatial.base_head_height = a->player_base_head_height;
  in.spatial.cached_head_height = head[1];
  in.interaction.trigger.group = (int32_t)a->actor_kind;
  in.interaction.trigger.area = (int32_t)a->area;
  for (unsigned i = 0; i < BK_CLIP_SLOTS; ++i) {
    BkClipTiming timing;
    if (!bk_actor_pose_timing(a->player, i, &timing)) {
      snprintf(error, 256, "entry player: unavailable retained timing");
      return 0;
    }
    in.interaction.clips[i] =
        (BkPlayerClipTiming){timing.start, timing.end, timing.source};
  }
  BkPlayerControl next = *state;
  BkPlayerControlEffects out;
  if (!bk_player_control_step(&next, collision, &in, &out, error))
    return 0;
  for (unsigned i = 0; i < out.placements; ++i)
    if (!bk_actor_pose_place(a->player, out.roots[i].position,
                             out.roots[i].yaw_degrees, error))
      return 0;
  if (a->player_shadow && out.shadow_place &&
      !bk_npc_shadow_place_exact(a->player_shadow, &out.shadow, error))
    return 0;
  *state = next;
  *effects = out;
  return 1;
}
int bk_entry_assets_step_player_view(BkEntryAssets *a, BkPlayerControl *player,
                                     BkPlayerView *view, uint8_t *hidden,
                                     const BkEntryPlayerViewInput *input,
                                     char error[256]) {
  if (!a || !player || !view || !hidden || !input) {
    snprintf(error, 256, "entry player view: missing context");
    return 0;
  }
  BkPlayerViewFlags flags = {player->spatial.movement.interaction_mode,
                             *hidden};
  BkPlayerViewKind kind;
  if (!bk_player_view_route(&flags, &kind, input->camera_mode,
                            input->npc_hidden, player->spatial.movement.action,
                            input->actions))
    return 0;
  BkPlayerView next = *view;
  next.target_distance = player->spatial.scene.wall.camera_distance;
  memcpy(next.blocked, player->spatial.scene.wall.rays_blocked, 28);
  const BkActorPlacement *npc = bk_actor_pose_placement(a->actor);
  BkPlayerViewInput in = {.vertical = player->spatial.vertical_position,
                          .yaw = player->spatial.movement.yaw,
                          .pitch = player->spatial.movement.pitch,
                          .npc_yaw = npc->yaw_degrees,
                          .npc_height = a->actor_base_head_height,
                          .npc_vertical = input->npc_vertical,
                          .seconds = input->seconds,
                          .buttons = input->buttons};
  memcpy(in.position, player->spatial.movement.position, 12);
  memcpy(in.head, bk_actor_pose_head(a->player), 12);
  memcpy(in.npc_position, npc->position, 12);
  BkPlayerViewEffects effects;
  if (!bk_follow_camera_player_view(a->camera, &next, kind, &in, &effects,
                                    error))
    return 0;
  if (effects.root_hidden != -1) {
    BkActorVisibilityEdit edit = {a->player_root,
                                  (uint32_t)effects.root_hidden};
    if (!bk_actor_pose_visibility(a->player, &edit, 1, error))
      return 0;
  }
  if (effects.player_hidden != -1)
    flags.hidden = (uint8_t)effects.player_hidden;
  if (effects.reset_mode)
    flags.mode = 0;
  if (effects.write_return_yaw)
    player->return_yaw = effects.return_yaw;
  player->spatial.movement.interaction_mode = flags.mode;
  player->spatial.scene.wall.camera_distance = next.target_distance;
  memcpy(player->spatial.scene.wall.rays_blocked, next.blocked, 28);
  *hidden = flags.hidden;
  *view = next;
  return 1;
}
int bk_entry_assets_step_npc_spatial(BkEntryAssets *a, BkNpcSpatialState *state,
                                     BkNpcInteractionState *interaction,
                                     uint32_t *random_state,
                                     const BkCollision *collision,
                                     const BkEntryNpcContext *context,
                                     float seconds, uint32_t now_ms,
                                     BkNpcSpatialEffects *effects,
                                     char error[256]) {
  if (!a || !state || !interaction || !random_state || !context || !effects) {
    snprintf(error, 256, "entry NPC: invalid spatial input");
    return 0;
  }
  BkNpcHeadInput head;
  BkClipState clip;
  if (!bk_entry_assets_head_input(a, state->path.yaw_degrees, &head, error) ||
      !bk_actor_pose_state(a->actor, &clip))
    return 0;
  BkNpcSpatialInput input = {.group = (int32_t)a->actor_kind,
                             .area = (int32_t)a->area,
                             .player_action = context->player_action,
                             .active_clip = clip.slot,
                             .background_clip = context->background_clip,
                             .short_range_action = context->short_range_action,
                             .vertical_offset = a->actor_base_head_height,
                             .interaction_df = context->interaction_df,
                             .interaction_e0 = context->interaction_e0,
                             .excluded_surface = context->excluded_surface};
  memcpy(input.suppressed_actions, context->suppressed_actions,
         sizeof(input.suppressed_actions));
  memcpy(input.player_direction, context->player_direction, 12);
  memcpy(input.player_position, head.player_position, 12);
  memcpy(input.player_head, head.player_head, 12);
  memcpy(input.head_world, head.actor_head_world, 64);
  memcpy(input.head_local, head.actor_head_local, 64);
  memcpy(input.torso_local, head.torso_local, 64);
  BkNpcSpatialState next = *state;
  BkNpcInteractionState shared = *interaction;
  uint32_t random = *random_state;
  BkNpcSpatialEffects result;
  if (!bk_npc_spatial_step(&next, &shared, &random, a->route, collision, &input,
                           seconds, now_ms, &result, error) ||
      !bk_actor_pose_place(a->actor, result.placement.position,
                           result.placement.yaw_degrees, error))
    return 0;
  *state = next;
  *interaction = shared;
  *random_state = random;
  *effects = result;
  return 1;
}
int bk_entry_assets_step_npc_fade(BkEntryAssets *a, BkNpcSpatialState *state,
                                  float seconds, char error[256]) {
  if (!a || !state) {
    snprintf(error, 256, "entry NPC: missing fade state");
    return 0;
  }
  return bk_npc_fade_apply(&state->alpha, state->ai.point.fade_out,
                           a->actor_kind, seconds, a->actor_materials,
                           a->actor_root, a->marks, error);
}
const BkModelMaterial *bk_entry_assets_actor_material(const BkEntryAssets *a,
                                                      uint32_t index) {
  return a ? bk_material_pose_material(a->actor_materials, index) : NULL;
}
int bk_entry_assets_step_npc_visibility(BkEntryAssets *a,
                                        const BkNpcSpatialState *state,
                                        int8_t interface_mode, int8_t phase,
                                        char error[256]) {
  if (!a || !state) {
    snprintf(error, 256, "entry NPC: missing visibility state");
    return 0;
  }
  BkNpcVisibilityInput input = {.hidden = state->ai.point.motion.hidden,
                                .interface_mode = interface_mode,
                                .phase = phase,
                                .area = (int32_t)a->area,
                                .behavior = state->ai.point.motion.behavior};
  return bk_npc_visibility_apply(a->actor, a->actor_root, a->marks, &input,
                                 error);
}
int bk_entry_assets_load_mesh_shadow(BkEntryAssets *a,
                                     BkResourceStore *resources,
                                     char error[256]) {
  if (!a) {
    snprintf(error, 256, "entry NPC: missing shadow owner");
    return 0;
  }
  if (a->shadow)
    return 1;
  BkNpcShadow *shadow = bk_npc_shadow_create(resources, error);
  if (!shadow)
    return 0;
  a->shadow = shadow;
  return 1;
}
BkNpcShadow *bk_entry_assets_shadow(BkEntryAssets *a) {
  return a ? a->shadow : NULL;
}
int bk_entry_assets_step_npc_presentation(BkEntryAssets *a,
                                          BkNpcSpatialState *state,
                                          BkFaceState *face,
                                          uint32_t *random_state,
                                          const BkEntryNpcPresentation *input,
                                          char error[256]) {
  if (!a || !state || !face || !random_state || !input ||
      !isfinite(input->seconds) || input->seconds < 0 ||
      (double)input->seconds * 60 >= INT32_MAX ||
      !isfinite(input->voice_level) || !isfinite(state->alpha)) {
    snprintf(error, 256, "entry NPC: invalid presentation input");
    return 0;
  }
  int32_t action = state->ai.point.motion.action;
  const BkClipDefinition *clip =
      bk_clip_definition(a->clips[1], (unsigned)action);
  const BkFaceConfig *config = bk_face_assets_config(a->face);
  if (action < 0 || !clip || !clip->active || !config ||
      face->eye_count != config->counts[0] ||
      face->mouth_count != config->counts[1]) {
    snprintf(error, 256, "entry NPC: invalid presentation action/face binding");
    return 0;
  }
  if (!bk_entry_assets_step_npc_visibility(a, state, input->interface_mode,
                                           input->phase, error))
    return 0;
  const BkActorPlacement *p = bk_actor_pose_placement(a->actor);
  if (!bk_actor_pose_step(a->actor, p->position, p->yaw_degrees, action,
                          (float)((double)input->seconds * .5), error) ||
      !bk_face_assets_step(a->face, face, 0, input->voice_level,
                           input->timestamp_ms, input->request_clock_ms,
                           input->mouth_clock_ms, input->blink_clock_ms,
                           random_state, error) ||
      (a->shadow &&
       !bk_npc_shadow_step(a->shadow, state->ai.point.motion.hidden,
                           input->seconds, error)))
    return 0;
  return bk_entry_assets_step_npc_fade(a, state, input->seconds, error);
}
int bk_entry_assets_prepare_npc_footsteps(
    const BkEntryAssets *a, const BkNpcSpatialState *state,
    const BkNpcFootstepActions *actions, int32_t effect_volume,
    uint8_t *latches, size_t count, BkEntryNpcFootsteps *out, char error[256]) {
  if (!a || !state || !actions || !latches ||
      count < BK_NPC_FOOTSTEP_LATCH_COUNT || !out ||
      !memchr(state->surface_name, 0, sizeof(state->surface_name))) {
    snprintf(error, 256, "entry NPC: invalid footstep input");
    return 0;
  }
  BkClipState clip;
  if (!bk_actor_pose_state(a->actor, &clip))
    return 0;
  BkNpcFootstepInput input = {.group = a->actor_kind,
                              .area = a->area,
                              .action = state->ai.point.motion.action,
                              .source_tick = clip.source,
                              .surface_name = state->surface_name,
                              .actions = *actions};
  uint8_t next[BK_NPC_FOOTSTEP_LATCH_COUNT];
  memcpy(next, latches, sizeof(next));
  BkEntryNpcFootsteps result = {0};
  const BkActorPlacement *source = bk_actor_pose_placement(a->actor);
  const BkActorPlacement *listener = bk_actor_pose_placement(a->player);
  if (!bk_npc_footsteps(next, sizeof(next), &input, &result.footsteps) ||
      (result.footsteps.count &&
       !bk_spatial_audio(&result.audio, source->position, listener->position,
                         listener->yaw_degrees, effect_volume, 6))) {
    snprintf(error, 256, "entry NPC: footstep evaluation failed");
    return 0;
  }
  memcpy(latches, next, sizeof(next));
  *out = result;
  return 1;
}

int bk_entry_assets_load_player_shadow(BkEntryAssets *a,
                                       BkResourceStore *resources,
                                       char error[256]) {
  if (!a) {
    snprintf(error, 256, "player shadow: missing entry");
    return 0;
  }
  if (a->player_shadow)
    return 1;
  a->player_shadow = bk_npc_shadow_create(resources, error);
  return a->player_shadow != NULL;
}
BkNpcShadow *bk_entry_assets_player_shadow(BkEntryAssets *a) {
  return a ? a->player_shadow : NULL;
}
const BkMaterialPose *bk_entry_assets_player_materials(const BkEntryAssets *a) {
  return a ? a->player_materials : NULL;
}
int bk_entry_assets_step_player_presentation(
    BkEntryAssets *a, BkPlayerEventState *state, uint8_t *steps,
    size_t step_count, uint8_t *shared, size_t shared_count,
    const BkEntryPlayerPresentation *input, BkPlayerSoundSubmit submit,
    void *context, char error[256]) {
  if (!a || !state || !input || !submit || !steps ||
      step_count < BK_PLAYER_EVENT_LATCH_COUNT || !shared ||
      shared_count < BK_PLAYER_LOOP_LATCH_COUNT || !input->surface ||
      input->action < 0 || input->action >= BK_CLIP_SLOTS ||
      !isfinite(input->seconds) || input->seconds < 0 ||
      (double)input->seconds * 60 >= INT32_MAX) {
    snprintf(error, 256, "entry player: invalid presentation input");
    return 0;
  }
  BkClipState clip;
  if (!bk_actor_pose_state(a->player, &clip))
    return 0;
  BkPlayerEventInput event_input = {.group = (int32_t)a->actor_kind,
                                    .area = (int32_t)a->area,
                                    .action = input->action,
                                    .source = clip.source,
                                    .surface = input->surface,
                                    .voice_present = input->voice_present,
                                    .voice_playing = input->voice_playing};
  memcpy(event_input.actions, input->actions, sizeof(event_input.actions));
  BkMaterialAlphaEdit alpha = {
      .frame = a->player_root,
      .alpha = bk_player_presentation_alpha(
          input->camera_mode, input->interface_mode, input->interaction_mode)};
  BkActorVisibilityEdit hidden = {a->player_root, input->hidden};
  if (!bk_material_pose_alpha(a->player_materials, &alpha, 1, error) ||
      !bk_actor_pose_visibility(a->player, &hidden, 1, error) ||
      (a->player_shadow &&
       !bk_npc_shadow_visibility(a->player_shadow, input->hidden, error)))
    return 0;
  BkPlayerEvents events;
  if (!bk_player_events(state, steps, step_count, shared, shared_count,
                        &event_input, &events)) {
    snprintf(error, 256, "entry player: event evaluation failed");
    return 0;
  }
  if (!submit(context, &events, error))
    return 0;
  BkPlayerAnimationActions actions = {input->actions[0], input->actions[1],
                                      input->actions[3]};
  const BkActorPlacement *p = bk_actor_pose_placement(a->player);
  return bk_player_animation_step(a->player, p->position, p->yaw_degrees,
                                  &actions, input->action, input->seconds,
                                  error) &&
         (!a->player_shadow ||
          bk_npc_shadow_step(a->player_shadow, input->hidden, input->seconds,
                             error));
}

int bk_entry_assets_base_heights(const BkEntryAssets *a, float *player,
                                 float *npc) {
  if (!a || !player || !npc)
    return 0;
  *player = a->player_base_head_height;
  *npc = a->actor_base_head_height;
  return 1;
}

int bk_entry_assets_reload_player(BkEntryAssets *a, BkResourceStore *resources,
                                  const float position[3], float yaw,
                                  char error[256]) {
  if (!a || !resources || !position) {
    snprintf(error, 256, "entry reload: missing player/resources/placement");
    return 0;
  }
  BkActorPose *player =
      bk_actor_pose_create(a->models[0], a->clips[0], a->player_root,
                           "qqq21_atama", position, yaw, 0, 1, error);
  if (!player)
    return 0;
  BkMaterialPose *materials = bk_material_pose_create(a->models[0], error);
  BkNpcShadow *shadow =
      a->player_shadow ? bk_npc_shadow_create(resources, error) : NULL;
  if (!materials || (a->player_shadow && !shadow)) {
    bk_actor_pose_destroy(player);
    bk_material_pose_destroy(materials);
    bk_npc_shadow_destroy(shadow);
    return 0;
  }
  bk_actor_pose_destroy(a->player);
  bk_material_pose_destroy(a->player_materials);
  bk_npc_shadow_destroy(a->player_shadow);
  a->player = player;
  a->player_materials = materials;
  a->player_shadow = shadow;
  a->player_base_head_height = bk_actor_pose_head(player)[1];
  return 1;
}

int bk_entry_assets_advance_area(BkEntryAssets *a, BkResourceStore *resources,
                                 const BkEntryRequest *next,
                                 BkPlayerControl *player,
                                 BkNpcSpatialState *npc, float *vertical,
                                 BkPlayerView *view, BkGameCameraState *phase,
                                 const BkNpcEntryRoute *metadata,
                                 const BkNpcFootstepActions *actions,
                                 char error[256]) {
  BkEntrySelection selection;
  if (!a || !resources || !player || !npc || !vertical || !view || !phase ||
      !metadata || !actions || !bk_game_entry_select(&selection, next) ||
      next->group != a->request.group || a->request.area >= 8 ||
      next->area != a->request.area + 1 || next->route_cursor != 0) {
    snprintf(error, 256, "area assets: invalid live entry/next profile");
    return 0;
  }
  BkBlob data = {0};
  BkRoute *route = NULL;
  BkModel *model = NULL;
  BkClipSet *clips = NULL;
  BkFollowCamera *camera = NULL;
  BkPlayerControl p = *player;
  BkNpcSpatialState n = *npc;
  BkPlayerView v = *view;
  BkGameCameraState c = *phase;
  float height = *vertical;
  if (bk_resources_read(resources, "routes", selection.route_file, &data,
                        error) != BK_RESOURCE_OK)
    goto bad;
  route = bk_route_decode(data.data, data.size, error);
  bk_blob_free(&data);
  if (!route || !bk_area_entry_reset(&p, &n, &height, &v, &c, metadata, actions,
                                     route, next, error))
    goto bad;
  uint32_t root, track;
  if (!load_actor_assets(resources, "bk3_04", selection.camera_clip, &clips,
                         &model, &root, error) ||
      !bk_model_find_frame(model, "Cam_AUTO", &track, error))
    goto bad;
  camera = bk_follow_camera_create(model, clips, root, track, &v.pose, error);
  if (!camera)
    goto bad;
  bk_follow_camera_destroy(a->camera);
  bk_clip_set_destroy(a->clips[2]);
  bk_model_destroy(a->models[2]);
  bk_route_destroy(a->route);
  a->camera = camera;
  a->clips[2] = clips;
  a->models[2] = model;
  a->route = route;
  a->request = *next;
  a->selection = selection;
  a->area = next->area;
  *player = p;
  *npc = n;
  *vertical = height;
  *view = v;
  *phase = c;
  return 1;
bad:
  bk_blob_free(&data);
  bk_follow_camera_destroy(camera);
  bk_clip_set_destroy(clips);
  bk_model_destroy(model);
  bk_route_destroy(route);
  return 0;
}

void bk_entry_assets_unload_track(BkEntryAssets *a) {
  if (!a)
    return;
  bk_follow_camera_destroy(a->camera);
  bk_clip_set_destroy(a->clips[2]);
  bk_model_destroy(a->models[2]);
  a->camera = NULL;
  a->clips[2] = NULL;
  a->models[2] = NULL;
}
