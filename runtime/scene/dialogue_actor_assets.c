#include "scene/dialogue_actor_assets.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkDialogueActorAssets {
  BkModel *model;
  BkClipSet *clips;
  BkActorPose *pose;
  BkFaceAssets *face;
  BkEyeAssets *eyes;
  BkFaceState face_state;
  uint32_t root, group;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "dialogue actor: %s", why);
  return 0;
}
void bk_dialogue_actor_assets_destroy(BkDialogueActorAssets *a) {
  if (!a)
    return;
  bk_actor_pose_destroy(a->pose);
  bk_face_assets_destroy(a->face);
  bk_eye_assets_destroy(a->eyes);
  bk_clip_set_destroy(a->clips);
  bk_model_destroy(a->model);
  free(a);
}
BkDialogueActorAssets *bk_dialogue_actor_assets_create(BkResourceStore *store,
                                                       unsigned group,
                                                       const uint32_t clocks[4],
                                                       uint32_t *random,
                                                       char e[256]) {
  if (!store || group >= 5 || !clocks || !random) {
    fail(e, "invalid construction input");
    return NULL;
  }
  BkDialogueActorAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(e, "allocation failed");
    return NULL;
  }
  a->root = BK_MODEL_NONE;
  a->group = group;
  uint32_t rng = *random;
  char name[32];
  BkBlob raw = {0};
  snprintf(name, sizeof(name), "h%02u_70.xan", group + 1);
  if (bk_resources_read(store, "bk3_01", name, &raw, e) != BK_RESOURCE_OK)
    goto bad;
  a->clips = bk_clip_set_decode(raw.data, raw.size, e);
  bk_blob_free(&raw);
  if (!a->clips ||
      bk_resources_read(store, "bk3_01", bk_clip_model_name(a->clips), &raw,
                        e) != BK_RESOURCE_OK)
    goto bad;
  if (bk_model_decode(raw.data, raw.size, &a->model, e) != BK_MODEL_OK)
    goto bad;
  bk_blob_free(&raw);
  for (uint32_t i = 0; i < a->model->frame_count; ++i)
    if (a->model->frames[i].parent_index == BK_MODEL_NONE) {
      if (a->root != BK_MODEL_NONE) {
        fail(e, "multiple body roots");
        goto bad;
      }
      a->root = i;
    }
  if (a->root == BK_MODEL_NONE) {
    fail(e, "missing body root");
    goto bad;
  }
  static const float position[5][3] = {
      {0, 0, 55}, {2, 0, 55}, {0, 0, 55}, {1, 2, 55}, {0, 0, 55}};
  a->pose = bk_actor_pose_create(a->model, a->clips, a->root, NULL,
                                 position[group], 0, 0, 1, e);
  snprintf(name, sizeof(name), "h%02u_70.fam", group + 1);
  a->face = bk_face_assets_create(store, "faces", name, "bk3_01", a->model,
                                  bk_clip_model_name(a->clips), e);
  if (!a->pose || !a->face)
    goto bad;
  a->eyes = bk_eye_assets_create(store, "bk3_01", a->model,
                                 bk_clip_model_name(a->clips),
                                 bk_face_assets_config(a->face), e);
  if (!a->eyes ||
      !bk_face_assets_initialize(a->face, &a->face_state, clocks, &rng, e))
    goto bad;
  *random = rng;
  return a;
bad:
  bk_blob_free(&raw);
  bk_dialogue_actor_assets_destroy(a);
  return NULL;
}
BkActorPose *bk_dialogue_actor_assets_pose(BkDialogueActorAssets *a) {
  return a ? a->pose : NULL;
}
BkFaceAssets *bk_dialogue_actor_assets_face(BkDialogueActorAssets *a) {
  return a ? a->face : NULL;
}
BkEyeAssets *bk_dialogue_actor_assets_eyes(BkDialogueActorAssets *a) {
  return a ? a->eyes : NULL;
}
const BkFaceState *
bk_dialogue_actor_assets_face_state(const BkDialogueActorAssets *a) {
  return a ? &a->face_state : NULL;
}
uint32_t bk_dialogue_actor_assets_root(const BkDialogueActorAssets *a) {
  return a ? a->root : BK_MODEL_NONE;
}
int bk_dialogue_actor_assets_step(BkDialogueActorAssets *a,
                                  BkDialogueActorState *s, BkDialogue *d,
                                  uint8_t *phase, BkTimer *timer,
                                  const BkDialogueActorInput *in,
                                  uint32_t *random, char e[256]) {
  if (!s || !d || !phase || !timer || !in || !random ||
      !isfinite(in->game_seconds) || in->game_seconds < 0 ||
      (double)in->game_seconds * 60 >= INT32_MAX || !isfinite(in->mouth_level))
    return fail(e, "invalid frame input");
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(in->camera_world[i]))
      return fail(e, "invalid camera world");
  bk_dialogue_actor_rules(&s->rules, *phase, d->code_f, d->code_m);
  if (!a)
    return 1;
  s->mouth = s->speaker == d->code_c && *phase == 0 ? in->mouth_level : 0;
  bk_timer_poll(timer, in->timer_clock_ms);
  if (s->rules.expression != -1) {
    s->visibility = 1;
    if (*phase == 5)
      *phase = 0;
    if (s->rules.once == 0 || s->rules.once == 1) {
      if (!bk_actor_pose_request(a->pose, (unsigned)s->rules.clip, e))
        return 0;
      int32_t loops;
      if (s->rules.once == 1) {
        if (!bk_actor_pose_loops(a->pose, (unsigned)s->rules.clip, &loops))
          return fail(e, "invalid completion slot");
        if (loops >= 1)
          d->code_m = 0;
      }
    }
    uint32_t pitch_bits = 0x3db2b55f;
    uint32_t yaw_bits = a->group == 1 ? 0x40400000 : 0x3e32b7fe;
    float pitch, yaw;
    memcpy(&pitch, &pitch_bits, 4);
    memcpy(&yaw, &yaw_bits, 4);
    if (!bk_eye_assets_gaze(a->eyes, a->pose, (int8_t)s->rules.gaze,
                            in->camera_world, pitch, yaw, e) ||
        !bk_eye_assets_select(a->eyes, (uint8_t)s->rules.texture, e))
      return 0;
    BkFaceState face = a->face_state;
    uint32_t rng = *random;
    if (!bk_face_eye_range(&face, 0, (float)d->code_e, &rng, e) ||
        !bk_face_assets_step(a->face, &face, s->rules.expression, s->mouth,
                             in->timestamp_ms, in->face_clocks[0],
                             in->face_clocks[1], in->face_clocks[2], &rng, e))
      return 0;
    a->face_state = face;
    *random = rng;
  } else {
    s->visibility = 0;
    s->rules.clip = 0;
    if (*phase == 1)
      *phase = 2;
  }
  BkActorVisibilityEdit visible = {a->root, s->visibility <= 0};
  return bk_actor_pose_visibility(a->pose, &visible, 1, e) &&
         bk_actor_pose_advance(a->pose, -1, (float)(.2 * in->game_seconds), e);
}
