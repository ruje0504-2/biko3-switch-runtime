#include "world/follow_camera.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct BkFollowCamera {
  BkActorPose *track;
  uint32_t root, track_node;
  BkCameraFollowPose pose;
  float matrix[16];
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "follow camera: %s", message);
  return 0;
}
void bk_follow_camera_destroy(BkFollowCamera *c) {
  if (!c)
    return;
  bk_actor_pose_destroy(c->track);
  free(c);
}
BkFollowCamera *bk_follow_camera_create(const BkModel *model,
                                        const BkClipSet *clips, uint32_t root,
                                        uint32_t track_node,
                                        const BkCameraFollowPose *initial,
                                        char error[256]) {
  float inverse[16];
  if (!model || !clips || !initial || root >= model->frame_count ||
      track_node >= model->frame_count ||
      model->frames[root].parent_index != BK_MODEL_NONE ||
      !bk_camera_view(inverse, initial->world)) {
    fail(error, "invalid assets, binding or initial pose");
    return NULL;
  }
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(initial->position[i])) {
      fail(error, "invalid initial smoothing position");
      return NULL;
    }
  /* This verified path uses the loader's identity root. A non-identity asset
   * root needs a separately verified initialization path, not an approximation.
   */
  for (unsigned i = 0; i < 16; i++)
    if (model->frames[root].local[i] != (float)(i % 5 == 0)) {
      fail(error, "non-identity camera asset root unsupported");
      return NULL;
    }
  uint32_t ancestor = track_node;
  for (uint32_t count = 0; ancestor != root && count < model->frame_count;
       count++) {
    if (ancestor == BK_MODEL_NONE || ancestor >= model->frame_count)
      break;
    ancestor = model->frames[ancestor].parent_index;
  }
  if (ancestor != root || track_node == root) {
    fail(error, "track node is not a descendant of the selected root");
    return NULL;
  }
  BkFollowCamera *c = calloc(1, sizeof(*c));
  if (!c) {
    fail(error, "allocation failed");
    return NULL;
  }
  c->track = bk_actor_pose_create(model, clips, root, NULL, (float[3]){0, 0, 0},
                                  0, 0, 1, error);
  if (!c->track) {
    bk_follow_camera_destroy(c);
    return NULL;
  }
  c->root = root;
  c->track_node = track_node;
  c->pose = *initial;
  memcpy(c->matrix, initial->world, 64);
  bk_follow_camera_publish(c);
  return c;
}
int bk_follow_camera_step(BkFollowCamera *c, const float actor_origin[3],
                          const float head[3], const float *correction,
                          float seconds, char error[256]) {
  return bk_follow_camera_step_distance(c, actor_origin, head, correction,
                                        seconds, NULL, error);
}
static int follow_step(BkFollowCamera *c, const float actor_origin[3],
                       const float head[3], const float *correction,
                       float seconds, float *distance,
                       const BkCollision *collision, int enabled,
                       BkFollowObstacle *obstacle, char error[256]) {
  if (!c || !actor_origin || !head || !isfinite(seconds) || seconds < 0)
    return fail(error, "invalid step arguments");
  for (unsigned i = 0; i < 3; i++) {
    if (!isfinite(actor_origin[i]))
      return fail(error, "invalid actor origin");
  }
  const float *track = bk_follow_camera_track(c);
  double dx = (double)actor_origin[0] - track[12],
         dz = (double)actor_origin[2] - track[14];
  volatile double x2 = dx * dx, z2 = dz * dz;
  float target_distance = (float)sqrt(x2 + z2);
  if (!isfinite(target_distance))
    return fail(error, "invalid follow distance");
  BkFollowObstacle obstacle_next = {0};
  if (obstacle) {
    if (!collision || (enabled != 0 && enabled != 1))
      return fail(error, "invalid collision context");
    obstacle_next = *obstacle;
    obstacle_next.distance = target_distance;
    float smoothed[3];
    for (unsigned i = 0; i < 3; ++i) {
      float difference = (float)((double)track[12 + i] - c->pose.position[i]);
      float delta = (float)((double)difference * seconds);
      smoothed[i] = (float)((double)delta + c->pose.position[i]);
    }
    int hit = 0;
    if (enabled &&
        !bk_follow_obstacle_scene(&obstacle_next, collision, actor_origin,
                                  smoothed, &hit, error))
      return 0;
    correction = hit ? obstacle_next.point : NULL;
    target_distance = obstacle_next.distance;
  }
  BkCameraFollowPose next = c->pose;
  if (!bk_camera_follow_pose(&next, bk_follow_camera_track(c) + 12, head,
                             correction, seconds))
    return fail(error, "invalid head target, correction or pose overflow");
  if (!bk_actor_pose_step(c->track, actor_origin, 0, -1,
                          (float)((double)seconds * .5), error))
    return 0;
  if (obstacle)
    *obstacle = obstacle_next;
  c->pose = next;
  memcpy(c->matrix, next.world, 64);
  if (distance)
    *distance = target_distance;
  return 1;
}
int bk_follow_camera_step_distance(BkFollowCamera *c, const float actor[3],
                                   const float head[3], const float *correction,
                                   float seconds, float *distance,
                                   char error[256]) {
  return follow_step(c, actor, head, correction, seconds, distance, NULL, 0,
                     NULL, error);
}
int bk_follow_camera_step_collision(BkFollowCamera *c, const float actor[3],
                                    const float head[3],
                                    const BkCollision *collision, int enabled,
                                    float seconds, BkFollowObstacle *obstacle,
                                    char error[256]) {
  if (!obstacle)
    return fail(error, "missing obstacle state");
  return follow_step(c, actor, head, NULL, seconds, NULL, collision, enabled,
                     obstacle, error);
}

