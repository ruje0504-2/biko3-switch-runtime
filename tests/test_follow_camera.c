#include "world/actor_forest.h"
#include "world/actor_pose.h"
#include "world/follow_camera.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void word(uint8_t *p, uint32_t x) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(x >> (i * 8));
}
static void number(uint8_t *p, float x) {
  uint32_t bits;
  memcpy(&bits, &x, 4);
  word(p, bits);
}
static void identity(float *m) {
  memset(m, 0, 64);
  m[0] = m[5] = m[10] = m[15] = 1;
}
static void obstacle_edges(void) {
  char error[256];
  float vertices[3][3] = {{-10, 1000, 0}, {10, 1000, 0}, {10, 1000, 0}};
  uint32_t indices[6] = {0, 1, 2, 0, 1, 99};
  float normals[2][3] = {{0, 0, 1}, {0, 0, 1}};
  BkCollisionMesh mesh = {.vertex_count = 3,
                          .index_count = 3,
                          .vertices = vertices,
                          .indices = indices,
                          .normals = normals};
  float actor[3] = {-10, 7, -10}, camera[3] = {10, 17, 10};
  BkFollowObstacle obstacle = {.distance = 100, .point = {13, 19, 23}},
                   before = obstacle;
  int hit = 37;
  /* A real intersection at the origin is the original miss sentinel. */
  assert(bk_follow_obstacle_mesh(&obstacle, &mesh, actor, camera, &hit, error));
  assert(!hit && !memcmp(&obstacle, &before, sizeof(before)));
  vertices[0][0] = 0;
  vertices[1][0] = vertices[2][0] = 20;
  actor[0] = -5;
  camera[0] = 15;
  assert(bk_follow_obstacle_mesh(&obstacle, &mesh, actor, camera, &hit, error));
  assert(hit && obstacle.point[0] == 5 && obstacle.point[1] == 17 &&
         obstacle.point[2] == 0);
  assert(obstacle.distance == (float)sqrt(200));
  /* Wall height1000 does not gate this XZ query; equal distances still hit. */
  before = obstacle;
  assert(bk_follow_obstacle_mesh(&obstacle, &mesh, actor, camera, &hit, error));
  assert(hit && !memcmp(&obstacle, &before, sizeof(before)));
  mesh.index_count = 6;
  hit = 37;
  assert(
      !bk_follow_obstacle_mesh(&obstacle, &mesh, actor, camera, &hit, error));
  assert(hit == 37 && !memcmp(&obstacle, &before, sizeof(before)));
  mesh.index_count = 3;
  actor[0] = camera[0] = 5;
  assert(bk_follow_obstacle_mesh(&obstacle, &mesh, actor, camera, &hit, error));
  assert(!hit && obstacle.singular_intersections > 0 &&
         !memcmp(obstacle.point, before.point, sizeof(before.point)));
}
int main(void) {
  obstacle_edges();
  uint8_t anim[72 + 24 + 2 * 220] = {0}, xan[0x5190] = {0};
  BkModelFrame frames[2] = {0};
  frames[0].id = 100;
  frames[0].parent_index = BK_MODEL_NONE;
  frames[1].id = 101;
  frames[1].parent_index = 0;
  strcpy(frames[0].name, "root");
  strcpy(frames[1].name, "export head");
  identity(frames[0].local);
  identity(frames[1].local);
  frames[1].local[13] = 1;
  frames[1].local[14] = 20;
  BkModelChunk chunk = {"ANIM", 0, sizeof(anim)};
  BkModel model = {.source = anim,
                   .source_size = sizeof(anim),
                   .chunks = &chunk,
                   .chunk_count = 1,
                   .frames = frames,
                   .frame_count = 2};
  word(anim + 68, 1);
  word(anim + 72, 101);
  word(anim + 92, 2);
  for (unsigned i = 0; i < 2; i++) {
    uint8_t *k = anim + 96 + i * 220;
    number(k, (float)i * 20);
    word(k + 4, 1);
    word(k + 20, 1);
    word(k + 36, 1);
    number(k + 8, (float)i * 20);
    number(k + 12, 20);
    number(k + 16, 40);
    number(k + 40, 1);
    number(k + 44, 1);
    number(k + 48, 1);
    number(k + 128, 1);
  }
  memcpy(xan, "camera.x", 9);
  memcpy(xan + 256, "camera.x", 9);
  word(xan + 512 + 0x190 + 0x50, 20);
  number(xan + 512 + 0x190 + 0x54, 1);
  number(xan + 512 + 0x190 + 0x58, 20);
  memcpy(xan + 512 + 0x190 + 156, xan + 512 + 0x190, 156);
  char error[256];
  BkClipSet *clips = bk_clip_set_decode(xan, sizeof(xan), error);
  assert(clips);
  BkCameraFollowPose initial = {0};
  identity(initial.world);
  initial.world[13] = initial.position[1] = 20;
  assert(!bk_follow_camera_create(&model, clips, 1, 0, &initial, error));
  assert(!bk_follow_camera_create(&model, clips, 0, 0, &initial, error));
  BkFollowCamera *camera =
      bk_follow_camera_create(&model, clips, 0, 1, &initial, error);
  assert(camera);
  float published[16];
  memcpy(published, bk_follow_camera_track(camera), 64);
  const float origin[] = {10, 0, 10}, head[] = {10, 25, 20};
  assert(bk_follow_camera_step(camera, origin, head, NULL, 0, error));
  BkCameraFollowPose saved = *bk_follow_camera_pose(camera);
  BkClipState before, after;
  assert(bk_follow_camera_clip_state(camera, &before));
  const float bad_seconds[] = {-1, NAN, INFINITY, 1e30f};
  const float bad_point[] = {NAN, 1, 2};
  for (unsigned i = 0; i < 4; i++)
    assert(!bk_follow_camera_step(camera, origin, head, NULL, bad_seconds[i],
                                  error));
  assert(!bk_follow_camera_step(camera, bad_point, head, NULL, .1f, error));
  assert(!bk_follow_camera_step(camera, origin, bad_point, NULL, .1f, error));
  assert(!bk_follow_camera_step(camera, origin, head, bad_point, .1f, error));
  assert(!bk_follow_camera_step(camera, origin, initial.position, NULL, .1f,
                                error));
  assert(bk_follow_camera_clip_state(camera, &after));
  assert(!memcmp(&before, &after, sizeof(before)));
  assert(!memcmp(&saved, bk_follow_camera_pose(camera), sizeof(saved)));
  assert(!memcmp(published, bk_follow_camera_track(camera), 64));
  assert(bk_follow_camera_step(camera, origin, head, NULL, .1f, error));
  assert(bk_follow_camera_step(camera, origin, head, NULL, .1f, error));
  assert(!memcmp(published, bk_follow_camera_track(camera), 64));
  assert(bk_follow_camera_clip_state(camera, &before));
  bk_follow_camera_publish(camera);
  assert(memcmp(published, bk_follow_camera_track(camera), 64));
  assert(bk_follow_camera_track(camera)[12] > 10);
  assert(bk_follow_camera_track(camera)[14] == 50);
  assert(bk_follow_camera_clip_state(camera, &after));
  assert(!memcmp(&before, &after, sizeof(before)));
  BkActorPose *track = bk_follow_camera_bind_track(camera);
  assert(track &&
         bk_follow_camera_track(camera) == bk_actor_pose_frame(track, 1));
  BkActorForest *forest = bk_actor_forest_create(&track, 1, error);
  assert(forest);
  uint32_t track_root = bk_actor_forest_node(forest, 0, 0);
  assert(bk_actor_forest_attach(forest, 0, track_root, error));
  memcpy(published, bk_follow_camera_track(camera), 64);
  const float track_moved[] = {100, 0, 100};
  assert(bk_follow_camera_step(camera, track_moved, head, NULL, .1f, error));
  assert(!memcmp(published, bk_follow_camera_track(camera), 64));
  const BkFrameVisit *visits;
  uint32_t count;
  /* Camera-only traversal stops before the track root. */
  assert(bk_actor_forest_draw(forest, 1, &visits, &count, error));
  assert(!memcmp(published, bk_follow_camera_track(camera), 64));
  assert(bk_actor_forest_draw(forest, track_root, &visits, &count, error));
  assert(memcmp(published, bk_follow_camera_track(camera), 64));
  assert(bk_follow_camera_track(camera)[14] == 140);
  bk_actor_forest_destroy(forest);
  bk_follow_camera_destroy(camera);
  BkActorPose *actor =
      bk_actor_pose_create(&model, clips, 0, "head", origin, 0, 0, 1, error);
  assert(actor);
  float local_before[16];
  memcpy(local_before, bk_actor_pose_local(actor, 1), sizeof(local_before));
  assert(bk_actor_pose_local(actor, 0)[12] == 10);
  assert(bk_actor_pose_local(actor, 1)[12] == 0);
  assert(bk_actor_pose_frame(actor, 0)[12] == 10);
  assert(bk_actor_pose_head(actor)[0] == 0 &&
         bk_actor_pose_head(actor)[2] == 20);
  assert(bk_actor_pose_state(actor, &before));
  bk_actor_pose_publish(actor);
  assert(bk_actor_pose_head(actor)[0] == 10 &&
         bk_actor_pose_head(actor)[2] == 30);
  assert(bk_actor_pose_state(actor, &after) &&
         !memcmp(&before, &after, sizeof(before)));
  const float moved[] = {20, 0, 30};
  for (unsigned i = 0; i < 4; i++) {
    assert(!bk_actor_pose_step(actor, moved, 0, 1, bad_seconds[i], error));
    assert(bk_actor_pose_state(actor, &after) &&
           !memcmp(&before, &after, sizeof(before)));
    assert(bk_actor_pose_frame(actor, 0)[12] == 10);
    assert(!memcmp(local_before, bk_actor_pose_local(actor, 1), 64));
    assert(bk_actor_pose_local(actor, 0)[12] == 10);
  }
  assert(bk_actor_pose_step(actor, moved, 0, 0, 0, error));
  assert(bk_actor_pose_frame(actor, 0)[12] == 20);
  assert(bk_actor_pose_local(actor, 0)[12] == 20);
  assert(memcmp(local_before, bk_actor_pose_local(actor, 1), 64));
  assert(bk_actor_pose_local(actor, 1)[14] == 40);
  assert(bk_actor_pose_head(actor)[0] == 10 &&
         bk_actor_pose_head(actor)[2] == 30);
  bk_actor_pose_publish(actor);
  assert(bk_actor_pose_head(actor)[0] > 20 &&
         bk_actor_pose_head(actor)[2] == 70);
  assert(!bk_actor_pose_frame(actor, 2));
  assert(!bk_actor_pose_local(actor, 2));
  float held_head[3], held_local[16];
  memcpy(held_head, bk_actor_pose_head(actor), 12);
  memcpy(held_local, bk_actor_pose_local(actor, 1), 64);
  assert(bk_actor_pose_state(actor, &before));
  assert(!bk_actor_pose_place(actor, bad_point, 0, error));
  assert(bk_actor_pose_place(actor, origin, 90, error));
  assert(bk_actor_pose_frame(actor, 0)[12] == 10);
  assert(!memcmp(held_local, bk_actor_pose_local(actor, 1), 64));
  assert(!memcmp(held_head, bk_actor_pose_head(actor), 12));
  assert(bk_actor_pose_state(actor, &after));
  assert(!memcmp(&before, &after, sizeof(before)));
  /* Native updates a hidden node itself, but not descendants, even when
   * a later edit explicitly shows one of those descendants. */
  BkActorVisibilityEdit hidden[] = {{0, 1}, {1, 0}};
  assert(bk_actor_pose_visibility(actor, hidden, 2, error));
  bk_actor_pose_publish(actor);
  assert(!memcmp(held_head, bk_actor_pose_head(actor), 12));
  hidden[0].hidden = 0;
  hidden[1].hidden = 1;
  assert(bk_actor_pose_visibility(actor, hidden, 2, error));
  bk_actor_pose_publish(actor);
  assert(memcmp(held_head, bk_actor_pose_head(actor), 12));
  uint32_t hidden_value;
  assert(bk_actor_pose_hidden(actor, 1, &hidden_value) && hidden_value == 1);
  hidden[0].hidden = 9;
  hidden[1].frame = 2;
  assert(!bk_actor_pose_visibility(actor, hidden, 2, error));
  assert(bk_actor_pose_hidden(actor, 0, &hidden_value) && hidden_value == 0);
  assert(bk_actor_pose_hidden(actor, 1, &hidden_value) && hidden_value == 1);
  /* Hidden root receives a new request but never enters the scheduler,
   * including a zero-dt step. Reveal retains the native first-step swallow. */
  hidden[0] = (BkActorVisibilityEdit){0, 255};
  assert(bk_actor_pose_visibility(actor, hidden, 1, error));
  memcpy(held_local, bk_actor_pose_local(actor, 1), 64);
  assert(bk_actor_pose_step(actor, moved, 90, 1, 5, error));
  assert(bk_actor_pose_state(actor, &before));
  assert(before.requested == 1 && before.slot == 1 && before.elapsed == 0 &&
         before.blend_elapsed == 0 && !before.blend_done);
  assert(!memcmp(held_local, bk_actor_pose_local(actor, 1), 64));
  assert(bk_actor_pose_step(actor, moved, 90, 1, 0, error));
  assert(bk_actor_pose_state(actor, &after));
  assert(!memcmp(&before, &after, sizeof(before)));
  for (unsigned i = 0; i < 4; i++) {
    assert(!bk_actor_pose_step(actor, origin, 0, 0, bad_seconds[i], error));
    assert(bk_actor_pose_state(actor, &after));
    assert(!memcmp(&before, &after, sizeof(before)));
    assert(!memcmp(held_local, bk_actor_pose_local(actor, 1), 64));
    assert(bk_actor_pose_frame(actor, 0)[12] == moved[0]);
  }
  assert(!bk_actor_pose_step(actor, bad_point, 0, 0, 1, error));
  assert(!bk_actor_pose_step(actor, origin, 0, 127, 1, error));
  assert(bk_actor_pose_state(actor, &after));
  assert(!memcmp(&before, &after, sizeof(before)));
  hidden[0].hidden = 0;
  assert(bk_actor_pose_visibility(actor, hidden, 1, error));
  assert(bk_actor_pose_step(actor, moved, 90, 1, .1f, error));
  assert(bk_actor_pose_state(actor, &after));
  assert(after.elapsed == 0 && after.blend_elapsed == 2e-6f);
  assert(bk_actor_pose_step(actor, moved, 90, 1, .1f, error));
  assert(bk_actor_pose_state(actor, &after) && after.elapsed > 0);
  assert(
      !bk_actor_pose_create(&model, clips, 0, "root", origin, 0, 0, 1, error));
  assert(!bk_actor_pose_create(&model, clips, 0, "missing", origin, 0, 0, 1,
                               error));
  bk_actor_pose_destroy(actor);
  /* Request alone must not submit even a zero-time sample. This is the idle
   * phase's early401b0a call, before collision and later presentation. */
  actor =
      bk_actor_pose_create(&model, clips, 0, "head", origin, 0, 0, 1, error);
  assert(actor);
  memcpy(held_local, bk_actor_pose_local(actor, 1), 64);
  memcpy(held_head, bk_actor_pose_head(actor), 12);
  assert(bk_actor_pose_request(actor, 1, error));
  assert(bk_actor_pose_state(actor, &before) && before.requested == 1 &&
         !before.blend_done && before.blend_elapsed == 0);
  assert(!memcmp(held_local, bk_actor_pose_local(actor, 1), 64) &&
         !memcmp(held_head, bk_actor_pose_head(actor), 12));
  assert(bk_actor_pose_request(actor, 1, error));
  assert(!bk_actor_pose_request(actor, 127, error));
  assert(bk_actor_pose_state(actor, &after) &&
         !memcmp(&before, &after, sizeof(before)));
  assert(bk_actor_pose_advance(actor, -1, .05f, error));
  assert(bk_actor_pose_state(actor, &after) && after.blend_elapsed == 2e-6f);
  bk_actor_pose_destroy(actor);
  /* Auxiliary XANs can have no ANIM or head. Their clocks still advance,
   * while placement and publish retain the normal cache boundary. */
  model.chunk_count = 0;
  actor = bk_actor_pose_create(&model, clips, 0, NULL, origin, 0, 0, 1, error);
  assert(actor && !bk_actor_pose_head(actor));
  assert(bk_actor_pose_step(actor, origin, 0, -1, 0, error));
  assert(bk_actor_pose_state(actor, &before));
  assert(bk_actor_pose_step(actor, moved, 0, -1, .05f, error));
  assert(bk_actor_pose_state(actor, &after));
  assert(memcmp(&before, &after, sizeof(before)));
  assert(bk_actor_pose_local(actor, 1)[13] == 1);
  assert(bk_actor_pose_frame(actor, 1)[14] == 20);
  bk_actor_pose_publish(actor);
  assert(bk_actor_pose_frame(actor, 1)[12] == 20 &&
         bk_actor_pose_frame(actor, 1)[14] == 50);
  float parent[16], local[16], held_parent[16];
  identity(parent);
  parent[12] = 1000;
  assert(bk_actor_pose_publish_under(actor, parent, error));
  assert(bk_actor_pose_frame(actor, 0)[12] == 1020);
  assert(bk_actor_pose_frame(actor, 1)[12] == 1020);
  assert(bk_actor_pose_parent_world(actor, 0)[12] == 1000);
  assert(bk_actor_pose_local(actor, 0)[12] == 20);
  identity(local);
  local[12] = 3;
  local[14] = 10;
  assert(bk_actor_pose_root_local(actor, local, error));
  assert(bk_actor_pose_frame(actor, 0)[12] == 1003);
  assert(bk_actor_pose_frame(actor, 1)[12] == 1020);
  memcpy(held_parent, bk_actor_pose_parent_world(actor, 1), 64);
  assert(bk_actor_pose_advance(actor, -1, .05f, error));
  assert(!memcmp(local, bk_actor_pose_local(actor, 0), 64));
  assert(bk_actor_pose_frame(actor, 0)[12] == 1003);
  assert(!memcmp(held_parent, bk_actor_pose_parent_world(actor, 1), 64));
  parent[12] = 1100;
  assert(bk_actor_pose_publish_under(actor, parent, error));
  assert(bk_actor_pose_frame(actor, 1)[12] == 1103 &&
         bk_actor_pose_frame(actor, 1)[14] == 30);
  BkActorPlacement exact = {.position = {8, 9, 10}, .yaw_degrees = 23};
  identity(exact.world);
  exact.world[12] = 1200;
  assert(bk_actor_pose_place_exact(actor, &exact, error));
  assert(bk_actor_pose_local(actor, 0)[12] == 100);
  assert(bk_actor_pose_frame(actor, 0)[12] == 1200);
  assert(bk_actor_pose_frame(actor, 1)[12] == 1103);
  assert(bk_actor_pose_advance(actor, -1, 0, error));
  assert(bk_actor_pose_frame(actor, 0)[12] == 1200);
  assert(bk_actor_pose_local(actor, 0)[12] == 100);
  BkActorVisibilityEdit hide_root = {0, 1};
  assert(bk_actor_pose_visibility(actor, &hide_root, 1, error));
  memcpy(held_parent, bk_actor_pose_parent_world(actor, 1), 64);
  parent[12] = 1300;
  assert(bk_actor_pose_publish_under(actor, parent, error));
  assert(bk_actor_pose_frame(actor, 0)[12] == 1400);
  assert(bk_actor_pose_frame(actor, 1)[12] == 1103);
  assert(!memcmp(held_parent, bk_actor_pose_parent_world(actor, 1), 64));
  float held_root[16];
  memcpy(held_root, bk_actor_pose_frame(actor, 0), 64);
  parent[0] = NAN;
  assert(!bk_actor_pose_publish_under(actor, parent, error));
  assert(!bk_actor_pose_root_local(actor, parent, error));
  assert(!memcmp(held_root, bk_actor_pose_frame(actor, 0), 64));
  identity(parent);
  parent[15] = .5f;
  assert(!bk_actor_pose_publish_under(actor, parent, error));
  assert(!memcmp(held_root, bk_actor_pose_frame(actor, 0), 64));
  parent[15] = 1;
  parent[0] = 0;
  assert(bk_actor_pose_publish_under(actor, parent, error));
  memcpy(held_root, bk_actor_pose_frame(actor, 0), 64);
  assert(!bk_actor_pose_place_exact(actor, &exact, error));
  assert(!memcmp(held_root, bk_actor_pose_frame(actor, 0), 64));
  /* Global attachment refresh deliberately publishes hidden descendants;
   * a selected frame's external parent need not be its asset parent. */
  BkActorPlacement held_placement = *bk_actor_pose_placement(actor);
  identity(parent);
  parent[12] = 321;
  parent[15] = .75f;
  assert(bk_actor_pose_publish_node(actor, 1, parent, error));
  assert(bk_actor_pose_frame(actor, 1)[12] == 321 &&
         bk_actor_pose_frame(actor, 1)[15] == .75f);
  assert(!memcmp(parent, bk_actor_pose_parent_world(actor, 1), 64));
  assert(!memcmp(held_root, bk_actor_pose_frame(actor, 0), 64));
  assert(!memcmp(&held_placement, bk_actor_pose_placement(actor),
                 sizeof(held_placement)));
  memcpy(held_parent, bk_actor_pose_parent_world(actor, 1), 64);
  memcpy(local, bk_actor_pose_frame(actor, 1), 64);
  parent[2] = NAN;
  assert(!bk_actor_pose_publish_node(actor, 1, parent, error));
  assert(!bk_actor_pose_publish_node(actor, 2, held_parent, error));
  assert(!memcmp(local, bk_actor_pose_frame(actor, 1), 64));
  assert(!memcmp(held_parent, bk_actor_pose_parent_world(actor, 1), 64));
  bk_actor_pose_destroy(actor);
  model.chunk_count = 1;
  bk_clip_set_destroy(clips);
  puts("PASS: camera phase publication, no-publication updates, binding "
       "checks, atomic rejection");
  return 0;
}
