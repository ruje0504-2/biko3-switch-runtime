#include "scene/ending_selected_assets.h"
#include "game/ending_normal.h"
#include "game/ending_selected_motion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef BkEndingBackgroundData SelectedActor;
struct BkEndingSelectedAssets {
  BkEndingSelectedConfig config;
  SelectedActor primary, background;
  BkEndingBackgroundAssets *background_owner;
  BkActorForest *forest;
  BkEndingCameraAssets *cameras;
  BkFaceAssets *face;
  BkEyeAssets *eyes;
  BkFaceState face_state;
  const char *background_name;
  unsigned background_state, background_first, replaced_background, missing_visible;
  uint32_t nodes[BK_ENDING_NORMAL_NODES], visible[3], anchor, follow, special;
  float targets[3][3];
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "selected ending assets: %s", why);
  return 0;
}
static void destroy_actor(SelectedActor *p) {
  bk_morph_group_destroy(p->morph);
  bk_material_animation_destroy(p->animation);
  bk_material_pose_destroy(p->materials);
  bk_actor_pose_destroy(p->pose);
  bk_clip_set_destroy(p->clips);
  bk_model_destroy(p->model);
}
void bk_ending_selected_assets_destroy(BkEndingSelectedAssets *a) {
  if (!a) return;
  bk_actor_forest_destroy(a->forest);
  bk_ending_camera_assets_destroy(a->cameras);
  bk_eye_assets_destroy(a->eyes);
  bk_face_assets_destroy(a->face);
  destroy_actor(&a->primary);
  bk_ending_background_assets_destroy(a->background_owner);
  free(a);
}
static int load_actor(SelectedActor *p, BkResourceStore *store,
                       const char *pack, const char *name, char e[256]) {
  BkBlob blob = {0};
  int ok = 0;
  if (bk_resources_read(store, pack, name, &blob, e) != BK_RESOURCE_OK) goto done;
  p->clips = bk_clip_set_decode(blob.data, blob.size, e);
  bk_blob_free(&blob);
  if (!p->clips ||
      bk_resources_read(store, pack, bk_clip_model_name(p->clips), &blob, e) != BK_RESOURCE_OK ||
      bk_model_decode(blob.data, blob.size, &p->model, e) != BK_MODEL_OK)
    goto done;
  if (bk_model_chunk(p->model, "LIGA") || bk_model_chunk(p->model, "TEXA")) {
    fail(e, "animated resource effect needs its own owner");
    goto done;
  }
  p->materials = bk_material_pose_create(p->model, e);
  if (!p->materials ||
      (bk_model_chunk(p->model, "MATA") && !(p->animation = bk_material_animation_create(p->model, e))) ||
      (bk_model_chunk(p->model, "MORP") && !(p->morph = bk_morph_group_create(p->model, e))))
    goto done;
  p->root = BK_MODEL_NONE;
  for (uint32_t i = 0; i < p->model->frame_count; ++i)
    if (p->model->frames[i].parent_index == BK_MODEL_NONE) {
      if (p->root != BK_MODEL_NONE) {
        fail(e, "multiple actor roots");
        goto done;
      }
      p->root = i;
    }
  if (p->root == BK_MODEL_NONE) {
    fail(e, "missing actor root");
    goto done;
  }
  p->pose = bk_actor_pose_create_loaded(p->model, p->clips, p->root, (float[3]){0}, 0, e);
  if (!p->pose || !bk_actor_pose_root_local(p->pose, p->model->frames[p->root].local, e))
    goto done;
  ok = 1;
done:
  bk_blob_free(&blob);
  return ok;
}
static void snapshot(const SelectedActor *p, BkNodeReference *node) {
  memcpy(node->local, bk_actor_pose_local(p->pose, p->root), 64);
  memcpy(node->world, bk_actor_pose_frame(p->pose, p->root), 64);
  memcpy(node->parent_world, bk_actor_pose_parent_world(p->pose, p->root), 64);
}
static int attach_actor(BkEndingSelectedAssets *a, SelectedActor *p,
                         unsigned actor, char e[256]) {
  if (!bk_actor_forest_attach(a->forest, 0,
          bk_actor_forest_node(a->forest, actor, p->root), e)) return 0;
  BkNodeReference root;
  snapshot(p, &root);
  return bk_node_reference_orientation(&root, bk_actor_forest_world(a->forest, 0),
             (float[3]){0, 0, 1}, (float[3]){0, 1, 0}, e) &&
         bk_actor_pose_root_local(p->pose, root.local, e);
}
static const SelectedActor *effect_actor(const BkEndingSelectedAssets *a, unsigned actor) {
  if (!a || a->background_state == 2 || (actor != 0 && actor != 3)) return NULL;
  const SelectedActor *p = actor == 0 ? &a->primary : &a->background;
  return p->pose ? p : NULL;
}
int bk_ending_selected_assets_advance(BkEndingSelectedAssets *a, unsigned actor,
                                      float seconds, char e[256]) {
  const SelectedActor *p = effect_actor(a, actor);
  if (!p) return fail(e, "missing primary/background effect owner");
  BkPlaybackEffects effects;
  int submitted;
  if (!bk_actor_pose_advance_effects(p->pose, seconds, &effects, &submitted, e)) return 0;
  if (!submitted) return 1;
  if (effects.pose.blend && p->morph &&
      !bk_morph_group_blend(p->morph, effects.pose.from, effects.pose.to,
                              effects.pose.weight, NULL, 0, e)) return 0;
  if (p->animation && !bk_material_animation_sample(
          p->animation, effects.source, p->materials, e)) return 0;
  return effects.pose.blend || !p->morph ||
         bk_morph_group_sample(p->morph, effects.source, NULL, 0, e);
}
int bk_ending_selected_assets_advance_plain(BkEndingSelectedAssets *a,
                                             float seconds, BkClipPlainMode mode,
                                             char e[256]) {
  const SelectedActor *p = effect_actor(a, 0);
  if (!p) return fail(e, "missing primary controlled effect owner");
  BkPlaybackEffects effects;
  int submitted;
  if (!bk_actor_pose_advance_plain(p->pose, seconds, mode,
                                    &effects, &submitted, e)) return 0;
  if (!submitted) return 1;
  if (p->animation && !bk_material_animation_sample(
          p->animation, effects.source, p->materials, e)) return 0;
  return !p->morph || bk_morph_group_sample(p->morph, effects.source, NULL, 0, e);
}
static int motion_clip(const SelectedActor *p, BkClipState *state,
                         BkEndingSelectedMotionClip *out, char e[256]) {
  BkClipTiming timing;
  BkClipPrediction prediction;
  if (!p || !bk_actor_pose_state(p->pose, state) || state->slot < 0 ||
      !bk_actor_pose_timing(p->pose, (unsigned)state->slot, &timing) ||
      !bk_actor_pose_prediction(p->pose, (unsigned)state->slot, &prediction))
    return fail(e, "missing primary motion descriptor");
  *out = (BkEndingSelectedMotionClip){prediction.duration,
      timing.start, timing.end, timing.source, prediction.rate, state->elapsed};
  return 1;
}
static int motion_commit(const SelectedActor *p, const BkClipState *state,
                           const BkEndingSelectedMotionClip *clip, char e[256]) {
  BkClipEdit edit = {.slot = (unsigned)state->slot,
      .fields = BK_CLIP_EDIT_SOURCE, .source = clip->source};
  return bk_actor_pose_edit_clips(p->pose, &edit, 1, e);
}
int bk_ending_selected_assets_pointer(BkEndingSelectedAssets *a,
                                       const int32_t target[2],
                                       const int32_t menu[2],
                                       const int32_t pointer[2], char e[256]) {
  const SelectedActor *p = effect_actor(a, 0);
  BkClipState state;
  BkEndingSelectedMotionClip clip;
  return motion_clip(p, &state, &clip, e) &&
      bk_ending_selected_motion_pointer(&clip, target, menu, pointer, e) &&
      motion_commit(p, &state, &clip, e);
}
int bk_ending_selected_assets_drag(BkEndingSelectedAssets *a,
                                    int32_t plain_scheduled, int32_t reverse,
                                    const float motion[2], char e[256]) {
  const SelectedActor *p = effect_actor(a, 0);
  BkClipState state;
  BkEndingSelectedMotionClip clip;
  return motion_clip(p, &state, &clip, e) &&
      bk_ending_selected_motion_drag(&clip, plain_scheduled, reverse, motion, e) &&
      motion_commit(p, &state, &clip, e);
}
int bk_ending_selected_assets_material_alpha(BkEndingSelectedAssets *a,
                                              const char *name, uint32_t hidden,
                                              float alpha, char e[256]) {
  if (!a || a->background_state == 2 || !name) return fail(e, "invalid material owner/name");
  if (!*name) return 1;
  const unsigned fresh[] = {0, 3}, retained[] = {3, 0};
  const unsigned *order = a->background_first ? retained : fresh;
  for (unsigned owner = 0; owner < 2; ++owner) {
    const SelectedActor *p = effect_actor(a, order[owner]);
    if (!p) continue;
    for (uint32_t i = 0; i < p->model->material_count; ++i) {
      if (strcmp(p->model->materials[i].name, name)) continue;
      const BkModelMaterial *m = bk_material_pose_material(p->materials, i);
      if (!m || m->id != p->model->materials[i].id || (!hidden && !isfinite(alpha)))
        return fail(e, "invalid material identity/alpha");
      BkMaterialValuesEdit edit = {.index = i, .id = m->id};
      memcpy(edit.values.diffuse, m->diffuse, sizeof(edit.values.diffuse));
      memcpy(edit.values.ambient, m->ambient, sizeof(edit.values.ambient));
      memcpy(edit.values.specular, m->specular, sizeof(edit.values.specular));
      memcpy(edit.values.emissive, m->emissive, sizeof(edit.values.emissive));
      edit.values.power = m->power;
      edit.values.diffuse[3] = hidden ? 0 : alpha;
      return bk_material_pose_values(p->materials, &edit, 1, e);
    }
  }
  return 1;
}
BkEndingSelectedAssets *bk_ending_selected_assets_create(
    BkResourceStore *store, const BkEndingSelectedLoad *load,
    const uint32_t clocks[4], uint32_t *random, BkMenuCamera *camera,
    BkEndingCameraPresets *presets, char e[256]) {
  BkEndingSelectedConfig config;
  if (!store || !load || !clocks || !random || !camera || !presets ||
      !load->primary_path || strlen(load->primary_path) >= 260 ||
      !bk_ending_selected_config(&config, load->group, load->variant, load->selection)) {
    fail(e, "invalid construction input");
    return NULL;
  }
  const char *name = load->primary_path;
  if (*name == '\\') ++name;
  if (!*name || strchr(name, '\\') || strchr(name, '/')) {
    fail(e, "primary path must name an actual entry inside the selected pack");
    return NULL;
  }
  if (load->background) {
    const char *actual = bk_ending_background_assets_name(load->background);
    const char *first = bk_ending_normal_background(load->group, 0);
    const char *second = bk_ending_normal_background(load->group, 1);
    /*4CF318's group1 stage owns m02_92, outside the outer4CC582 two-name
     *table. It can reach4D1025, which either keeps it or replaces it at
     *4D1C64 according to the live721ED8 value. Other groups remain strict.*/
    if (!actual || (strcmp(actual, first) && strcmp(actual, second) &&
        !(load->group == 1 && !strcmp(actual, "m02_92.xan")))) {
      fail(e, "retained background belongs to another group");
      return NULL;
    }
  }
  BkEndingSelectedAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(e, "allocation failed");
    return NULL;
  }
  a->config = config;
  a->background_name = bk_ending_normal_background(load->group, load->variant);
  a->replaced_background = (unsigned)bk_ending_selected_replaces_background(
      load->group, load->variant, load->selected);
  a->background_first = load->background && !a->replaced_background;
  a->anchor = a->follow = a->special = BK_MODEL_NONE;
  BkMenuCamera next_camera = *camera;
  BkEndingCameraPresets next_presets;
  uint32_t rng = *random;
  if (!load_actor(&a->primary, store, config.pack, name, e)) goto bad;
  const char *model_name = bk_clip_model_name(a->primary.clips);
  a->face = bk_face_assets_create_ending(store, "fambom", config.face,
                                          config.pack, a->primary.model, model_name, e);
  if (!a->face) goto bad;
  for (uint32_t i = 0; i < a->primary.model->submesh_count; ++i)
    if (bk_morph_group_mesh(a->primary.morph, i) && bk_face_assets_mesh(a->face, i)) {
      fail(e, "model and face MORP require a shared target owner");
      goto bad;
    }
  a->eyes = bk_eye_assets_create(store, config.pack, a->primary.model, model_name,
                                   bk_face_assets_config(a->face), e);
  a->cameras = bk_ending_camera_assets_create(store, load->group, config.event,
                                               config.camera_yaw, e);
  if (!a->eyes || !a->cameras) goto bad;
  if (a->replaced_background || load->background) {
    a->background_owner = a->replaced_background
        ? bk_ending_background_assets_create(store, "m02_90.xan", e)
        : bk_ending_background_assets_retain(load->background);
    const BkEndingBackgroundData *data = bk_ending_background_assets_data(a->background_owner);
    if (!data) goto bad;
    a->background = *data;
  }
  BkActorPose *poses[] = {a->primary.pose, bk_ending_camera_assets_pose(a->cameras, 0),
      bk_ending_camera_assets_pose(a->cameras, 1), a->background.pose};
  a->forest = bk_actor_forest_create(poses, a->background_owner ? 4 : 3, e);
  if (!a->forest ||
      (a->background_first && !bk_actor_forest_restore_global(a->forest, 3, a->background.root, e)) ||
      !attach_actor(a, &a->primary, 0, e) ||
      !bk_face_assets_initialize(a->face, &a->face_state, clocks, &rng, e)) goto bad;
  BkNodeReference root;
  snapshot(&a->primary, &root);
  float rotated[16];
  if (!bk_node_reference_position(&root, bk_actor_forest_world(a->forest, 0), config.position, e) ||
      !bk_ending_camera_root_rotation(rotated, root.local, config.yaw) ||
      !bk_actor_pose_root_local(a->primary.pose, rotated, e) ||
      !bk_eye_assets_select(a->eyes, 1, e) ||
      !bk_model_find_frame_first(a->primary.model, a->primary.root, "A_okosi", &a->anchor, e)) goto bad;
  if (a->anchor == BK_MODEL_NONE) {
    fail(e, "missing required A_okosi target");
    goto bad;
  }
  for (unsigned i = 0; i < 3; ++i) {
    if (!bk_model_find_frame_first(a->primary.model, a->primary.root,
                                    config.visible_nodes[i], &a->visible[i], e)) goto bad;
    if (a->visible[i] == BK_MODEL_NONE) ++a->missing_visible;
    else if (!bk_actor_forest_visibility(a->forest,
                 bk_actor_forest_node(a->forest, 0, a->visible[i]), 0, e)) goto bad;
  }
  const uint32_t tracks[] = {1, 2};
  if (!bk_actor_pose_request_mode(a->primary.pose, 1, BK_CLIP_REQUEST_CONFIGURED, e) ||
      !bk_ending_camera_assets_attach(a->cameras, a->forest, tracks,
                                        &next_camera, &next_presets, e)) goto bad;
  for (unsigned i = 0; i < BK_ENDING_NORMAL_NODES; ++i)
    if (!bk_model_find_frame_first(a->primary.model, a->primary.root,
                                    bk_ending_normal_node_name(i), &a->nodes[i], e)) goto bad;
  if (load->group == 2 && !bk_model_find_frame_first(a->primary.model, a->primary.root,
          load->variant ? "cris_baiza_A_Layer1" : "OYU", &a->special, e)) goto bad;
  /*4D1939..4D194A is ordinary seconds, not fixed30ticks. This advances all
   *real model effects but deliberately does not publish actor worlds. */
  if (!bk_ending_selected_assets_advance(a, 0, 30.f, e)) goto bad;
  const unsigned indices[] = {5, 13, 0};
  for (unsigned i = 0; i < 3; ++i) {
    if (a->nodes[indices[i]] == BK_MODEL_NONE) {
      fail(e, "missing required cached camera target");
      goto bad;
    }
    const float *world = bk_actor_pose_frame(a->primary.pose, a->nodes[indices[i]]);
    if (!world || !isfinite(world[12]) || !isfinite(world[13]) || !isfinite(world[14])) {
      fail(e, "invalid cached camera target");
      goto bad;
    }
    memcpy(a->targets[i], world + 12, sizeof(a->targets[i]));
  }
  if (!bk_ending_camera_assets_step(a->cameras, a->forest, tracks, &next_camera,
          BK_ENDING_CAMERA_FIXED, a->targets[0], NULL, 0, BK_FRAME_NONE, 0, e) ||
      !bk_model_find_frame_first(a->primary.model, a->primary.root, "A_kuch", &a->follow, e)) goto bad;
  if (a->follow == BK_MODEL_NONE) {
    fail(e, "missing required A_kuch follow target");
    goto bad;
  }
  if (a->replaced_background &&
      (!attach_actor(a, &a->background, 3, e) ||
       !bk_actor_pose_request_mode(a->background.pose, 0, BK_CLIP_REQUEST_CONFIGURED, e))) goto bad;
  if (load->group == 0 && !load->variant &&
      (!bk_ending_selected_assets_material_alpha(a, "I_hako", 1, 1, e) ||
       !bk_ending_selected_assets_material_alpha(a, "kurodenwa", 1, 1, e) ||
       !bk_ending_selected_assets_material_alpha(a, "OPkage", 1, -1, e))) goto bad;
  next_camera.fov = 1;
  a->background_state = a->background_owner != NULL;
  *random = rng;
  *camera = next_camera;
  *presets = next_presets;
  return a;
