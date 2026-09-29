#include "scene/ending_normal_controller.h"
#include "scene/ending_target.h"
#include <stdio.h>
#include <string.h>
typedef struct {
  const BkEndingNormalControllerScene *scene;
  BkEndingNormalControlBindings bindings;
  BkEndingNormalControlOps ops;
  float seconds;
} Context;
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "normal controller scene: %s", why);
  return 0;
}
void bk_ending_normal_controller_initialize(BkEndingNormalControllerRetained *s) {
  if (!s)
    return;
  memset(s, 0, sizeof(*s));
  s->control = bk_ending_normal_control_initial();
  s->opening = (BkEndingOpeningRetained){1};
  bk_bom_return_init(&s->recoil[0]);
  bk_bom_return_init(&s->recoil[1]);
}
static int key(void *p, unsigned code, unsigned mode, uint32_t *out,
                 char e[256]) {
  const BkEndingNormalControllerScene *s = ((Context *)p)->scene;
  return s->key ? s->key(s->input_context, code, mode, out, e)
                 : fail(e, "missing physical key input");
}
static int raw_key(void *p, unsigned code, uint32_t *out, char e[256]) {
  const BkEndingNormalControllerScene *s = ((Context *)p)->scene;
  return s->raw_key ? s->raw_key(s->input_context, code, out, e)
                     : fail(e, "missing physical raw-key input");
}
static int warp(void *p, float x, float y, char e[256]) {
  const BkEndingNormalControllerScene *s = ((Context *)p)->scene;
  return s->warp ? s->warp(s->input_context, x, y, e)
                  : fail(e, "missing virtual pointer owner");
}
static int target(void *p, const float point[2], int32_t *result, char e[256]) {
  const BkEndingNormalControllerScene *s = ((Context *)p)->scene;
  BkEndingState *v = s->state;
  BkClipState clip;
  BkNodeReference camera;
  if (!s->geometry || !s->ring ||
      !bk_actor_pose_state(bk_ending_normal_assets_pose(s->assets, 0), &clip) ||
      !bk_actor_forest_anchor_reference(bk_ending_normal_assets_forest(s->assets),
                                        1, &camera, e))
    return fail(e, "missing held target geometry/active clip/camera local");
  BkEndingTargetBindings b = {
      &v->frame, &v->control, &v->auxiliary, &clip.slot,
      bk_ending_normal_assets_config(s->assets)->actions, v->targets,
      &s->retained->control.action_kind, &s->retained->control.action_column,
      s->ring, camera.local, s->geometry};
  int selected;
  if (!bk_ending_target_step(&b, point, &selected, e))
    return 0;
  *result = selected;
  return 1;
}
static int present(void *p, unsigned slot, int *out, char e[256]) {
  return bk_ending_audio_present(((Context *)p)->scene->audio, slot, out)
      ? 1 : fail(e, "invalid real audio presence query");
}
static int audio(Context *c, const BkEndingAudioCall *call, int *out,
                   char e[256]) {
  const BkEndingState *v = c->scene->state;
  return bk_ending_audio_call(c->scene->audio, v->frame.group,
                               v->auxiliary.variant, v->auxiliary.selection,
                               call, out, e);
}
static int status(void *p, unsigned slot, int *out, char e[256]) {
  return audio(p, &(BkEndingAudioCall){.operation = BK_ENDING_AUDIO_STATUS,
                                      .slot = slot}, out, e);
}
static int pause_audio(void *p, unsigned slot, char e[256]) {
  int ignored;
  return audio(p, &(BkEndingAudioCall){.operation = BK_ENDING_AUDIO_PAUSE,
                                      .slot = slot}, &ignored, e);
}
static int load(void *p, unsigned slot, const char *name, char e[256]) {
  return bk_ending_audio_load_speech(((Context *)p)->scene->audio, slot, name, e);
}
static int play(void *p, unsigned slot, int32_t flags, int32_t volume,
                  char e[256]) {
  int ignored;
  return audio(p, &(BkEndingAudioCall){.operation = BK_ENDING_AUDIO_RESTART,
      .slot = slot, .flags = flags, .volume = volume}, &ignored, e);
}
static int name(void *p, BkEndingNormalVoice kind, char out[32], char e[256]) {
  BkEndingState *v = ((Context *)p)->scene->state;
  int ok;
  if (kind == BK_ENDING_NORMAL_LOOP_NAME)
    ok = bk_ending_sound_loop_name(v->frame.group, v->frame.phase,
                                    v->auxiliary.progress, out, e);
  else if (kind == BK_ENDING_NORMAL_ACTION_NAME)
    ok = bk_ending_sound_action_name(v->frame.group, v->frame.camera_cached,
                                      v->auxiliary.progress, out, e);
  else
    return fail(e, "unknown original filename builder");
  if (ok)
    memcpy(v->speech_names[0], out, strlen(out) + 1);
  return ok;
}
static int eyes(void *p, unsigned slot, char e[256]) {
  return bk_eye_assets_select(bk_ending_normal_assets_eyes(
      ((Context *)p)->scene->assets), slot, e);
}
static int request(void *p, unsigned actor, int32_t clip, char e[256]) {
  BkActorPose *pose = actor < 2 ? bk_ending_normal_assets_pose(
      ((Context *)p)->scene->assets, actor) : NULL;
  return pose && clip >= 0
      ? bk_actor_pose_request_mode(pose, (unsigned)clip,
                                    BK_CLIP_REQUEST_CONFIGURED, e)
      : fail(e, "invalid configured actor/clip request");
}
static int actor_present(void *p, unsigned actor, int *out, char e[256]) {
  if (!out || actor >= 2)
    return fail(e, "invalid actor presence query");
  *out = bk_ending_normal_assets_pose(((Context *)p)->scene->assets, actor) != NULL;
  return 1;
}
static int root_flag(void *p, unsigned actor, int32_t hidden, char e[256]) {
  BkEndingNormalAssets *assets = ((Context *)p)->scene->assets;
  uint32_t root = actor < 2 ? bk_ending_normal_assets_root(assets, actor)
                            : BK_FRAME_NONE;
  return root != BK_FRAME_NONE
      ? bk_actor_forest_visibility(bk_ending_normal_assets_forest(assets),
                                     root, (uint32_t)hidden, e)
      : fail(e, "missing required model root");
}
static int write(void *p, int32_t clip, BkEndingClipWrite field, int32_t value,
                   char e[256]) {
  if (clip < 0 || (field != BK_ENDING_CLIP_CHAIN && field != BK_ENDING_CLIP_NEXT))
    return fail(e, "invalid manual descriptor write");
  BkClipEdit edit = {.slot = (unsigned)clip,
      .fields = field == BK_ENDING_CLIP_CHAIN ? BK_CLIP_EDIT_CHAIN : BK_CLIP_EDIT_NEXT,
      .chain = value, .next = value};
  return bk_actor_pose_edit_clips(bk_ending_normal_assets_pose(
      ((Context *)p)->scene->assets, 0), &edit, 1, e);
}
static int manual(void *p, char e[256]) {
  Context *c = p;
  BkEndingNormalControllerRetained *r = c->scene->retained;
  BkEndingNormalManualOps ops = {c->ops, write};
  return bk_ending_normal_manual_step(&r->manual_clip, &r->control,
                                       &c->bindings, &ops, e);
}
static int project(void *p, unsigned index, int32_t point[2], char e[256]) {
  return bk_ending_ui_project_target(((Context *)p)->scene->geometry, index,
                                      point, e);
}
static int timing(void *p, int32_t clip, BkEndingNormalActionTiming *out,
                    char e[256]) {
  BkClipTiming t;
  if (clip < 0 || !bk_actor_pose_timing(bk_ending_normal_assets_pose(
          ((Context *)p)->scene->assets, 0), (unsigned)clip, &t))
    return fail(e, "invalid actual animation descriptor");
  *out = (BkEndingNormalActionTiming){t.source, t.end};
  return 1;
}
static int source(void *p, int32_t clip, float value, char e[256]) {
  if (clip < 0)
    return fail(e, "negative source descriptor");
  BkClipEdit edit = {.slot = (unsigned)clip, .fields = BK_CLIP_EDIT_SOURCE,
                      .source = value};
  return bk_actor_pose_edit_clips(bk_ending_normal_assets_pose(
      ((Context *)p)->scene->assets, 0), &edit, 1, e);
}
static int instant(void *p, unsigned actor, int32_t clip, char e[256]) {
  BkActorPose *pose = actor < 2 ? bk_ending_normal_assets_pose(
      ((Context *)p)->scene->assets, actor) : NULL;
  return pose && clip >= 0 ? bk_actor_pose_select(pose, (unsigned)clip, 1, e)
                           : fail(e, "invalid instant actor/clip request");
}
static int find(void *p, const char *name, uint32_t *out, char e[256]) {
  BkEndingNormalAssets *assets = ((Context *)p)->scene->assets;
  uint32_t found, root = bk_ending_normal_assets_root(assets, 0);
  if (root == BK_FRAME_NONE || !bk_actor_forest_find(
          bk_ending_normal_assets_forest(assets), root, name, &found, e))
    return fail(e, "actual primary lookup failed");
  *out = found == BK_FRAME_NONE ? 0 : found;
  return 1;
}
static int recoil(void *p, unsigned kind, float degrees, int32_t flip,
                    uint32_t ms, int32_t reset, int *out, char e[256]) {
  const BkEndingNormalControllerScene *s = ((Context *)p)->scene;
  if (kind > 1 || !s->scene_world)
    return fail(e, "missing actual recoil coordinate frame");
  BkBomAssets *bom = bk_ending_normal_assets_bom(s->assets);
  return kind ? bk_bom_assets_return_multiple(bom, &s->retained->recoil[1], 2,
                          s->scene_world, degrees, flip, ms, out, e)
               : bk_bom_assets_return_single(bom, &s->retained->recoil[0], 0,
                          s->scene_world, degrees, flip, ms, reset, out, e);
}
static int action(void *p, int32_t clip, const BkEndingFrameInput *in,
                    char e[256]) {
  Context *c = p;
  const BkEndingNormalControllerScene *s = c->scene;
  BkEndingState *v = s->state;
  BkEndingNormalActionBindings b = {
      c->bindings, &v->contact_index, &v->normal_side, v->targets,
      &v->retained.normal.direct_reference, &v->retained.normal.direct_node,
      s->flip, s->effect_volume};
  BkEndingNormalActionOps ops = {c->ops, project, timing, source, instant,
                                 find, warp, recoil};
  uint32_t ignored;
  return bk_ending_normal_action_step(&s->retained->action, &b, clip, in,
                                       c->seconds, &ops, &ignored, e);
}
static int opening(void *p, float seconds, char e[256]) {
  Context *c = p;
  const BkEndingNormalControllerScene *s = c->scene;
  BkEndingState *v = s->state;
  BkEndingOpeningBindings b = {
      &v->frame, &v->control, &v->auxiliary, s->camera,
      &v->retained.normal.word_719b50, &v->retained.normal.word_719b24,
      &v->next_mode, s->previous_flow, s->voice_volume, v->speech_names[0],
      &s->retained->opening};
  BkEndingOpeningScene scene = {s->assets, s->audio, s->transitions,
      s->presets, &v->selected, {c, key}};
  return bk_ending_opening_scene_step(&scene, &b, seconds, e);
}
int bk_ending_normal_controller_scene_step(const BkEndingNormalControllerScene *s,
                                           const BkEndingFrameInput *in,
                                           float seconds, char e[256]) {
  if (!s || !s->assets || !s->audio || !s->state || !s->retained ||
      !s->camera || !s->transitions || !s->presets || !s->previous_flow ||
      !s->voice_volume || !s->random)
    return fail(e, "missing scene or process controller owners");
  BkEndingState *v = s->state;
  Context c = {.scene = s, .seconds = seconds};
  c.bindings = (BkEndingNormalControlBindings){
      &v->frame, &v->control, &v->auxiliary, &v->retained.normal.word_719b50,
      &v->retained.normal.word_719b24, v->retained.normal.words_719b54,
      &v->normal_ready, &v->normal_target, v->normal_inputs, v->normal_processed,
      &v->next_mode, &v->open, s->voice_volume, s->previous_flow,
      bk_ending_normal_assets_config(s->assets)->actions,
      v->speech_names[0], s->random, s->records};
  c.ops = (BkEndingNormalControlOps){&c, key, raw_key, target, present, status,
      pause_audio, load, play, name, eyes, request, actor_present, root_flag,
      manual, action, opening};
  return bk_ending_normal_control_step(&s->retained->control, &c.bindings,
                                        in, seconds, &c.ops, e);
}
