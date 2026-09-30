#include "scene/special_world.h"
#include "core/random.h"
#include "game/draw_dispatch.h"
#include "world/timeline_event.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkSpecialWorld {
  BkModel *model;
  BkClipSet *clips;
  BkActorPose *body;
  BkFaceAssets *face;
  BkEyeAssets *eyes;
  BkFaceState face_state;
  BkSpecialCameraAssets *tracks;
  BkActorForest *forest;
  BkSceneLighting *lighting;
  BkMenuCamera *camera;
  BkEndingCameraTransitions *transitions;
  BkEndingCameraPresets presets;
  uint32_t root, back, focus, roots[3], *random;
  uint8_t *latches;
  size_t latch_count;
  unsigned group;
};
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "special world: %s", why);
  return 0;
}
BkActorPose *bk_special_world_pose(BkSpecialWorld *w, unsigned i) {
  return !w || i >= 3 ? NULL : i == 2 ? w->body
                                         : bk_special_camera_assets_pose(w->tracks, i);
}
unsigned bk_special_world_group(const BkSpecialWorld *w) { return w ? w->group : 5; }
BkActorForest *bk_special_world_forest(BkSpecialWorld *w) { return w ? w->forest : NULL; }
BkFaceAssets *bk_special_world_face(BkSpecialWorld *w) { return w ? w->face : NULL; }
BkEyeAssets *bk_special_world_eyes(BkSpecialWorld *w) { return w ? w->eyes : NULL; }
const BkFaceState *bk_special_world_face_state(const BkSpecialWorld *w) { return w ? &w->face_state : NULL; }
BkSceneLighting *bk_special_world_lighting(BkSpecialWorld *w) { return w ? w->lighting : NULL; }
BkEndingCameraPresets *bk_special_world_presets(BkSpecialWorld *w) { return w ? &w->presets : NULL; }
uint32_t bk_special_world_root(const BkSpecialWorld *w, unsigned i) { return w && i < 3 ? w->roots[i] : BK_FRAME_NONE; }
uint32_t bk_special_world_focus(const BkSpecialWorld *w) { return w ? w->focus : BK_FRAME_NONE; }
uint32_t bk_special_world_back(const BkSpecialWorld *w) { return w ? w->back : BK_MODEL_NONE; }
int bk_special_world_needs_movie(const BkSpecialWorld *w) { return w && (w->group == 1 || w->group == 4); }
void bk_special_world_destroy(BkSpecialWorld *w) {
  if (!w) return;
  bk_actor_forest_destroy(w->forest);
  bk_scene_lighting_destroy(w->lighting);
  bk_special_camera_assets_destroy(w->tracks);
  bk_actor_pose_destroy(w->body);
  bk_face_assets_destroy(w->face);
  bk_eye_assets_destroy(w->eyes);
  bk_clip_set_destroy(w->clips);
  bk_model_destroy(w->model);
  free(w);
}
BkSpecialWorld *bk_special_world_create(BkResourceStore *store, unsigned group,
    float seconds, BkMenuCamera *camera, BkEndingCameraTransitions *transitions,
    const uint32_t clocks[4], uint32_t *random, uint8_t *latches, size_t count,
    char e[256]) {
  if (!store || group >= 5 || !camera || !transitions || !clocks || !random ||
      !latches || count <= 120 || !isfinite(seconds) || seconds < 0) {
    fail(e, "invalid construction services"); return NULL;
  }
  BkSpecialWorld *w = calloc(1, sizeof(*w));
  if (!w) { fail(e, "allocation failed"); return NULL; }
  w->camera = camera; w->transitions = transitions; w->random = random;
  w->latches = latches; w->latch_count = count; w->group = group;
  w->root = w->back = BK_MODEL_NONE;
  uint32_t rng = *random;
  BkMenuCamera next = *camera;
  BkBlob raw = {0}; char name[32];
  snprintf(name, sizeof name, "h%02u_55.xan", group + 1);
  if (bk_resources_read(store, "bk3_14", name, &raw, e) != BK_RESOURCE_OK) goto bad;
  w->clips = bk_clip_set_decode(raw.data, raw.size, e); bk_blob_free(&raw);
  if (!w->clips || bk_resources_read(store, "bk3_14", bk_clip_model_name(w->clips),
                                    &raw, e) != BK_RESOURCE_OK) goto bad;
  BkModelResult result = bk_model_decode(raw.data, raw.size, &w->model, e);
  bk_blob_free(&raw);
  if (result != BK_MODEL_OK) goto bad;
  for (uint32_t n = 0; n < w->model->frame_count; ++n)
    if (w->model->frames[n].parent_index == BK_MODEL_NONE) {
      if (w->root != BK_MODEL_NONE) { fail(e, "multiple roots"); goto bad; }
      w->root = n;
    }
  if (w->root == BK_MODEL_NONE) { fail(e, "missing root"); goto bad; }
  w->body = bk_actor_pose_create_loaded(w->model, w->clips, w->root, (float[3]){0}, 0, e);
  if (!w->body || !bk_actor_pose_root_local(w->body, w->model->frames[w->root].local, e) ||
      !bk_actor_pose_select(w->body, 0, 1, e)) goto bad;
  snprintf(name, sizeof name, "h%02u_55.fam", group + 1);
  w->face = bk_face_assets_create(store, "faces", name, "bk3_01", w->model,
                                  bk_clip_model_name(w->clips), e);
  if (!w->face) goto bad;
  w->eyes = bk_eye_assets_create(store, "bk3_01", w->model,
      bk_clip_model_name(w->clips), bk_face_assets_config(w->face), e);
  if (!w->eyes || !bk_face_assets_initialize(w->face, &w->face_state, clocks, &rng, e)) goto bad;
  /* Native423b01 dereferences this binding; do not invent a null no-op. */
  if (!bk_model_find_frame(w->model, "Back", &w->back, e)) goto bad;
  w->tracks = bk_special_camera_assets_create(store, group, e);
  if (!w->tracks) goto bad;
  BkActorPose *poses[3] = {bk_special_camera_assets_pose(w->tracks, 0),
                          bk_special_camera_assets_pose(w->tracks, 1), w->body};
  w->forest = bk_actor_forest_create(poses, 3, e);
  if (!w->forest || !bk_actor_forest_anchor(w->forest, 1, next.pose.world, 0, e)) goto bad;
  w->roots[2] = bk_actor_forest_node(w->forest, 2, w->root);
  if (!bk_actor_forest_attach(w->forest, 0, w->roots[2], e)) goto bad;
  w->lighting = bk_scene_lighting_create_key(w->model, bk_actor_pose_frame(w->body, 0),
                                         (size_t)w->model->frame_count * 16, NULL, e);
  if (!w->lighting || !bk_special_camera_assets_attach(w->tracks, w->forest,
      (uint32_t[2]){0, 1}, seconds, &next, &w->presets, e)) goto bad;
  for (unsigned i = 0; i < 2; ++i)
    w->roots[i] = bk_actor_forest_node(w->forest, i, bk_special_camera_assets_root(w->tracks, i));
  static const char *const focus[] = {"A_kao", "A_Kao", "kubiX", "mune", "atama"};
  uint32_t frame;
  if (!bk_model_find_frame(w->model, focus[group], &frame, e)) goto bad;
  w->focus = bk_actor_forest_node(w->forest, 2, frame);
  *camera = next; *random = rng;
  return w;
bad:
  bk_blob_free(&raw); bk_special_world_destroy(w); return NULL;
}
typedef struct {
  BkSpecialWorld *world;
  const BkSpecialEventBindings *bindings;
  const BkSpecialWorldServices *services;
  const float *motion;
  unsigned buttons;
} Frame;
#define FORWARD(member, ...) do { \
  Frame *f = p; const BkSpecialWorldServices *s = f->services; \
  return s->member ? s->member(s->context, __VA_ARGS__) \
                   : fail(e, "missing " #member " service"); \
} while (0)
static int key(void *p, uint32_t code, uint8_t *out, char e[256]) { FORWARD(key, code, out, e); }
static int audio(void *p, const BkSpecialEventAudioCall *call, char e[256]) { FORWARD(audio, call, e); }
static int movie(void *p, char e[256]) { FORWARD(movie, e); }
static int clock_read(void *p, int timer, uint32_t *out, char e[256]) { FORWARD(clock, timer, out, e); }
static int level(void *p, unsigned effect, float seconds, float *out, char e[256]) { FORWARD(level, effect, seconds, out, e); }
static int present(void *p, BkSpecialEventObject object, int *out, char e[256]) {
  Frame *f = p; BkSpecialWorld *w = f->world;
  if (object == BK_SPECIAL_PRIMARY_FACE) { *out = w->face != NULL; return 1; }
  if (object == BK_SPECIAL_CAMERA_PRIMARY || object == BK_SPECIAL_CAMERA_SECONDARY) { *out = 1; return 1; }
  FORWARD(present, object, out, e);
}
static int camera(void *p, BkSpecialEventCamera kind, int32_t clip,
                  const float center[3], float seconds, uint8_t *done, char e[256]) {
  Frame *f = p; BkSpecialWorld *w = f->world;
  /* 4bb82e ignores its explicit third argument, even when paused. */
  if (kind == BK_SPECIAL_CAMERA_OPEN) seconds = *f->bindings->seconds;
  return bk_special_camera_assets_step(w->tracks, w->forest, (uint32_t[2]){0, 1},
      w->camera, w->transitions, &w->presets, kind, clip, center, f->motion,
      f->buttons, w->focus, seconds, done, e);
}
static int timing(void *p, unsigned camera, float *source, float *end, char e[256]) {
  Frame *f = p; BkClipState state; BkClipTiming desc;
  BkActorPose *pose = camera == 0 ? f->world->body : camera == 1
      ? bk_special_camera_assets_pose(f->world->tracks, 1) : NULL;
  if (!pose || !bk_actor_pose_state(pose, &state) ||
      !bk_actor_pose_timing(pose, (unsigned)state.slot, &desc)) return fail(e, "invalid active descriptor");
  *source = desc.source; *end = desc.end; return 1;
}
static int place(void *p, unsigned track, const float v[3], float radians, char e[256]) {
  Frame *f = p; BkSpecialWorld *w = f->world;
  return bk_special_camera_assets_place(w->tracks, w->forest, (uint32_t[2]){0, 1}, track, v, radians, e);
}
static int cue(void *p, int32_t tick, uint8_t *out, char e[256]) {
  Frame *f = p; BkSpecialWorld *w = f->world; float source, end; int fired;
  if (tick < 0 || (size_t)tick >= w->latch_count || !timing(p, 0, &source, &end, e) ||
      !bk_timeline_event(w->latches + tick, source, tick, 0, &fired)) return fail(e, "invalid timeline cue");
  *out = (uint8_t)fired; return 1;
}
static int random_next(void *p, int32_t *out, char e[256]) {
  (void)e; *out = bk_random_next(((Frame *)p)->world->random); return 1;
}
static int request(void *p, int32_t clip, char e[256]) {
  return clip >= 0 ? bk_actor_pose_request(((Frame *)p)->world->body, (unsigned)clip, e)
                   : fail(e, "negative body clip");
}
static int advance(void *p, float seconds, char e[256]) {
  return bk_actor_pose_advance(((Frame *)p)->world->body, -1, seconds, e);
}
static int gaze(void *p, float pitch, float yaw, char e[256]) {
  BkSpecialWorld *w = ((Frame *)p)->world;
  return bk_eye_assets_gaze(w->eyes, w->body, 1, w->camera->pose.world, pitch, yaw, e);
}
static int face(void *p, BkSpecialEventFace kind, int32_t value, float amount,
                uint32_t timestamp, char e[256]) {
  BkSpecialWorld *w = ((Frame *)p)->world;
  if (kind == BK_SPECIAL_FACE_MODE) return bk_eye_assets_select(w->eyes, (uint32_t)value, e);
  if (kind == BK_SPECIAL_FACE_RANGE) return bk_face_eye_range(&w->face_state, 0, amount, w->random, e);
  if (kind == BK_SPECIAL_FACE_EXPRESSION && w->face_state.expression == value)
    return bk_face_request(&w->face_state, value, 0, e);
  uint32_t now;
  if (!clock_read(p, 0, &now, e)) return 0;
  if (kind == BK_SPECIAL_FACE_EXPRESSION) return bk_face_request(&w->face_state, value, now, e);
  BkFaceState next = w->face_state; BkFaceCommands commands; uint32_t rng = *w->random;
  int ok = kind == BK_SPECIAL_FACE_MOUTH ? bk_face_mouth(&next, amount, now, &commands, e)
         : kind == BK_SPECIAL_FACE_BLINK ? bk_face_blink(&next, timestamp, now, &rng, &commands, e)
                                         : fail(e, "unknown face operation");
  if (!ok || !bk_face_assets_apply(w->face, &commands, e)) return 0;
  w->face_state = next; *w->random = rng; return 1;
}
static int hide(void *p, uint32_t value, char e[256]) {
  BkSpecialWorld *w = ((Frame *)p)->world;
  return bk_actor_forest_draw_disable(w->forest,
      bk_actor_forest_node(w->forest, 2, w->back), value, e);
}
int bk_special_world_step(BkSpecialWorld *w, const BkSpecialEventBindings *b,
    const float motion[2], unsigned buttons, const BkSpecialWorldServices *services, char e[256]) {
  if (!w || !b || !motion || !services || (buttons & ~3u) ||
      !isfinite(motion[0]) || !isfinite(motion[1])) return fail(e, "invalid frame services/input");
  Frame frame = {w, b, services, motion, buttons};
  BkSpecialEventOps ops = {&frame, camera, timing, key, present, place, audio,
      cue, movie, clock_read, random_next, level, request, advance, gaze, face, hide};
  return bk_special_event_step(b, &ops, e);
}
int bk_special_world_pass(BkSpecialWorld *w, unsigned group, BkLightingPass *out, char e[256]) {
  if (!w || group >= 5 || !out) return fail(e, "invalid draw group");
  BkDrawDispatch dispatch;
  BkDrawDispatchInput request = {.flow = 0x48, .group = (int32_t)group,
                                  .root_721b28 = w->roots[2]};
  BkLightingPassInput input = {0};
  if (!bk_draw_dispatch_select(&request, &dispatch) ||
      !bk_draw_dispatch_lighting(&dispatch, &input) ||
      !bk_scene_lighting_input(w->lighting, &input) || !bk_lighting_pass(&input, out))
    return fail(e, "invalid lighting dispatch");
  return 1;
}
