#include "scene/selection_actor_assets.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkSelectionActorAssets {
  BkModel *model;
  BkClipSet *clips;
  BkActorPose *pose;
  BkFaceAssets *face;
  BkEyeAssets *eyes;
  BkAudioClip *voice;
  BkFaceState face_state;
  uint32_t root, focus;
  uint8_t alternate;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "selection actor: %s", why);
  return 0;
}
void bk_selection_actor_assets_destroy(BkSelectionActorAssets *a) {
  if (!a)
    return;
  bk_actor_pose_destroy(a->pose);
  bk_face_assets_destroy(a->face);
  bk_eye_assets_destroy(a->eyes);
  bk_audio_clip_release(a->voice);
  bk_clip_set_destroy(a->clips);
  bk_model_destroy(a->model);
  free(a);
}
BkSelectionActorAssets *
bk_selection_actor_assets_create(BkResourceStore *store, unsigned group,
                                 uint8_t alternate, const uint32_t clocks[4],
                                 uint32_t *random, char e[256]) {
  if (!store || group >= 5 || alternate > 1 || !clocks || !random) {
    fail(e, "invalid construction input");
    return NULL;
  }
  BkSelectionActorAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(e, "allocation failed");
    return NULL;
  }
  a->alternate = alternate;
  a->root = a->focus = BK_MODEL_NONE;
  uint32_t rng = *random;
  BkBlob raw = {0};
  char name[32];
  snprintf(name, sizeof(name), "h%02u_%u.xan", group + 1, alternate ? 61 : 60);
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
  a->pose = bk_actor_pose_create(a->model, a->clips, a->root, NULL,
                                 (float[3]){0}, 0, 0, 1, e);
  if (!a->pose ||
      !bk_actor_pose_root_local(a->pose, a->model->frames[a->root].local, e))
    goto bad;
  snprintf(name, sizeof(name), "h%02u_%u.fam", group + 1, alternate ? 61 : 60);
  a->face = bk_face_assets_create(store, "faces", name, "bk3_01", a->model,
                                  bk_clip_model_name(a->clips), e);
  if (!a->face)
    goto bad;
  a->eyes = bk_eye_assets_create(store, "bk3_01", a->model,
                                 bk_clip_model_name(a->clips),
                                 bk_face_assets_config(a->face), e);
  if (!a->eyes ||
      !bk_face_assets_initialize(a->face, &a->face_state, clocks, &rng, e))
    goto bad;
  static const char *const focus[2][5] = {
      {"Frame27_kubiX", "Frame164_A_kao", "kubiX", "atama", "atama"},
      {"atama", "kubiX", "kubiX", "A_kao", "A_kao"}};
  /*425904 absence leaves NULL, which4bb612 maps to its (0,18,0) focus. */
  char ignored[256];
  bk_model_find_frame(a->model, focus[alternate][group], &a->focus, ignored);
  static const char *const voices[] = {"PT10000.wav", "PT20029.wav",
                                       "PT30011.wav", "PT40001.wav",
                                       "PT51000.wav"};
  a->voice = bk_audio_clip_load(store, "bk3_06", voices[group], e);
  if (!a->voice)
    goto bad;
  *random = rng;
  return a;
bad:
  bk_blob_free(&raw);
  bk_selection_actor_assets_destroy(a);
  return NULL;
}
BkActorPose *bk_selection_actor_assets_pose(BkSelectionActorAssets *a) {
  return a ? a->pose : NULL;
}
BkFaceAssets *bk_selection_actor_assets_face(BkSelectionActorAssets *a) {
  return a ? a->face : NULL;
}
BkEyeAssets *bk_selection_actor_assets_eyes(BkSelectionActorAssets *a) {
  return a ? a->eyes : NULL;
}
BkAudioClip *bk_selection_actor_assets_voice(BkSelectionActorAssets *a) {
  return a ? a->voice : NULL;
}
const BkFaceState *
bk_selection_actor_assets_face_state(const BkSelectionActorAssets *a) {
  return a ? &a->face_state : NULL;
}
uint32_t bk_selection_actor_assets_root(const BkSelectionActorAssets *a) {
  return a ? a->root : BK_MODEL_NONE;
}
uint32_t bk_selection_actor_assets_focus(const BkSelectionActorAssets *a) {
  return a ? a->focus : BK_MODEL_NONE;
}
int bk_selection_actor_assets_needs_movie(const BkSelectionActorAssets *a) {
  return a && a->alternate;
}
int bk_selection_actor_assets_step(BkSelectionActorAssets *a, unsigned selected,
                                   float seconds, float mouth,
                                   const float camera[16], uint32_t timestamp,
                                   const uint32_t clocks[3], uint32_t *random,
                                   char e[256]) {
  if (!a || selected >= 5 || !isfinite(seconds) || seconds < 0 ||
      (double)seconds * 60 >= INT32_MAX || !isfinite(mouth) || !camera ||
      !clocks || !random)
    return fail(e, "invalid actor frame");
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(camera[i]))
      return fail(e, "invalid camera matrix");
  BkClipState old;
  BkSelectionActorRules rules;
  if (!bk_actor_pose_state(a->pose, &old) ||
      !bk_selection_actor_rules(&rules, selected, a->alternate, old.source))
    return fail(e, "invalid old active clip");
  if (!bk_actor_pose_advance(a->pose, 0, (float)(.4 * seconds), e))
    return 0;
  BkActorVisibilityEdit visible = {a->root, 0};
  if (!bk_actor_pose_visibility(a->pose, &visible, 1, e) ||
      !bk_eye_assets_gaze(a->eyes, a->pose, rules.gaze, camera,
                          rules.pitch_limit, rules.yaw_limit, e) ||
      !bk_eye_assets_select(a->eyes, (uint8_t)rules.texture, e))
    return 0;
  BkFaceState next = a->face_state;
  uint32_t rng = *random;
  if (!bk_face_eye_range(&next, 0, rules.eye_max, &rng, e) ||
      !bk_face_assets_step(a->face, &next, rules.expression, mouth, timestamp,
                           clocks[0], clocks[1], clocks[2], &rng, e))
    return 0;
  a->face_state = next;
  *random = rng;
  return 1;
}
