#include "scene/ending_normal_render.h"
#include "model/skin.h"
#include "scene/avi_texture.h"
#include "scene/bom_render.h"
#include "scene/lighting_registry.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkEndingNormalRender {
  BkRenderer *renderer;
  BkEndingNormalAssets *assets;
  BkEndingSecondaryAssets *phase2_assets;
  BkEndingTertiaryAssets *phase3_assets;
  BkActorForest *forest;
  /*Preserve the existing primary0/upper1/background2 slots. Lower3 is an
   * independent source whose actual nodes may be parented into primary.*/
  unsigned actor_indices[4];
  BkActorPose *poses[4];
  BkActorRender *actors[4];
  BkActorRenderBatch *batches[3];
  BkActorRenderVisit *visits;
  BkLightSet *lights[3];
  BkSceneLightRegistry *registry;
  BkBomRender *bom;
  BkActorRender *bom_source;
  BkActorRender *bom_source_lower;
  BkAviTexture *movie;
  uint32_t capacity, roots[4], passes;
  unsigned light_indices[4], light_count;
  size_t world_floats;
  float *held_world;
  BkEndingNormalRender *secondary;
  BkViewport viewports[2];
  int ready, secondary_view, special_ready, staged_geometry;
};
static BkActorPose *asset_pose(BkEndingNormalRender *r, unsigned actor) {
  if (r->phase3_assets) {
    for (unsigned role = 0; role < BK_ENDING_TERTIARY_ASSET_ROLES; ++role)
      if (bk_ending_tertiary_assets_registry(r->phase3_assets, role) == actor)
        return bk_ending_tertiary_assets_pose(r->phase3_assets, role);
    return NULL;
  }
  return r->phase2_assets ? bk_ending_secondary_assets_pose(r->phase2_assets, actor)
                          : bk_ending_normal_assets_pose(r->assets, actor);
}
static unsigned tertiary_role(unsigned index) {
  static const unsigned roles[] = {BK_ENDING_TERTIARY_ASSET_PRIMARY,
      BK_ENDING_TERTIARY_ASSET_UPPER, BK_ENDING_TERTIARY_ASSET_BACKGROUND,
      BK_ENDING_TERTIARY_ASSET_LOWER};
  return index < 4 ? roles[index] : BK_ENDING_TERTIARY_ASSET_ROLES;
}
static size_t binding_count(const BkEndingNormalRender *r) {
  return r->phase3_assets ? bk_bom_dual_assets_count(
      bk_ending_tertiary_assets_bom(r->phase3_assets)) : r->phase2_assets ? 0 :
      bk_bom_assets_count(bk_ending_normal_assets_bom(r->assets));
}
static int prepare_bom(BkEndingNormalRender *r, const int32_t *disabled,
                         size_t count, char e[256]) {
  return r->bom ? bk_bom_render_prepare(r->bom, disabled, count, e) : count == 0;
}
static int prepare_view(BkEndingNormalRender *, const BkDrawDispatch *,
                        const BkMenuCamera *, const BkFog *,
                        const BkMaterialPose *, const int32_t *, size_t,
                        char e[256]);
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "normal ending render: %s", why);
  return 0;
}
int bk_ending_normal_render_relight(BkEndingNormalRender *r,
                                    BkEndingReloadLight operation, char e[256]) {
  if (!r || !r->registry || r->secondary_view)
    return fail(e, "missing primary light registry owner");
  int ok = operation == BK_ENDING_LIGHT_RESET
               ? bk_scene_light_registry_reset(r->registry)
           : operation == BK_ENDING_LIGHT_SELECT_BK3_L
               ? bk_scene_light_registry_select_bk3_l(r->registry)
           : operation == BK_ENDING_LIGHT_ENABLE
               ? bk_scene_light_registry_ambient(r->registry)
               : 0;
  return ok || fail(e, "invalid ending light registry reload");
}
int bk_ending_normal_render_inherit_lighting(BkEndingNormalRender *to,
                                            const BkEndingNormalRender *from,
                                            char e[256]) {
  if (!to || !from || to->ready || to->secondary_view || from->secondary_view)
    return fail(e, "invalid stage light-state handoff");
  return bk_scene_light_registry_inherit(to->registry, from->registry, e);
}
int bk_ending_normal_render_save_lighting(const BkEndingNormalRender *r,
                                         const BkModel *model,
                                         BkRetainedLightState *out, char e[256]) {
  if (!r || r->secondary_view)
    return fail(e, "missing primary retained light owner");
  return bk_scene_light_registry_save(r->registry, model, out, e);
}
int bk_ending_normal_render_restore_lighting(BkEndingNormalRender *r,
                                            const BkRetainedLightState *saved,
                                            char e[256]) {
  if (!r || r->ready || r->secondary_view)
    return fail(e, "invalid pure-UI light-state handoff");
  return bk_scene_light_registry_restore(r->registry, saved, e);
}
static int special_bom(BkEndingNormalAssets *assets, char e[256]) {
  BkBomAssets *bom = bk_ending_normal_assets_bom(assets);
  const BkModel *model =
      bk_actor_pose_model(bk_ending_normal_assets_pose(assets, 0));
  BkModelSkin *skin = bk_model_skin_create(model, e);
  if (!skin)
    return 0;
  uint32_t target = BK_MODEL_NONE;
  int valid = 1;
  for (uint32_t i = 0; valid && i < bk_bom_assets_count(bom); i++) {
    BkBomDeformBinding plan;
    valid = bk_bom_assets_plan(bom, i, &plan);
    if (!valid)
      break;
    const BkBomAssetMesh *s = bk_bom_assets_mesh(bom, plan.source),
                         *t = bk_bom_assets_mesh(bom, plan.target);
    valid = s && t && s->actor == 1 && t->actor == 0 &&
            (target == BK_MODEL_NONE || target == t->submesh);
    if (!valid)
      break;
    target = t->submesh;
    uint32_t entry = 0;
    while (entry < bk_model_skin_count(skin) &&
           bk_model_skin_entry(skin, entry)->submesh != target)
      entry++;
    if (entry == bk_model_skin_count(skin)) {
      valid = 0;
      break;
    }
    const BkSkinEntry *se = bk_model_skin_entry(skin, entry);
    for (size_t j = 0; valid && j < plan.count; j++) {
      int found = 0;
      for (uint32_t k = 0; !found && k < se->bone_count; k++) {
        const BkSkinBone *bone = bk_model_skin_bone(skin, entry, k);
        for (uint32_t n = 0; !found && n < bone->count; n++)
          found = bone->influences[n].index == plan.indices[j];
      }
      valid = found;
    }
  }
  bk_model_skin_destroy(skin);
  return valid || fail(e, "special views require independent auxiliary sources "
                          "and reset primary targets");
}
static int tertiary_bom(BkEndingTertiaryAssets *assets, char e[256]) {
  BkBomDualAssets *bom = bk_ending_tertiary_assets_bom(assets);
  if (!bom) return 1;
  const BkModel *model = bk_actor_pose_model(
      bk_ending_tertiary_assets_pose(assets, BK_ENDING_TERTIARY_ASSET_PRIMARY));
  BkModelSkin *skin = bk_model_skin_create(model, e);
  if (!skin) return 0;
  int valid = 1;
  for (uint32_t i = 0; valid && i < bk_bom_dual_assets_count(bom); ++i) {
    BkBomDeformBinding plan;
    valid = bk_bom_dual_assets_plan(bom, i, &plan);
    if (!valid || !plan.count) continue;
    const BkBomAssetMesh *source = bk_bom_dual_assets_mesh(bom, plan.source),
                         *target = bk_bom_dual_assets_mesh(bom, plan.target);
    valid = source && target && source->actor >= 1 && source->actor <= 2 &&
            target->actor == 0;
    if (!valid) break;
    uint32_t entry = 0;
    while (entry < bk_model_skin_count(skin) &&
           bk_model_skin_entry(skin, entry)->submesh != target->submesh) ++entry;
    if (entry == bk_model_skin_count(skin)) { valid = 0; break; }
    const BkSkinEntry *se = bk_model_skin_entry(skin, entry);
    for (size_t j = 0; valid && j < plan.count; ++j) {
      int found = 0;
      for (uint32_t b = 0; !found && b < se->bone_count; ++b) {
        const BkSkinBone *bone = bk_model_skin_bone(skin, entry, b);
        for (uint32_t n = 0; !found && n < bone->count; ++n)
          found = bone->influences[n].index == plan.indices[j];
      }
      valid = found;
    }
  }
  bk_model_skin_destroy(skin);
  return valid || fail(e, "third views require independent upper/lower sources "
                          "and skinned primary targets");
}
static int create_bom(BkEndingNormalRender *r, char e[256]) {
  if (r->phase2_assets) return 1;
  if (r->phase3_assets && !bk_ending_tertiary_assets_bom(r->phase3_assets))
    return 1;
  if (r->staged_geometry) {
    r->bom_source = bk_actor_render_create_view(r->actors[1], e);
    if (!r->bom_source) return 0;
    if (r->phase3_assets) {
      r->bom_source_lower = bk_actor_render_create_view(r->actors[3], e);
      if (!r->bom_source_lower) return 0;
    }
  }
  BkActorRender *actors[3] = {r->actors[0],
      r->bom_source ? r->bom_source : r->actors[1],
      r->bom_source_lower ? r->bom_source_lower : r->actors[3]};
  r->bom = r->phase3_assets ? bk_bom_render_create_dual(r->renderer,
      bk_ending_tertiary_assets_bom(r->phase3_assets), actors, e) :
      bk_bom_render_create(r->renderer, bk_ending_normal_assets_bom(r->assets), actors, e);
  return r->bom != NULL;
}
static BkEndingNormalRender *with_special_view(BkEndingNormalRender *r,
                                               char e[256]) {
  if (!r)
    return NULL;
  BkRenderer *renderer = r->renderer;
  BkEndingNormalRender *s = calloc(1, sizeof(*s));
  if (!s) {
    fail(e, "secondary owner allocation failed");
    goto bad;
  }
  r->secondary = s;
  s->secondary_view = 1;
  s->staged_geometry = r->staged_geometry;
  s->renderer = renderer;
  s->assets = r->assets;
  s->phase2_assets = r->phase2_assets;
  s->phase3_assets = r->phase3_assets;
  s->forest = r->forest;
  memcpy(s->actor_indices, r->actor_indices, sizeof(s->actor_indices));
  s->registry = r->registry;
  s->light_count = r->light_count;
  memcpy(s->light_indices, r->light_indices, sizeof(s->light_indices));
  s->movie = r->movie;
  s->capacity = r->capacity;
  memcpy(s->poses, r->poses, sizeof(s->poses));
  memcpy(s->roots, r->roots, sizeof(s->roots));
  s->visits = calloc((size_t)s->capacity * 2, sizeof(*s->visits));
  uint64_t meshes = 0;
  for (unsigned i = 0; i < 4; i++) {
    if (i == 2) continue;
    if (!s->poses[i]) continue;
    const BkModel *m = bk_actor_pose_model(s->poses[i]);
    s->world_floats += (size_t)m->frame_count * 16;
    for (uint32_t f = 0; f < m->frame_count; f++)
      if (m->frames[f].mesh_index != BK_MODEL_NONE)
        meshes += m->meshes[m->frames[f].mesh_index].submesh_count;
    s->actors[i] = bk_actor_render_create_view(r->actors[i], e);
    if (!s->actors[i])
      goto bad;
  }
  s->held_world = malloc(s->world_floats * sizeof(float));
  if (!s->visits || !s->held_world || !meshes || meshes > UINT32_MAX) {
    fail(e, "invalid secondary geometry capacity");
    goto bad;
  }
  if (!create_bom(s, e)) goto bad;
  for (unsigned i = 0; i < 2; i++) {
    s->batches[i] = bk_actor_render_batch_create(renderer, (uint32_t)meshes, e);
    s->lights[i] = bk_light_set_create(renderer, &(BkLighting){0}, e);
    if (!s->batches[i] || !s->lights[i])
      goto bad;
  }
  return r;
bad:
  bk_ending_normal_render_destroy(r);
  return NULL;
}
BkEndingNormalRender *bk_ending_normal_render_create_special(
    BkRenderer *renderer, BkResourceStore *store, BkEndingNormalAssets *assets,
    int32_t clock, char e[256]) {
  if (!renderer || !store || !assets) {
    fail(e, "missing special render inputs");
    return NULL;
  }
  if (!special_bom(assets, e)) return NULL;
  return with_special_view(
      bk_ending_normal_render_create(renderer, store, assets, clock, e), e);
}
typedef struct {
  BkEndingNormalRender *owner;
  const BkEndingSpecialRenderInput *input;
  unsigned stage;
} SpecialPrepare;
static int special_draw(void *p, const BkDrawDispatch *d,
                        const BkMenuCamera *camera, char e[256]) {
  SpecialPrepare *s = p;
  if (s->stage != 0 && s->stage != 5)
    return fail(e, "invalid special draw ordering");
  BkEndingNormalRender *view = s->stage ? s->owner->secondary : s->owner;
  if (!prepare_view(view, d, camera, s->input->fog, s->input->primary_materials,
                    s->input->bom_disabled, s->input->bom_count, e))
    return 0;
  s->stage++;
  return 1;
}
static int special_event(void *p, BkEndingSpecialRenderEvent event,
                         unsigned arg, char e[256]) {
  SpecialPrepare *s = p;
  static const BkEndingSpecialRenderEvent events[] = {
      BK_ENDING_SPECIAL_END,      BK_ENDING_SPECIAL_END,
      BK_ENDING_SPECIAL_VIEWPORT, BK_ENDING_SPECIAL_CLEAR,
      BK_ENDING_SPECIAL_BEGIN,    BK_ENDING_SPECIAL_END,
      BK_ENDING_SPECIAL_VIEWPORT};
  static const unsigned args[] = {0, 0, 1, 2, 0, 0, 0};
  if (s->stage >= 7 || s->stage == 0 || s->stage == 5 ||
      events[s->stage] != event || args[s->stage] != arg)
    return fail(e, "invalid special render event ordering");
  /* Record an executable draw plan. Vulkan fuses End/Begin around the depth
   * clear, retaining color and the explicit viewports. No empty GPU backend. */
  s->stage++;
  return 1;
}
int bk_ending_normal_render_prepare_special(
    BkEndingNormalRender *r, const BkEndingSpecialRenderInput *in,
    char e[256]) {
  if (!r)
    return fail(e, "missing special owner");
  r->special_ready = r->ready = 0;
  if (!r->secondary || !in)
    return fail(e, "special view storage unavailable");
  r->secondary->ready = 0;
  unsigned width, height;
  bk_renderer_extent(r->renderer, &width, &height);
  const BkViewport vp[] = {in->main_viewport, in->special_viewport};
  for (unsigned i = 0; i < 2; i++)
    if (!vp[i].width || !vp[i].height || vp[i].x > width || vp[i].y > height ||
        vp[i].width > width - vp[i].x || vp[i].height > height - vp[i].y)
      return fail(e, "invalid stored viewport");
  SpecialPrepare p = {r, in, 0};
  BkEndingSpecialScene scene = {r->forest,
                                in->camera,
                                in->materials,
                                in->material_count,
                                &p,
                                special_draw,
                                special_event};
  if (!bk_ending_special_scene_draw(&scene, in->bindings, in->dispatch, e) ||
      p.stage != 7) {
    r->ready = r->secondary->ready = 0;
    return 0;
  }
  memcpy(r->viewports, vp, sizeof(vp));
  r->special_ready = 1;
  return 1;
}
BkGpuMesh *bk_ending_normal_render_view_mesh(BkEndingNormalRender *r,
                                             unsigned view, unsigned actor,
                                             uint32_t submesh) {
  if (!r || view > 1)
    return NULL;
  return bk_ending_normal_render_mesh(view ? r->secondary : r, actor, submesh);
}
unsigned bk_ending_normal_render_pass_view(const BkEndingNormalRender *r,
                                           unsigned pass) {
  if (!r || !r->ready)
    return UINT32_MAX;
  if (pass < r->passes)
    return 0;
  return r->special_ready && pass - r->passes < r->secondary->passes
             ? 1
             : UINT32_MAX;
}
void bk_ending_normal_render_destroy(BkEndingNormalRender *r) {
  if (!r)
    return;
  bk_ending_normal_render_destroy(r->secondary);
  bk_bom_render_destroy(r->bom);
  bk_actor_render_destroy(r->bom_source);
  bk_actor_render_destroy(r->bom_source_lower);
  for (unsigned i = 0; i < 3; i++) {
    bk_actor_render_batch_destroy(r->batches[i]);
    bk_light_set_destroy(r->renderer, r->lights[i]);
  }
  for (unsigned i = 0; i < 4; ++i)
    bk_actor_render_destroy(r->actors[i]);
  if (!r->secondary_view) {
    bk_avi_texture_destroy(r->movie);
    bk_scene_light_registry_destroy(r->registry);
  }
  free(r->visits);
  free(r->held_world);
  free(r);
}
static BkEndingNormalRender *create_render(
    BkRenderer *renderer, BkResourceStore *store, BkEndingNormalAssets *assets,
    BkEndingSecondaryAssets *phase2, BkEndingTertiaryAssets *phase3,
    int32_t movie_clock, char e[256]) {
  if (!renderer || !store || (!!assets + !!phase2 + !!phase3 != 1) ||
      !(phase3 ? bk_ending_tertiary_assets_pose(phase3, BK_ENDING_TERTIARY_ASSET_BACKGROUND)
                : phase2 ? bk_ending_secondary_assets_pose(phase2, 3)
                : bk_ending_normal_assets_pose(assets, 4))) {
    fail(e, "missing renderer/resources or unloaded background");
    return NULL;
  }
  BkEndingNormalRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(e, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  r->assets = assets;
  r->phase2_assets = phase2;
  r->phase3_assets = phase3;
  r->actor_indices[0] = phase3 ? bk_ending_tertiary_assets_registry(phase3, 0) : 0;
  r->actor_indices[1] = phase3 ? bk_ending_tertiary_assets_registry(phase3, 1) : phase2 ? BK_MODEL_NONE : 1;
  r->actor_indices[2] = phase3 ? bk_ending_tertiary_assets_registry(phase3, 5) : phase2 ? 3 : 4;
  r->actor_indices[3] = phase3 ? bk_ending_tertiary_assets_registry(phase3, 2) : BK_MODEL_NONE;
  BkActorForest *forest = r->forest = phase3 ? bk_ending_tertiary_assets_forest(phase3) : phase2
      ? bk_ending_secondary_assets_forest(phase2)
      : bk_ending_normal_assets_forest(assets);
  r->capacity = bk_frame_tree_count(bk_actor_forest_tree(forest));
  r->visits = calloc((size_t)r->capacity * 3, sizeof(*r->visits));
  if (!r->visits) {
    fail(e, "visit allocation failed");
    goto bad;
  }
  uint64_t meshes = 0;
  uint32_t movie_target = BK_MODEL_NONE;
  BkLightSource sources[4];
  for (unsigned i = 0; i < 4; i++) {
    if (r->actor_indices[i] == BK_MODEL_NONE) continue;
    r->poses[i] = asset_pose(r, r->actor_indices[i]);
    const BkModel *m = bk_actor_pose_model(r->poses[i]);
    r->world_floats += (size_t)m->frame_count * 16;
    r->roots[i] = BK_FRAME_NONE;
    for (uint32_t f = 0; f < m->frame_count; f++) {
      if (m->frames[f].parent_index == BK_MODEL_NONE)
        r->roots[i] = bk_actor_forest_node(forest, r->actor_indices[i], f);
      if (m->frames[f].mesh_index != BK_MODEL_NONE)
        meshes += m->meshes[m->frames[f].mesh_index].submesh_count;
    }
    for (uint32_t t = 0; t < m->texture_count; t++)
      if (!strcmp(m->textures[t].filename, "D_moza.bmp")) {
        /*466313/466327 set645690/645698=1:41745b creates a distinct
         * texture, while429b43 leaves later duplicate names unregistered.
         * Thus4d35fc's first registry lookup still selects primary. The
         * third-stage lower model also has D_moza.bmp; its separate bitmap
         * must neither reject loading nor receive the movie override. */
        if (phase3 && i == 3)
          continue;
        if (i || movie_target != BK_MODEL_NONE) {
          fail(e, "unexpected movie texture alias");
          goto bad;
        }
        movie_target = t;
      }
    r->actors[i] = bk_actor_render_create(
        renderer, store, i == 2 ? "bk3_03" : phase3 ? "bk3_11" : phase2 ? "bk3_09" : "bk3_08", m,
        i ? NULL : phase3 ? bk_ending_tertiary_assets_eyes(phase3) : phase2 ? bk_ending_secondary_assets_eyes(phase2)
                           : bk_ending_normal_assets_eyes(assets), e);
    if (!r->actors[i] || r->roots[i] == BK_FRAME_NONE)
      goto bad;
  }
  if (!meshes || meshes > UINT32_MAX || movie_target == BK_MODEL_NONE) {
    fail(e, "missing geometry or movie target");
    goto bad;
  }
  r->held_world = malloc(r->world_floats * sizeof(float));
  if (!r->held_world) {
    fail(e, "world snapshot allocation failed");
    goto bad;
  }
  r->staged_geometry = bk_frame_tree_next(bk_actor_forest_tree(forest),
                                          r->roots[2]) != BK_FRAME_NONE;
  /*Register every third-stage light-bearing model in actual load order.
   * Existing normal/secondary keep their established primary/background list.*/
  const unsigned fresh[] = {0, 1, 3, 2}, retained[] = {2, 0, 1, 3};
  const unsigned *order = r->staged_geometry ? retained : fresh;
  for (unsigned j = 0; j < 4; ++j) {
    unsigned i = order[j];
    if (!r->poses[i] || (!phase3 && i != 0 && i != 2)) continue;
    const BkModel *model = bk_actor_pose_model(r->poses[i]);
    if (phase3 && !bk_model_chunk(model, "LIGH")) continue;
    unsigned n = r->light_count++;
    r->light_indices[n] = i;
    sources[n].model = model;
    sources[n].world.matrices = bk_actor_pose_world(r->poses[i], &sources[n].world.floats);
  }
  r->registry = bk_scene_light_registry_create(sources, r->light_count, e);
  if ((phase3 && !tertiary_bom(phase3, e)) ||
      (assets && r->staged_geometry && !special_bom(assets, e)) ||
      !r->registry || !create_bom(r, e)) goto bad;
  for (unsigned i = 0; i < 3; i++) {
    r->batches[i] = bk_actor_render_batch_create(renderer, (uint32_t)meshes, e);
    r->lights[i] = bk_light_set_create(renderer, &(BkLighting){0}, e);
    if (!r->batches[i] || !r->lights[i])
      goto bad;
  }
  BkBlob blob = {0};
  if (bk_resources_read(store, "bk3_18", "poi.avi", &blob, e) != BK_RESOURCE_OK)
    goto bad;
  r->movie =
      bk_avi_texture_create(renderer, blob.data, blob.size, movie_clock, e);
  bk_blob_free(&blob);
  if (!r->movie)
    goto bad;
  const BkImage *image = bk_avi_texture_image(r->movie);
  if (image->width != 256 || image->height != 256) {
    fail(e, "movie dimensions differ from native256 surface");
    goto bad;
  }
  if (!bk_actor_render_texture_surface(r->actors[0], movie_target,
                                       bk_avi_texture_gpu(r->movie), e))
    goto bad;
  return r;
bad:
  bk_ending_normal_render_destroy(r);
  return NULL;
}
BkEndingNormalRender *
bk_ending_normal_render_create(BkRenderer *renderer, BkResourceStore *store,
                               BkEndingNormalAssets *assets,
                               int32_t movie_clock, char e[256]) {
  return create_render(renderer, store, assets, NULL, NULL, movie_clock, e);
}
BkEndingNormalRender *
bk_ending_secondary_render_create(BkRenderer *renderer, BkResourceStore *store,
                                   BkEndingSecondaryAssets *assets,
                                   int32_t movie_clock, char e[256]) {
  return with_special_view(create_render(renderer, store, NULL, assets, NULL,
                                         movie_clock, e), e);
}
BkEndingNormalRender *
bk_ending_tertiary_render_create(BkRenderer *renderer, BkResourceStore *store,
                                  BkEndingTertiaryAssets *assets,
                                  int32_t movie_clock, char e[256]) {
  return with_special_view(create_render(renderer, store, NULL, NULL, assets,
                                         movie_clock, e), e);
}
int bk_ending_normal_render_movie_step(BkEndingNormalRender *r, int32_t now,
                                       int32_t restart, char e[256]) {
  return r ? bk_avi_texture_step(r->movie, now, restart, e)
           : fail(e, "missing movie owner");
}
uint32_t bk_ending_normal_render_movie_frame(const BkEndingNormalRender *r) {
  return r ? bk_avi_texture_frame(r->movie) : UINT32_MAX;
}
const BkImage *
bk_ending_normal_render_movie_image(const BkEndingNormalRender *r) {
  return r ? bk_avi_texture_image(r->movie) : NULL;
}
static int prepare_actors(BkEndingNormalRender *r, const float *view,
                          const float *projection,
                          const BkMaterialPose *material,
                          const int32_t *disabled, size_t count, char e[256]) {
  if (r->phase3_assets) {
    BkEndingTertiaryAssets *a = r->phase3_assets;
    if (material && material != bk_ending_tertiary_assets_materials(a, 0))
      return fail(e, "third effects require the actual material owner");
    for (unsigned i = 0; i < 4; ++i) {
      if (!r->actors[i] || (r->secondary_view && i == 2)) continue;
      unsigned role = tertiary_role(i);
      if (!bk_actor_render_prepare_effects(r->actors[i], r->poses[i],
          bk_ending_tertiary_assets_materials(a, role),
          i ? NULL : bk_ending_tertiary_assets_face(a),
          bk_ending_tertiary_assets_morph(a, role), view, projection, e)) return 0;
    }
    return prepare_bom(r, disabled, count, e);
  }
  if (r->phase2_assets) {
    BkEndingSecondaryAssets *a = r->phase2_assets;
    if (count || disabled || (material &&
        material != bk_ending_secondary_assets_materials(a, 0)))
      return fail(e, "secondary effects require their actual asset owners");
    return bk_actor_render_prepare_effects(r->actors[0], r->poses[0],
        bk_ending_secondary_assets_materials(a, 0),
        bk_ending_secondary_assets_face(a), bk_ending_secondary_assets_morph(a, 0),
        view, projection, e) &&
        (r->secondary_view || bk_actor_render_prepare_morph(r->actors[2], r->poses[2],
            bk_ending_secondary_assets_materials(a, 3),
            bk_ending_secondary_assets_morph(a, 3), view, projection, e));
  }
  BkBomAssets *bom = bk_ending_normal_assets_bom(r->assets);
  return bk_actor_render_prepare(r->actors[0], r->poses[0], material,
                                 bk_ending_normal_assets_face(r->assets), view,
                                 projection, e) &&
         bk_actor_render_prepare_morph(
             r->actors[1], r->poses[1], bk_bom_assets_materials(bom),
             bk_bom_assets_morph(bom), view, projection, e) &&
         (r->secondary_view ||
          bk_actor_render_prepare(r->actors[2], r->poses[2], NULL, NULL, view,
                                  projection, e)) &&
         bk_bom_render_prepare(r->bom, disabled, count, e);
}
/*An outer background retained across reload precedes the newly loaded
 * roots. Its draw does not publish those roots. Capture each selected actor
 * at its actual draw walk, and pin a separate auxiliary source for primary
 * BOM commands so a later auxiliary walk cannot overwrite that snapshot. */
static int prepare_staged_actor(BkEndingNormalRender *r, unsigned index,
                                 const float *view, const float *projection,
                                 const BkMaterialPose *material,
                                 const int32_t *disabled, size_t count,
                                 char e[256]) {
  if (index == 2) {
    BkEndingBackgroundAssets *background = r->phase3_assets
        ? bk_ending_tertiary_assets_background(r->phase3_assets) : r->phase2_assets
        ? bk_ending_secondary_assets_background(r->phase2_assets)
        : bk_ending_normal_assets_background(r->assets);
    const BkEndingBackgroundData *data = bk_ending_background_assets_data(background);
    if (!data)
      return fail(e, "missing retained background effect owner");
    return bk_actor_render_prepare_morph(r->actors[2], r->poses[2],
        data->materials, data->morph, view, projection, e);
  }
  if (r->phase3_assets) {
    BkEndingTertiaryAssets *a = r->phase3_assets;
    if (index >= 4 || !r->actors[index] || (material &&
        material != bk_ending_tertiary_assets_materials(a, 0)))
      return fail(e, "invalid third-stage geometry owner");
    unsigned role = tertiary_role(index);
    if (!bk_actor_render_prepare_effects(r->actors[index], r->poses[index],
        bk_ending_tertiary_assets_materials(a, role),
        index ? NULL : bk_ending_tertiary_assets_face(a),
        bk_ending_tertiary_assets_morph(a, role), view, projection, e)) return 0;
    if (index != 0 || !r->bom) return 1;
    if (!r->bom_source || !r->bom_source_lower)
      return fail(e, "missing retained-stage upper/lower snapshots");
    return bk_actor_render_prepare_morph(r->bom_source, r->poses[1],
        bk_ending_tertiary_assets_materials(a, 1),
        bk_ending_tertiary_assets_morph(a, 1), view, projection, e) &&
        bk_actor_render_prepare_morph(r->bom_source_lower, r->poses[3],
        bk_ending_tertiary_assets_materials(a, 2),
        bk_ending_tertiary_assets_morph(a, 2), view, projection, e) &&
        prepare_bom(r, disabled, count, e);
  }
  if (r->phase2_assets) {
    if (index != 0 || count || disabled || (material &&
        material != bk_ending_secondary_assets_materials(r->phase2_assets, 0)))
      return fail(e, "invalid secondary stage geometry owner");
    return bk_actor_render_prepare_effects(r->actors[0], r->poses[0],
        bk_ending_secondary_assets_materials(r->phase2_assets, 0),
        bk_ending_secondary_assets_face(r->phase2_assets),
        bk_ending_secondary_assets_morph(r->phase2_assets, 0), view, projection, e);
  }
  BkBomAssets *bom = bk_ending_normal_assets_bom(r->assets);
  if (index == 1)
    return bk_actor_render_prepare_morph(r->actors[1], r->poses[1],
        bk_bom_assets_materials(bom), bk_bom_assets_morph(bom), view, projection, e);
  if (index != 0 || !r->bom_source)
    return fail(e, "missing retained-stage BOM source snapshot");
  return bk_actor_render_prepare(r->actors[0], r->poses[0], material,
             bk_ending_normal_assets_face(r->assets), view, projection, e) &&
         bk_actor_render_prepare_morph(r->bom_source, r->poses[1],
             bk_bom_assets_materials(bom), bk_bom_assets_morph(bom),
             view, projection, e) &&
         bk_bom_render_prepare(r->bom, disabled, count, e);
}
static int prepare_view(BkEndingNormalRender *r, const BkDrawDispatch *dispatch,
                        const BkMenuCamera *camera, const BkFog *fog,
                        const BkMaterialPose *material, const int32_t *disabled,
                        size_t count, char e[256]) {
  if (!r)
    return fail(e, "missing owner");
  r->ready = 0;
  r->passes = 0;
  if (!dispatch || dispatch->mode != (r->secondary_view ? 2 : 1) || !camera ||
      !fog ||
      count != binding_count(r) ||
      (count && !disabled) || !bk_fog_validate(fog, e))
    return fail(e, "invalid regular state");
  for (unsigned i = 0; i < 52; i++) {
    uint32_t root = i == 0                         ? r->roots[2]
                    : i == 4                       ? r->roots[0]
                    : i == 5                       ? r->roots[1]
                    : r->secondary_view && i == 20 ? r->roots[0]
                    : r->secondary_view && i == 21 ? r->roots[1]
                                                   : 0;
    if (dispatch->objects[i] && dispatch->objects[i] != root)
      return fail(e, "unexpected regular root");
  }
  if (!r->secondary_view && (dispatch->objects[4] || dispatch->objects[5]) &&
      !dispatch->objects[0])
    return fail(e, "normal actor pass requires preceding background walk");
  BkActorForest *forest = r->forest;
  if (bk_frame_tree_count(bk_actor_forest_tree(forest)) != r->capacity)
    return fail(e, "CPU registry changed before GPU retirement");
  if (!r->secondary_view && dispatch->objects[0]) {
    const BkFrameTree *tree = bk_actor_forest_tree(forest);
    uint32_t background_actor, background_frame, hidden;
    if (bk_frame_tree_parent(tree, r->roots[2]) != 0 ||
        ((bk_frame_tree_next(tree, r->roots[2]) != BK_FRAME_NONE) !=
          r->staged_geometry) ||
        !bk_actor_forest_binding(forest, r->roots[2], &background_actor,
                                 &background_frame) ||
        !bk_actor_pose_hidden(r->poses[2], background_frame, &hidden) ||
        (!r->phase2_assets && !r->phase3_assets && !r->staged_geometry && hidden))
      return fail(e,
                  "background order or visibility differs from captured topology");
  }
  const float *anchor = bk_actor_forest_world(forest, 1);
  for (unsigned i = 0; i < 16; i++)
    if (camera->pose.world[i] != anchor[i])
      return fail(e, "camera does not match forest anchor");
  float projection[16];
  if (!bk_camera_projection(projection,
                            &(BkCameraLens){camera->fov, .75f, .5f, 126384}))
    return fail(e, "invalid camera lens");
  BkLightingPassInput input = {0};
  BkLightingPass pass;
  if (!bk_scene_light_registry_input(r->registry, &input) ||
      !bk_draw_dispatch_lighting(dispatch, &input) ||
      !bk_lighting_pass(&input, &pass))
    return fail(e, "invalid lighting plan");
  unsigned flushes = 0;
  unsigned prepared = 0;
  for (uint32_t c = 0; c < pass.count; c++) {
    const BkLightingCommand *command = &pass.commands[c];
    if (command->kind <= BK_PASS_LIGHT_ENABLE) {
      if (!bk_scene_light_registry_command(r->registry, command))
        return fail(e, "invalid light command");
    } else if (command->kind == BK_PASS_OBJECT) {
      if (r->passes >= (r->secondary_view ? 2u : 3u) || flushes != r->passes)
        return fail(e, "unexpected flush order");
      const BkFrameVisit *walk;
      uint32_t n, submitted = 0;
      if (!bk_actor_forest_draw(forest, command->target, &walk, &n, e))
        return 0;
      const float *view = bk_actor_forest_view(forest);
      /*42273b returns before publishing any nodes when its selected root is
       * hidden. Secondary endings can hide the background: retain that empty
       * light/queue flush, and wait for the primary walk to publish its cache
       * before uploading. A refresh here would change the next frame's camera
       * and controller inputs. Normal/BOM snapshots retain their validated
       * visible-background requirement above. */
      unsigned prepare_now = 0;
      if (r->staged_geometry) {
        unsigned needed = 0;
        for (uint32_t i = 0; i < n; ++i) {
          uint32_t actor, frame;
          if (!walk[i].submit || !bk_actor_forest_binding(
                  forest, walk[i].node, &actor, &frame))
            continue;
          for (unsigned a = 0; a < 4; ++a)
            if (!(r->secondary_view && a == 2) && r->actors[a] && r->actor_indices[a] == actor)
              needed |= 1u << a;
        }
        prepare_now = needed & ~prepared;
        for (unsigned a = 0; a < 4; ++a)
          if ((prepare_now & (1u << a)) && !prepare_staged_actor(
                  r, a, view, projection, material, disabled, count, e))
            return 0;
      } else if (!prepared && (n || (!r->phase2_assets && !r->phase3_assets))) {
        if (!prepare_actors(r, view, projection, material, disabled, count, e))
          return 0;
        for (unsigned a = 0; a < 4; ++a)
          if (r->actors[a] && !(r->secondary_view && a == 2)) prepare_now |= 1u << a;
      }
      size_t offset = 0;
      for (unsigned a = 0; (prepare_now || prepared) && a < 4; a++) {
        if (!r->poses[a] || (r->secondary_view && a == 2)) continue;
        size_t floats;
        const float *world = bk_actor_pose_world(r->poses[a], &floats);
        if (offset > r->world_floats || floats > r->world_floats - offset)
          return fail(e, "world snapshot capacity changed");
        if (prepare_now & (1u << a))
          memcpy(r->held_world + offset, world, floats * sizeof(float));
        else if ((prepared & (1u << a)) &&
                 memcmp(r->held_world + offset, world, floats * sizeof(float)))
          return fail(e, "later flush changed prepared actor worlds");
        offset += floats;
      }
      prepared |= prepare_now;
      BkActorRenderVisit *visits = r->visits + r->passes * r->capacity;
      for (uint32_t i = 0; i < n; i++) {
        uint32_t actor, frame;
        if (!walk[i].submit ||
            !bk_actor_forest_binding(forest, walk[i].node, &actor, &frame))
          continue;
        unsigned index = 0;
        while (index < 4 && r->actor_indices[index] != actor) ++index;
        if (index == 4) {
          const BkModel *track = bk_actor_pose_model(asset_pose(r, actor));
          if (!track || frame >= track->frame_count)
            return fail(e, "unbound actor selected by root walk");
          if (track->frames[frame].mesh_index != BK_MODEL_NONE)
            return fail(e, "camera geometry selected by unexpected root walk");
          continue;
        }
        if (index >= 4 || !r->actors[index] || submitted >= r->capacity)
          return fail(e, "unexpected actor visit");
        visits[submitted++] = (BkActorRenderVisit){r->actors[index], frame};
      }
      BkLightWorld worlds[4];
      for (unsigned j = 0; j < r->light_count; ++j)
        worlds[j].matrices = bk_actor_pose_world(r->poses[r->light_indices[j]], &worlds[j].floats);
      BkLighting snapshot;
      unsigned p = r->passes;
      if (!bk_scene_light_registry_values(r->registry, worlds, r->light_count, &snapshot,
                                          e) ||
          !bk_light_set_update(r->renderer, r->lights[p], &snapshot, e) ||
          !bk_light_set_view(r->renderer, r->lights[p], camera->pose.world + 12,
                             e) ||
          !bk_light_set_fog(r->renderer, r->lights[p], fog, view, e) ||
          !bk_actor_render_batch_prepare_visits(r->batches[p], visits,
                                                submitted, e))
        return 0;
      r->passes++;
    } else if (command->kind == BK_PASS_FLUSH) {
      if (flushes >= r->passes)
        return fail(e, "flush without prepared object");
      flushes++;
    } else
      return fail(e, "unsupported regular draw event");
  }
  if (flushes != r->passes)
    return fail(e, "incomplete regular flushes");
  r->ready = 1;
  return 1;
}
static int draw_view(BkEndingNormalRender *r, char e[256]) {
  if (!r || !r->ready)
    return fail(e, "no prepared regular frame");
  for (uint32_t i = 0; i < r->passes; i++)
    if (!bk_actor_render_batch_draw(r->batches[i], r->lights[i], e))
      return 0;
  return 1;
}
int bk_ending_normal_render_prepare_regular(BkEndingNormalRender *r,
                                            const BkDrawDispatch *d,
                                            const BkMenuCamera *camera,
                                            const BkFog *fog,
                                            const BkMaterialPose *material,
                                            const int32_t *disabled,
                                            size_t count, char e[256]) {
  if (!r || r->secondary_view)
    return fail(e, "invalid regular owner");
  r->special_ready = 0;
  return prepare_view(r, d, camera, fog, material, disabled, count, e);
}
typedef struct {
  BkEndingNormalRender *render;
  const BkEndingEventRenderInput *input;
  char *error;
} EventPrepare;
static int event_movie(void *context) {
  EventPrepare *p = context;
  return bk_ending_normal_render_movie_step(p->render, p->input->movie_clock,
                                            p->input->movie_restart_clock,
                                            p->error);
}
static int event_special(void *context, BkDrawDispatch *dispatch) {
  EventPrepare *p = context;
  BkEndingSpecialRenderInput view = p->input->scene;
  view.dispatch = dispatch;
  return bk_ending_normal_render_prepare_special(p->render, &view, p->error);
}
static int event_status(void *context, int32_t *result, uint32_t *flags) {
  EventPrepare *p = context;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_STATUS, .slot = 1};
  int playing = 0;
  /* STATUS does not choose a cue, action variant or selection. A mixer
   * error terminates the frame; it is never reported as stopped playback. */
  if (!bk_ending_audio_call(p->input->audio, (unsigned)p->input->roots.group,
                             0, 0, &call, &playing, p->error))
    return 0;
  *result = 0;
  *flags = playing ? 1u : 0u;
  return 1;
}
static int event_duck(void *context, uint32_t active) {
  EventPrepare *p = context;
  int32_t result;
  return bk_ending_audio_duck(p->input->audio, p->input->duck_transition,
                               (int32_t)active, p->input->voice_master,
                               p->input->seconds, &result, p->error);
}
static int event_regular(void *context, const BkDrawDispatch *dispatch) {
  EventPrepare *p = context;
  const BkEndingSpecialRenderInput *view = &p->input->scene;
  return bk_ending_normal_render_prepare_regular(
      p->render, dispatch, view->camera, view->fog, view->primary_materials,
      view->bom_disabled, view->bom_count, p->error);
}
int bk_ending_normal_render_prepare_event(BkEndingNormalRender *r,
                                          const BkEndingEventRenderInput *in,
                                          char e[256]) {
  if (!r || r->secondary_view)
    return fail(e, "invalid event render owner");
  r->ready = r->special_ready = 0;
  if (r->secondary)
    r->secondary->ready = 0;
  if (!in || in->roots.flow != 16 || in->roots.group < 0 ||
      in->roots.group >= 5)
    return fail(e, "invalid ending event roots");
  BkEventDrawState event = {.mode = in->event_mode};
  if (in->roots.event_state != 7 && event.mode >= 0 && event.mode <= 2) {
    int present = 0;
    if (!in->audio || !in->duck_transition || !isfinite(in->seconds) ||
        in->seconds < 0 || !bk_ending_audio_present(in->audio, 1, &present))
      return fail(e, "missing event audio/retained state or invalid time");
    event.buffer_present = present ? 1u : 0u;
  }
  EventPrepare context = {r, in, e};
  BkDrawDispatchOps ops = {&context, event_movie, event_special, event_status,
                           event_duck, event_regular};
  if (!bk_draw_dispatch_run(&in->roots, &event, &ops)) {
    r->ready = r->special_ready = 0;
    if (r->secondary)
      r->secondary->ready = 0;
    return 0;
  }
  return 1;
}
int bk_ending_normal_render_draw(BkEndingNormalRender *r, char e[256]) {
  if (!r || !r->ready)
    return fail(e, "missing prepared frame");
  if (!r->special_ready)
    return draw_view(r, e);
  return bk_renderer_viewport(r->renderer, r->viewports, e) &&
         draw_view(r, e) &&
         bk_renderer_viewport(r->renderer, r->viewports + 1, e) &&
         bk_renderer_clear_depth(r->renderer, 1, e) &&
         draw_view(r->secondary, e) &&
         bk_renderer_viewport(r->renderer, r->viewports, e);
}
BkGpuMesh *bk_ending_normal_render_mesh(BkEndingNormalRender *r, unsigned actor,
                                        uint32_t submesh) {
  unsigned i = 0;
  if (!r || actor == BK_MODEL_NONE) return NULL;
  while (i < 4 && r->actor_indices[i] != actor) ++i;
  return i < 4 && r->actors[i]
             ? bk_actor_render_mesh(r->actors[i],
                                    bk_actor_pose_model(r->poses[i]), submesh)
             : NULL;
}
uint32_t bk_ending_normal_render_pass_count(const BkEndingNormalRender *r) {
  return r && r->ready
             ? r->passes + (r->special_ready ? r->secondary->passes : 0)
             : 0;
}
uint32_t bk_ending_normal_render_queue_count(const BkEndingNormalRender *r,
                                             unsigned pass) {
  if (r && r->special_ready && pass >= r->passes)
    return bk_ending_normal_render_queue_count(r->secondary, pass - r->passes);
  return r && r->ready && pass < r->passes
             ? bk_actor_render_batch_count(r->batches[pass])
             : 0;
}
int bk_ending_normal_render_queue_item(const BkEndingNormalRender *r,
                                       unsigned pass, uint32_t item,
                                       uint32_t *actor, uint32_t *frame,
                                       uint32_t *submesh) {
  uint32_t visit, f, mesh;
  if (r && r->special_ready && pass >= r->passes)
    return bk_ending_normal_render_queue_item(r->secondary, pass - r->passes,
                                              item, actor, frame, submesh);
  if (!r || !r->ready || pass >= r->passes || !actor || !frame || !submesh ||
      !bk_actor_render_batch_item(r->batches[pass], item, &visit, &f, &mesh) ||
      visit >= r->capacity)
    return 0;
  const BkActorRender *a = r->visits[pass * r->capacity + visit].actor;
  for (unsigned i = 0; i < 4; i++)
    if (a == r->actors[i]) {
      *actor = r->actor_indices[i];
      *frame = f;
      *submesh = mesh;
      return 1;
    }
  return 0;
}
