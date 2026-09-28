#include "scene/ending_normal_session.h"
#include "game/draw_dispatch.h"
#include "model/material_pose.h"
#include "scene/ending_audio.h"
#include "scene/ending_normal_assets.h"
#include "scene/ending_normal_render.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  BkSceneServices services;
  BkEndingState state;
  BkFadeSprite overlay;
  BkMenuCamera camera;
  BkEndingCameraPresets presets;
  BkEndingNormalAssets *assets;
  BkEndingNormalRender *render;
  BkEndingAudio *audio;
  BkMaterialPose *materials;
  int32_t disabled[64];
  size_t disabled_count;
  uint8_t option_a, option_b;
  int32_t selected_group;
  uint32_t random, now_ms;
  unsigned group, variant;
  BkEndingRecords *records;
  int pending, valid, drawn, failed;
} EndingNormalScene;

static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending normal scene: %s", why);
  return 0;
}

static int warp(void *context, float x, float y, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !isfinite(x) || !isfinite(y) ||
      !bk_menu_camera_dialogue(&s->camera))
    return fail(e, "window warp/camera reset failed");
  return 1;
}

static void release_resources(EndingNormalScene *s) {
  if (!s)
    return;
  char ignored[256];
  if (s->audio)
    bk_ending_audio_stop(s->audio, ignored);
  bk_ending_audio_destroy(s->audio);
  s->audio = NULL;
  bk_ending_normal_render_destroy(s->render);
  s->render = NULL;
  bk_material_pose_destroy(s->materials);
  s->materials = NULL;
  bk_ending_normal_assets_destroy(s->assets);
  s->assets = NULL;
  s->disabled_count = 0;
}

static int normal_load(void *context, BkEndingLoader loader, int32_t argument,
                       char e[256]) {
  EndingNormalScene *s = context;
  unsigned variant;
  if (!s || argument != -1)
    return fail(e, "invalid normal loader argument");
  /* 4d00fa is the second normal gallery profile. The remaining loaders have
   * different scene topologies and must reach their own owners later. */
  if (loader == BK_ENDING_LOAD_4CF318)
    variant = 0;
  else if (loader == BK_ENDING_LOAD_4D00FA)
    variant = 1;
  else
    return fail(e, "normal scene does not own this ending loader");
  release_resources(s);
  uint32_t clocks[4] = {s->now_ms, s->now_ms, s->now_ms, s->now_ms};
  s->assets = bk_ending_normal_assets_create(
      s->services.resources, s->state.frame.group, variant, clocks, &s->random,
      &s->camera, &s->presets, e);
  if (!s->assets ||
      !bk_ending_normal_assets_load_background(s->assets, s->services.resources,
                                               e))
    goto bad;
  s->render = bk_ending_normal_render_create(
      s->services.renderer, s->services.resources, s->assets, (int32_t)s->now_ms,
      e);
  if (!s->render)
    goto bad;
  s->materials = bk_material_pose_create(
      bk_actor_pose_model(bk_ending_normal_assets_pose(s->assets, 0)), e);
  if (!s->materials)
    goto bad;
  s->disabled_count = bk_bom_assets_count(bk_ending_normal_assets_bom(s->assets));
  if (s->disabled_count > sizeof(s->disabled) / sizeof(s->disabled[0])) {
    fail(e, "normal disabled-model table is too large");
    goto bad;
  }
  memset(s->disabled, 0, sizeof(s->disabled));
  s->audio = bk_ending_audio_create_entry(
      s->services.resources, s->services.audio, 0, s->state.frame.group, variant,
      -900, e);
  if (!s->audio)
    goto bad;
  s->variant = variant;
  return 1;
bad:
  release_resources(s);
  return 0;
}

static int clear_record(void *context, unsigned group, char e[256]) {
  EndingNormalScene *s = context;
  if (!s || !s->records)
    return fail(e, "story record owner is missing");
  return bk_ending_record_clear(s->records, group, e);
}

