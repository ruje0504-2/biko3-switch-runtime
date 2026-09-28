#include "scene/selection_world.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkSelectionWorld {
  BkResourceStore *store;
  BkMenuCamera *camera;
  BkMenuCameraAssets *tracks;
  BkModel *stage_model;
  BkClipSet *stage_clips;
  BkActorPose *stage;
  uint32_t stage_root, roots[BK_SELECTION_OBJECTS], focus;
  BkSelectionActorAssets *body;
  BkActorForest *forest;
  BkSceneLighting *lighting;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "selection world: %s", why);
  return 0;
}
BkActorPose *bk_selection_world_pose(BkSelectionWorld *s, unsigned i) {
  if (!s || i >= BK_SELECTION_OBJECTS)
    return NULL;
  return i < 2                     ? bk_menu_camera_assets_pose(s->tracks, i)
         : i == BK_SELECTION_STAGE ? s->stage
                                   : bk_selection_actor_assets_pose(s->body);
}
BkSelectionActorAssets *bk_selection_world_body(BkSelectionWorld *s) {
  return s ? s->body : NULL;
}
BkActorForest *bk_selection_world_forest(BkSelectionWorld *s) {
  return s ? s->forest : NULL;
}
BkSceneLighting *bk_selection_world_lighting(BkSelectionWorld *s) {
  return s ? s->lighting : NULL;
}
uint32_t bk_selection_world_root(const BkSelectionWorld *s, unsigned i) {
  return s && i < BK_SELECTION_OBJECTS ? s->roots[i] : BK_FRAME_NONE;
}
uint32_t bk_selection_world_focus(const BkSelectionWorld *s) {
  return s ? s->focus : BK_FRAME_NONE;
}
void bk_selection_world_destroy(BkSelectionWorld *s) {
  if (!s)
    return;
  bk_actor_forest_destroy(s->forest);
  bk_scene_lighting_destroy(s->lighting);
  bk_selection_actor_assets_destroy(s->body);
  bk_actor_pose_destroy(s->stage);
  bk_clip_set_destroy(s->stage_clips);
  bk_model_destroy(s->stage_model);
  bk_menu_camera_assets_destroy(s->tracks);
  free(s);
}
static int load_stage(BkSelectionWorld *s, char e[256]) {
  BkBlob raw = {0};
  if (bk_resources_read(s->store, "bk3_03", "m60_00.xan", &raw, e) !=
      BK_RESOURCE_OK)
    return 0;
  s->stage_clips = bk_clip_set_decode(raw.data, raw.size, e);
  bk_blob_free(&raw);
  if (!s->stage_clips ||
      bk_resources_read(s->store, "bk3_03", bk_clip_model_name(s->stage_clips),
                        &raw, e) != BK_RESOURCE_OK)
    return 0;
  BkModelResult result =
      bk_model_decode(raw.data, raw.size, &s->stage_model, e);
  bk_blob_free(&raw);
  if (result != BK_MODEL_OK)
    return 0;
  s->stage_root = BK_MODEL_NONE;
  for (uint32_t i = 0; i < s->stage_model->frame_count; ++i)
    if (s->stage_model->frames[i].parent_index == BK_MODEL_NONE) {
      if (s->stage_root != BK_MODEL_NONE)
        return fail(e, "multiple stage roots");
      s->stage_root = i;
    }
  if (s->stage_root == BK_MODEL_NONE)
    return fail(e, "missing stage root");
  s->stage = bk_actor_pose_create(s->stage_model, s->stage_clips, s->stage_root,
                                  NULL, (float[3]){0}, 0, 0, 1, e);
  return s->stage &&
         bk_actor_pose_root_local(
             s->stage, s->stage_model->frames[s->stage_root].local, e);
}
static BkActorForest *forest(BkSelectionWorld *s, BkSelectionActorAssets *body,
                             const BkMenuCamera *camera, uint32_t roots[4],
                             uint32_t *focus, char e[256]) {
  BkActorPose *poses[4] = {bk_menu_camera_assets_pose(s->tracks, 0),
                           bk_menu_camera_assets_pose(s->tracks, 1), s->stage,
                           bk_selection_actor_assets_pose(body)};
  BkActorForest *f = bk_actor_forest_create(poses, 4, e);
  if (!f)
    return NULL;
  roots[0] = bk_menu_camera_assets_root(s->tracks, 0);
  roots[1] = bk_menu_camera_assets_root(s->tracks, 1);
  roots[2] = s->stage_root;
  roots[3] = bk_selection_actor_assets_root(body);
  if (!bk_actor_forest_anchor(f, 1, camera->pose.world, 0, e))
    goto bad;
  for (unsigned i = 0; i < 4; ++i) {
    roots[i] = bk_actor_forest_node(f, i, roots[i]);
    if (!bk_actor_forest_attach(f, 0, roots[i], e))
      goto bad;
  }
  *focus = bk_actor_forest_node(f, BK_SELECTION_BODY,
                                bk_selection_actor_assets_focus(body));
  return f;
bad:
  bk_actor_forest_destroy(f);
  return NULL;
}
static BkSceneLighting *lighting(BkSelectionActorAssets *body, char e[256]) {
  BkActorPose *pose = bk_selection_actor_assets_pose(body);
  const BkModel *m = bk_actor_pose_model(pose);
  return bk_scene_lighting_create(m, bk_actor_pose_frame(pose, 0),
                                  (size_t)m->frame_count * 16, e);
}
BkSelectionWorld *bk_selection_world_create(BkResourceStore *store,
                                            unsigned group, uint8_t alternate,
                                            float seconds, BkMenuCamera *camera,
                                            const uint32_t clocks[4],
                                            uint32_t *random, char e[256]) {
  if (!store || group >= 5 || alternate > 1 || !camera || !clocks || !random) {
    fail(e, "invalid construction input");
    return NULL;
  }
  BkSelectionWorld *s = calloc(1, sizeof(*s));
  if (!s) {
    fail(e, "allocation failed");
    return NULL;
  }
  s->store = store;
  s->camera = camera;
  BkMenuCamera next = *camera;
  uint32_t rng = *random;
  s->tracks = bk_menu_camera_assets_create(store, group, seconds, &next, e);
  if (!s->tracks || !load_stage(s, e))
    goto bad;
  s->body =
      bk_selection_actor_assets_create(store, 0, alternate, clocks, &rng, e);
  if (!s->body)
    goto bad;
  s->forest = forest(s, s->body, &next, s->roots, &s->focus, e);
  if (!s->forest)
    goto bad;
  s->lighting = lighting(s->body, e);
  if (!s->lighting)
    goto bad;
  *camera = next;
  *random = rng;
  return s;
bad:
  bk_selection_world_destroy(s);
  return NULL;
}
int bk_selection_world_replace(BkSelectionWorld *s, unsigned group,
                               uint8_t alternate, const uint32_t clocks[4],
                               uint32_t *random, BkSelectionActorAssets **old,
                               char e[256]) {
  if (!s || !random || !old || *old)
    return fail(e, "missing replacement owner/output or unretired body");
  uint32_t rng = *random, roots[4], focus;
  BkSelectionActorAssets *body = bk_selection_actor_assets_create(
      s->store, group, alternate, clocks, &rng, e);
  if (!body)
    return 0;
  BkSceneLighting *lights = lighting(body, e);
  if (!lights) {
    bk_selection_actor_assets_destroy(body);
    return 0;
  }
  BkActorForest *f = forest(s, body, s->camera, roots, &focus, e);
  if (!f) {
    bk_scene_lighting_destroy(lights);
    bk_selection_actor_assets_destroy(body);
    return 0;
  }
  bk_actor_forest_destroy(s->forest);
  bk_scene_lighting_destroy(s->lighting);
  *old = s->body;
  s->forest = f;
  s->lighting = lights;
  s->body = body;
  memcpy(s->roots, roots, sizeof(roots));
  s->focus = focus;
  *random = rng;
  return 1;
}
int bk_selection_world_step(BkSelectionWorld *s,
                            const BkSelectionWorldInput *in,
                            const BkSelectionWorldOps *ops, uint32_t *random,
                            char e[256]) {
  if (!s || !in || !random || in->selected >= 5 || in->camera_mode > 1 ||
      in->voice_active > 1 || (in->buttons & ~3u) || !isfinite(in->seconds) ||
      in->seconds < 0 || (double)in->seconds * 60 >= INT32_MAX ||
      !isfinite(in->motion[0]) || !isfinite(in->motion[1]))
    return fail(e, "invalid retail frame");
  int movie = bk_selection_actor_assets_needs_movie(s->body);
  if ((movie && (!ops || !ops->movie_step)) ||
      (in->voice_active && (!ops || !ops->voice_level)))
    return fail(e, "required movie/voice service missing");
  if (!bk_menu_camera_assets_step(s->tracks, s->forest, (uint32_t[2]){0, 1},
                                  s->camera, in->camera_mode, in->motion,
                                  in->buttons, s->focus, in->seconds, e))
    return 0;
  if (movie && !ops->movie_step(ops->context, e))
    return 0;
  float mouth = 0;
  if (in->voice_active &&
      !ops->voice_level(ops->context, in->seconds, &mouth, e))
    return 0;
  return bk_selection_actor_assets_step(
             s->body, in->selected, in->seconds, mouth, s->camera->pose.world,
             in->timestamp, in->face_clocks, random, e) &&
         bk_actor_pose_advance(s->stage, 0, in->seconds, e);
}
int bk_selection_world_pass(BkSelectionWorld *s, BkLightingPass *out,
                            char e[256]) {
  if (!s || !out)
    return fail(e, "missing draw owner/output");
  BkLightingPassInput in = {.mode = 1};
  in.objects[0] = s->roots[BK_SELECTION_STAGE];
  in.objects[4] = s->roots[BK_SELECTION_BODY];
  if (!bk_scene_lighting_input(s->lighting, &in) || !bk_lighting_pass(&in, out))
    return fail(e, "invalid lighting dispatch");
  return 1;
}