void bk_follow_camera_publish(BkFollowCamera *c) {
  if (c)
    bk_actor_pose_publish(c->track);
}
BkActorPose *bk_follow_camera_bind_track(BkFollowCamera *c) {
  return c ? c->track : NULL;
}
const BkCameraFollowPose *bk_follow_camera_pose(const BkFollowCamera *c) {
  return c ? &c->pose : NULL;
}
const float *bk_follow_camera_track(const BkFollowCamera *c) {
  return c ? bk_actor_pose_frame(c->track, c->track_node) : NULL;
}
int bk_follow_camera_clip_state(const BkFollowCamera *c, BkClipState *state) {
  return c && bk_actor_pose_state(c->track, state);
}
int bk_follow_camera_handover(BkFollowCamera *c,
                              const BkPlayerCameraTarget *player, int head_mode,
                              float seconds, int *complete, char error[256]) {
  if (!c || !bk_player_camera_transition(&c->pose, player, head_mode, seconds,
                                         complete))
    return fail(error, "invalid player handover pose/input");
  memcpy(c->matrix, c->pose.world, 64);
  return 1;
}
int bk_follow_camera_player_view(BkFollowCamera *c, BkPlayerView *state,
                                 BkPlayerViewKind kind,
                                 const BkPlayerViewInput *input,
                                 BkPlayerViewEffects *effects,
                                 char error[256]) {
  if (!c || !state || !input || !effects)
    return fail(error, "invalid player view arguments");
  BkPlayerView next = *state;
  next.pose = c->pose;
  memcpy(next.matrix, c->matrix, 64);
  BkPlayerViewInput in = *input;
  memcpy(in.track, bk_follow_camera_track(c) + 12, 12);
  /* Source/end affect only completion; calculate and validate all pose
   * math before advancing the real timeline. Supply actual values below. */
  in.track_source = in.track_end = 0;
  BkPlayerViewEffects out;
  if (!bk_player_view_step(&next, kind, &in, &out, error))
    return 0;
  if (kind == BK_PLAYER_VIEW_TRACK) {
    BkClipTiming before;
    if (!bk_actor_pose_timing(c->track, 0, &before) ||
        !isfinite(before.source) || !isfinite(before.end))
      return fail(error, "invalid retained track timing");
    if (!bk_actor_pose_step(c->track, in.position, 0, -1, in.seconds, error))
      return 0;
    BkClipTiming timing;
    bk_actor_pose_timing(c->track, 0, &timing);
    out.reset_mode = timing.source >= timing.end;
  }
  c->pose = next.pose;
  memcpy(c->matrix, next.matrix, 64);
  *state = next;
  *effects = out;
  return 1;
}

int bk_follow_camera_failure(BkFollowCamera *c, BkFailureCameraKind kind,
                             const BkFailureCameraInput *input,
                             BkFailureCameraEffects *effects, char error[256]) {
  if (!c)
    return fail(error, "missing failure camera");
  return bk_failure_camera_step(&c->pose, kind, input, effects, error);
}