static int prepare_frame(EndingNormalScene *s, float seconds, char e[256]) {
  BkActorPose *primary, *camera_track;
  BkActorForest *forest;
  BkDrawDispatchInput in;
  BkDrawDispatch dispatch;
  BkFog fog = {0};
  uint32_t roots[3] = {BK_MODEL_NONE, BK_MODEL_NONE, BK_MODEL_NONE};
  if (!s || !s->assets || !s->render || !isfinite(seconds) || seconds <= 0 ||
      seconds > 1)
    return fail(e, "invalid live normal scene");
  uint32_t step_ms = (uint32_t)((double)seconds * 1000.0);
  s->now_ms += step_ms ? step_ms : 1;
  primary = bk_ending_normal_assets_pose(s->assets, 0);
  camera_track = bk_ending_normal_assets_pose(s->assets, 2);
  if (!primary || !camera_track ||
      !bk_actor_pose_advance(primary, -1, seconds, e) ||
      !bk_actor_pose_advance(camera_track, -1, seconds, e) ||
      !bk_bom_assets_advance(bk_ending_normal_assets_bom(s->assets), seconds,
                             e) ||
      !bk_bom_assets_follow_references(bk_ending_normal_assets_bom(s->assets),
                                        e) ||
      !bk_face_assets_step(
          bk_ending_normal_assets_face(s->assets),
          bk_ending_normal_assets_face_state(s->assets),
          bk_ending_normal_assets_config(s->assets)->expression_a, 0,
          s->now_ms, s->now_ms, s->now_ms, s->now_ms, &s->random, e))
    return 0;
  forest = bk_ending_normal_assets_forest(s->assets);
  if (!bk_actor_forest_refresh(forest, e))
    return 0;
  for (unsigned actor = 0; actor < 3; ++actor) {
    unsigned source = actor == 2 ? 4 : actor;
    BkActorPose *pose = bk_ending_normal_assets_pose(s->assets, source);
    const BkModel *model;
    if (!pose)
      return fail(e, "normal actor disappeared");
    model = bk_actor_pose_model(pose);
    for (uint32_t frame = 0; frame < model->frame_count; ++frame)
      if (model->frames[frame].parent_index == BK_MODEL_NONE) {
        roots[actor] = bk_actor_forest_node(forest, source, frame);
        break;
      }
    if (roots[actor] == BK_MODEL_NONE)
      return fail(e, "normal actor root is missing");
  }
  if (!bk_ending_normal_render_movie_step(s->render, (int32_t)s->now_ms,
                                          (int32_t)s->now_ms + 1, e))
    return 0;
  in = (BkDrawDispatchInput){.flow = 16,
                             .group = (int32_t)s->state.frame.group,
                             .event_state = 1,
                             .root_721b28 = roots[0],
                             .root_721b2c = bk_actor_forest_node(forest, 1, 0),
                             .root_721b34 = roots[2]};
  /* Resolve the auxiliary model root from its live forest binding rather than
   * guessing frame zero; roots[1] is the only source used by slot 5. */
  roots[1] = BK_MODEL_NONE;
  {
    const BkModel *model =
        bk_actor_pose_model(bk_ending_normal_assets_pose(s->assets, 1));
    for (uint32_t frame = 0; frame < model->frame_count; ++frame)
      if (model->frames[frame].parent_index == BK_MODEL_NONE) {
        roots[1] = bk_actor_forest_node(forest, 1, frame);
        break;
      }
  }
  if (roots[1] == BK_MODEL_NONE)
    return fail(e, "normal auxiliary root is missing");
  in.root_721b2c = roots[1];
  if (!bk_draw_dispatch_select(&in, &dispatch) ||
      !bk_ending_normal_render_prepare_regular(
          s->render, &dispatch, &s->camera, &fog, s->materials, s->disabled,
          s->disabled_count, e))
    return 0;
  s->pending = 1;
  s->valid = 1;
  s->drawn = 0;
  return 1;
}