bad:
  bk_ending_selected_assets_destroy(a);
  return NULL;
}
int bk_ending_selected_assets_load_background(BkEndingSelectedAssets *a,
                                               BkResourceStore *store, char e[256]) {
  if (!a || !store || a->background_state == 2) return fail(e, "invalid or failed background owner");
  if (a->background_state == 1) return 1;
  BkEndingBackgroundAssets *owner = bk_ending_background_assets_create(store, a->background_name, e);
  const BkEndingBackgroundData *data = bk_ending_background_assets_data(owner);
  uint32_t index;
  if (!data || !bk_actor_forest_append(a->forest, data->pose, &index, e)) {
    bk_ending_background_assets_destroy(owner);
    return 0;
  }
  a->background_owner = owner;
  a->background = *data;
  a->background_state = 2;
  if (index != 3) return fail(e, "background registry changed outside owner");
  if (!attach_actor(a, &a->background, index, e) ||
      !bk_actor_pose_request_mode(a->background.pose, 0, BK_CLIP_REQUEST_CONFIGURED, e)) return 0;
  a->background_state = 1;
  return 1;
}
const BkEndingSelectedConfig *bk_ending_selected_assets_config(const BkEndingSelectedAssets *a) {
  return a ? &a->config : NULL;
}
int bk_ending_selected_assets_replaced_background(const BkEndingSelectedAssets *a) {
  return a ? (int)a->replaced_background : -1;
}
int bk_ending_selected_assets_background_first(const BkEndingSelectedAssets *a) {
  return a ? (int)a->background_first : -1;
}
BkEndingBackgroundAssets *bk_ending_selected_assets_background(const BkEndingSelectedAssets *a) {
  return a ? a->background_owner : NULL;
}
BkActorForest *bk_ending_selected_assets_forest(BkEndingSelectedAssets *a) {
  return a && a->background_state != 2 ? a->forest : NULL;
}
BkActorPose *bk_ending_selected_assets_pose(BkEndingSelectedAssets *a, unsigned actor) {
  if (!a || a->background_state == 2) return NULL;
  if (actor == 1 || actor == 2) return bk_ending_camera_assets_pose(a->cameras, actor - 1);
  const SelectedActor *p = effect_actor(a, actor);
  return p ? p->pose : NULL;
}
uint32_t bk_ending_selected_assets_root(const BkEndingSelectedAssets *a, unsigned actor) {
  if (a && a->background_state != 2 && (actor == 1 || actor == 2))
    return bk_actor_forest_node(a->forest, actor,
        bk_ending_camera_assets_root(a->cameras, actor - 1));
  const SelectedActor *p = effect_actor(a, actor);
  return p ? bk_actor_forest_node(a->forest, actor, p->root) : BK_FRAME_NONE;
}
const char *bk_ending_selected_assets_model_name(const BkEndingSelectedAssets *a, unsigned actor) {
  const SelectedActor *p = effect_actor(a, actor);
  return p ? bk_clip_model_name(p->clips) : NULL;
}
BkMaterialPose *bk_ending_selected_assets_materials(BkEndingSelectedAssets *a, unsigned actor) {
  const SelectedActor *p = effect_actor(a, actor);
  return p ? p->materials : NULL;
}
const BkMaterialAnimation *bk_ending_selected_assets_material_animation(
    const BkEndingSelectedAssets *a, unsigned actor) {
  const SelectedActor *p = effect_actor(a, actor);
  return p ? p->animation : NULL;
}
const BkMorphGroup *bk_ending_selected_assets_morph(const BkEndingSelectedAssets *a, unsigned actor) {
  const SelectedActor *p = effect_actor(a, actor);
  return p ? p->morph : NULL;
}
const BkMorphMesh *bk_ending_selected_assets_mesh(const BkEndingSelectedAssets *a,
                                                 unsigned actor, uint32_t submesh) {
  const SelectedActor *p = effect_actor(a, actor);
  if (!p) return NULL;
  const BkMorphMesh *mesh = bk_morph_group_mesh(p->morph, submesh);
  return mesh || actor != 0 ? mesh : bk_face_assets_mesh(a->face, submesh);
}
BkFaceAssets *bk_ending_selected_assets_face(BkEndingSelectedAssets *a) { return a ? a->face : NULL; }
BkFaceState *bk_ending_selected_assets_face_state(BkEndingSelectedAssets *a) { return a ? &a->face_state : NULL; }
BkEyeAssets *bk_ending_selected_assets_eyes(BkEndingSelectedAssets *a) { return a ? a->eyes : NULL; }
BkEndingCameraAssets *bk_ending_selected_assets_cameras(BkEndingSelectedAssets *a) { return a ? a->cameras : NULL; }
uint32_t bk_ending_selected_assets_node(const BkEndingSelectedAssets *a, unsigned i) {
  return a && i < BK_ENDING_NORMAL_NODES ? a->nodes[i] : BK_MODEL_NONE;
}
uint32_t bk_ending_selected_assets_anchor(const BkEndingSelectedAssets *a) { return a ? a->anchor : BK_MODEL_NONE; }
uint32_t bk_ending_selected_assets_follow(const BkEndingSelectedAssets *a) { return a ? a->follow : BK_MODEL_NONE; }
uint32_t bk_ending_selected_assets_special(const BkEndingSelectedAssets *a) { return a ? a->special : BK_MODEL_NONE; }
uint32_t bk_ending_selected_assets_visible_node(const BkEndingSelectedAssets *a, unsigned i) {
  return a && i < 3 ? a->visible[i] : BK_MODEL_NONE;
}
unsigned bk_ending_selected_assets_missing_visible(const BkEndingSelectedAssets *a) {
  return a ? a->missing_visible : 3;
}
const float *bk_ending_selected_assets_target(const BkEndingSelectedAssets *a, unsigned i) {
  return a && i < 3 ? a->targets[i] : NULL;
}
