#include "scene/ending_normal_render.h"
#include "model/skin.h"
#include "scene/avi_texture.h"
#include "scene/bom_render.h"
#include "scene/lighting_registry.h"
#include <stdlib.h>
#include <string.h>
struct BkEndingNormalRender {
  BkRenderer *renderer;
  BkEndingNormalAssets *assets;
  BkActorPose *poses[3];
  BkActorRender *actors[3];
  BkActorRenderBatch *batches[3];
  BkActorRenderVisit *visits;
  BkLightSet *lights[3];
  BkSceneLightRegistry *registry;
  BkBomRender *bom;
  BkAviTexture *movie;
  uint32_t capacity, roots[3], passes;
  size_t world_floats;
  float *held_world;
  BkEndingNormalRender *secondary;
  BkViewport viewports[2];
  int ready, secondary_view, special_ready;
};
static const unsigned indices[] = {0, 1, 4};
static int prepare_view(BkEndingNormalRender *, const BkDrawDispatch *,
                        const BkMenuCamera *, const BkFog *,
                        const BkMaterialPose *, const int32_t *, size_t,
                        char e[256]);
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "normal ending render: %s", why);
  return 0;
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
BkEndingNormalRender *bk_ending_normal_render_create_special(
    BkRenderer *renderer, BkResourceStore *store, BkEndingNormalAssets *assets,
    int32_t clock, char e[256]) {
  if (!renderer || !store || !assets) {
    fail(e, "missing special render inputs");
    return NULL;
  }
  if (!special_bom(assets, e))
    return NULL;
  BkEndingNormalRender *r =
      bk_ending_normal_render_create(renderer, store, assets, clock, e);
  if (!r)
    return NULL;
  BkEndingNormalRender *s = calloc(1, sizeof(*s));
  if (!s) {
    fail(e, "secondary owner allocation failed");
    goto bad;
  }
  r->secondary = s;
  s->secondary_view = 1;
  s->renderer = renderer;
  s->assets = assets;
  s->registry = r->registry;
  s->movie = r->movie;
  s->capacity = r->capacity;
  memcpy(s->poses, r->poses, sizeof(s->poses));
  memcpy(s->roots, r->roots, sizeof(s->roots));
  s->visits = calloc((size_t)s->capacity * 2, sizeof(*s->visits));
  uint64_t meshes = 0;
  for (unsigned i = 0; i < 2; i++) {
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
  s->bom = bk_bom_render_create(renderer, bk_ending_normal_assets_bom(assets),
                                s->actors, e);
  if (!s->bom)
    goto bad;
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
  BkEndingSpecialScene scene = {bk_ending_normal_assets_forest(r->assets),
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
  for (unsigned i = 0; i < 3; i++) {
    bk_actor_render_batch_destroy(r->batches[i]);
    bk_actor_render_destroy(r->actors[i]);
    bk_light_set_destroy(r->renderer, r->lights[i]);
  }
  if (!r->secondary_view) {
    bk_avi_texture_destroy(r->movie);
    bk_scene_light_registry_destroy(r->registry);
  }
  free(r->visits);
  free(r->held_world);
  free(r);
}
BkEndingNormalRender *
bk_ending_normal_render_create(BkRenderer *renderer, BkResourceStore *store,
                               BkEndingNormalAssets *assets,
                               int32_t movie_clock, char e[256]) {
  if (!renderer || !store || !assets ||
      !bk_ending_normal_assets_pose(assets, 4)) {
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
  BkActorForest *forest = bk_ending_normal_assets_forest(assets);
  r->capacity = bk_frame_tree_count(bk_actor_forest_tree(forest));
  r->visits = calloc((size_t)r->capacity * 3, sizeof(*r->visits));
  if (!r->visits) {
    fail(e, "visit allocation failed");
    goto bad;
  }
  uint64_t meshes = 0;
  uint32_t movie_target = BK_MODEL_NONE;
  BkLightSource sources[2];
  for (unsigned i = 0; i < 3; i++) {
    r->poses[i] = bk_ending_normal_assets_pose(assets, indices[i]);
    const BkModel *m = bk_actor_pose_model(r->poses[i]);
    r->world_floats += (size_t)m->frame_count * 16;
    r->roots[i] = BK_FRAME_NONE;
    for (uint32_t f = 0; f < m->frame_count; f++) {
      if (m->frames[f].parent_index == BK_MODEL_NONE)
        r->roots[i] = bk_actor_forest_node(forest, indices[i], f);
      if (m->frames[f].mesh_index != BK_MODEL_NONE)
        meshes += m->meshes[m->frames[f].mesh_index].submesh_count;
    }
    for (uint32_t t = 0; t < m->texture_count; t++)
      if (!strcmp(m->textures[t].filename, "D_moza.bmp")) {
        /* All ten real normal entries bind this single primary resource;
         * a new alias layout requires original registry verification. */
        if (i || movie_target != BK_MODEL_NONE) {
          fail(e, "unexpected movie texture alias");
          goto bad;
        }
        movie_target = t;
      }
    if (i != 1) {
      unsigned s = i ? 1 : 0;
      sources[s].model = m;
      sources[s].world.matrices =
          bk_actor_pose_world(r->poses[i], &sources[s].world.floats);
    }
    r->actors[i] = bk_actor_render_create(
        renderer, store, i == 2 ? "bk3_03" : "bk3_08", m,
        i ? NULL : bk_ending_normal_assets_eyes(assets), e);
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
  r->registry = bk_scene_light_registry_create(sources, 2, e);
  r->bom = bk_bom_render_create(renderer, bk_ending_normal_assets_bom(assets),
                                r->actors, e);
  if (!r->registry || !r->bom)
    goto bad;
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
      count != bk_bom_assets_count(bk_ending_normal_assets_bom(r->assets)) ||
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
  BkActorForest *forest = bk_ending_normal_assets_forest(r->assets);
  if (bk_frame_tree_count(bk_actor_forest_tree(forest)) != r->capacity)
    return fail(e, "CPU registry changed before GPU retirement");
  if (!r->secondary_view && dispatch->objects[0]) {
    const BkFrameTree *tree = bk_actor_forest_tree(forest);
    uint32_t background_actor, background_frame, hidden;
    if (bk_frame_tree_parent(tree, r->roots[2]) != 0 ||
        bk_frame_tree_next(tree, r->roots[2]) != BK_FRAME_NONE ||
        !bk_actor_forest_binding(forest, r->roots[2], &background_actor,
                                 &background_frame) ||
        !bk_actor_pose_hidden(r->poses[2], background_frame, &hidden) || hidden)
      return fail(e,
                  "regular snapshot requires visible last-loaded background");
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
      if (!r->passes &&
          !prepare_actors(r, view, projection, material, disabled, count, e))
        return 0;
      size_t offset = 0;
      for (unsigned a = 0; a < (r->secondary_view ? 2u : 3u); a++) {
        size_t floats;
        const float *world = bk_actor_pose_world(r->poses[a], &floats);
        if (offset > r->world_floats || floats > r->world_floats - offset)
          return fail(e, "world snapshot capacity changed");
        if (!r->passes)
          memcpy(r->held_world + offset, world, floats * sizeof(float));
        else if (memcmp(r->held_world + offset, world, floats * sizeof(float)))
          return fail(e, "later flush changed prepared actor worlds");
        offset += floats;
      }
      BkActorRenderVisit *visits = r->visits + r->passes * r->capacity;
      for (uint32_t i = 0; i < n; i++) {
        uint32_t actor, frame;
        if (!walk[i].submit ||
            !bk_actor_forest_binding(forest, walk[i].node, &actor, &frame))
          continue;
        if (actor == 2 || actor == 3) {
          const BkModel *track = bk_actor_pose_model(
              bk_ending_normal_assets_pose(r->assets, actor));
          if (track->frames[frame].mesh_index != BK_MODEL_NONE)
            return fail(e, "camera geometry selected by unexpected root walk");
          continue;
        }
        unsigned index = actor == 4 ? 2 : actor;
        if (index >= 3 || !r->actors[index] || submitted >= r->capacity)
          return fail(e, "unexpected actor visit");
        visits[submitted++] = (BkActorRenderVisit){r->actors[index], frame};
      }
      BkLightWorld worlds[2];
      worlds[0].matrices = bk_actor_pose_world(r->poses[0], &worlds[0].floats);
      worlds[1].matrices = bk_actor_pose_world(r->poses[2], &worlds[1].floats);
      BkLighting snapshot;
      unsigned p = r->passes;
      if (!bk_scene_light_registry_values(r->registry, worlds, 2, &snapshot,
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
  unsigned i = actor == 4 ? 2 : actor;
  return r && i < 3 && actor != 2
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
  for (unsigned i = 0; i < 3; i++)
    if (a == r->actors[i]) {
      *actor = indices[i];
      *frame = f;
      *submesh = mesh;
      return 1;
    }
  return 0;
}