static int step(void *context, double seconds, const BkInput *input,
                char e[256]) {
  EndingNormalScene *s = context;
  (void)input;
  if (!s || s->pending || s->failed)
    return fail(e, "previous ending snapshot not presented");
  s->failed = 1;
  if (!prepare_frame(s, (float)seconds, e))
    return 0;
  s->failed = 0;
  return 1;
}

static int draw(void *context, const BkSceneFrame *frame, char e[256]) {
  EndingNormalScene *s = context;
  (void)frame;
  if (!s || (!s->pending && !s->valid) || !s->render)
    return fail(e, "no ending snapshot");
  if (!bk_ending_normal_render_draw(s->render, e))
    return 0;
  s->drawn = 1;
  return 1;
}

static void destroy(void *context) {
  EndingNormalScene *s = context;
  if (!s)
    return;
  release_resources(s);
  free(s);
}

static BkScene *create_entry(const BkSceneServices *services, unsigned group,
                             unsigned variant, int8_t previous,
                             uint32_t selected, BkEndingRecords *records,
                             char e[256]) {
  if (!services || !services->renderer || !services->resources ||
      !services->audio || group >= 5 || variant > 1)
    return NULL;
  EndingNormalScene *s = calloc(1, sizeof(*s));
  if (!s)
    return NULL;
  s->services = *services;
  s->group = group;
  s->variant = variant;
  s->records = records;
  s->random = UINT32_C(0x4cc582) ^ (group * UINT32_C(0x9e3779b9));
  s->overlay = (BkFadeSprite){0, 2, 0};
  BkEndingStateOps state_ops = {s, warp};
  if (!bk_ending_state_begin(&s->state, &s->overlay, group, variant, .5f,
                             (int32_t[2]){0, 0}, &state_ops, e))
    goto bad;
  BkEndingEntryBindings bindings;
  if (!bk_ending_state_entry_bindings(&s->state, &s->option_a, &s->option_b,
                                      &s->selected_group, &bindings))
    goto bad;
  BkEndingEntryOps ops = {s, normal_load, records ? clear_record : NULL, NULL};
  /* Gallery selection 0/1 enters the two normal loader profiles without
   * pretending that story record persistence or other loaders exist. */
  if (!bk_ending_entry_dispatch(&bindings, previous, selected, .5f, &ops, e))
    goto bad;
  if (!s->assets || !s->render || !s->audio)
    goto bad;
  /* The application may present a first frame before its fixed-step clock
   * emits a tick. Build that snapshot here so the scene follows the same
   * load->present contract as the other scene owners. */
  if (!prepare_frame(s, 1.0f / 60.0f, e))
    goto bad;
  BkScene *scene = bk_scene_custom_create(
      s, (BkSceneCustomOps){step, draw, destroy}, e);
  if (!scene)
    goto bad;
  return scene;
bad:
  destroy(s);
  return NULL;
}

BkScene *bk_ending_normal_scene_create(const BkSceneServices *services,
                                       unsigned group, unsigned variant,
                                       char e[256]) {
  return create_entry(services, group, variant, 0x18, variant, NULL, e);
}

BkScene *bk_ending_normal_scene_create_story(const BkSceneServices *services,
                                             unsigned group, unsigned variant,
                                             BkEndingRecords *records,
                                             char e[256]) {
  return create_entry(services, group, variant, 8, variant, records, e);
}

int bk_ending_normal_scene_after_present(BkScene *scene, char e[256]) {
  EndingNormalScene *s = bk_scene_custom_context(scene);
  if (!s || !s->valid || !s->drawn)
    return fail(e, "ending snapshot was not presented");
  s->pending = 0;
  s->drawn = 0;
  return 1;
}

const BkEndingState *bk_ending_normal_scene_state(const BkScene *scene) {
  EndingNormalScene *s =
      bk_scene_custom_context((BkScene *)(uintptr_t)scene);
  return s ? &s->state : NULL;
}
