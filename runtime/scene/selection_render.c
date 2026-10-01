#include "scene/selection_render.h"
#include "scene/avi_texture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkSelectionRender {
  BkRenderer *renderer;
  BkSelectionWorld *world;
  BkSelectionActorAssets *body;
  BkActorRender *actors[2];
  BkActorRenderBatch *batches[2];
  BkLightSet *lights[2];
  BkAviTexture *movie;
  int uncensored;
  BkActorRenderVisit *visits;
  uint32_t capacity;
  int ready;
};
static int fail(char error[256], const char *why) {
  snprintf(error, 256, "selection render: %s", why);
  return 0;
}
void bk_selection_render_destroy(BkSelectionRender *r) {
  if (!r)
    return;
  for (unsigned i = 0; i < 2; ++i) {
    bk_actor_render_batch_destroy(r->batches[i]);
    bk_actor_render_destroy(r->actors[i]);
    bk_light_set_destroy(r->renderer, r->lights[i]);
  }
  bk_avi_texture_destroy(r->movie);
  free(r->visits);
  free(r);
}
BkSelectionRender *bk_selection_render_create(BkRenderer *renderer,
                                              BkResourceStore *store,
                                              BkSelectionWorld *world,
                                              char error[256]) {
  if (world && !(bk_resources_patch_flags(store) & BK_PATCH_UNCENSORED) &&
      bk_selection_actor_assets_needs_movie(bk_selection_world_body(world))) {
    fail(error, "alternate61 requires an explicit movie clock (create_at)");
    return NULL;
  }
  return bk_selection_render_create_at(renderer, store, world, 0, error);
}
BkSelectionRender *bk_selection_render_create_at(BkRenderer *renderer,
                                                 BkResourceStore *store,
                                                 BkSelectionWorld *world,
                                                 int32_t clock_ms,
                                                 char error[256]) {
  if (!renderer || !store || !world) {
    fail(error, "missing services");
    return NULL;
  }
  BkSelectionActorAssets *body = bk_selection_world_body(world);
  BkSelectionRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(error, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  r->world = world;
  r->body = body;
  r->capacity = bk_frame_tree_count(
      bk_actor_forest_tree(bk_selection_world_forest(world)));
  r->visits = calloc(r->capacity, sizeof(*r->visits));
  if (!r->visits) {
    fail(error, "visit allocation failed");
    goto bad;
  }
  for (unsigned i = 0; i < 2; ++i) {
    const BkModel *m =
        bk_actor_pose_model(bk_selection_world_pose(world, 2 + i));
    uint64_t capacity = 0;
    for (unsigned f = 0; f < m->frame_count; ++f)
      if (m->frames[f].mesh_index != BK_MODEL_NONE)
        capacity += m->meshes[m->frames[f].mesh_index].submesh_count;
    if (!capacity || capacity > UINT32_MAX) {
      fail(error, "invalid geometry capacity");
      goto bad;
    }
    r->actors[i] = bk_actor_render_create(
        renderer, store, i ? "bk3_01" : "bk3_03", m,
        i ? bk_selection_actor_assets_eyes(body) : NULL, error);
    r->batches[i] =
        bk_actor_render_batch_create(renderer, (uint32_t)capacity, error);
    r->lights[i] = bk_light_set_create(renderer, &(BkLighting){0}, error);
    if (!r->actors[i] || !r->batches[i] || !r->lights[i])
      goto bad;
  }
  r->uncensored = !!(bk_resources_patch_flags(store) & BK_PATCH_UNCENSORED);
  if (!r->uncensored && bk_selection_actor_assets_needs_movie(body)) {
    const BkModel *m =
        bk_actor_pose_model(bk_selection_world_pose(world, BK_SELECTION_BODY));
    uint32_t target = BK_MODEL_NONE;
    for (uint32_t i = 0; i < m->texture_count; i++) {
      /* This binding is an exact authored resource identifier. */
      if (strcmp(m->textures[i].filename, "D_moza.bmp"))
        continue;
      if (target != BK_MODEL_NONE) {
        fail(error, "ambiguous movie texture target");
        goto bad;
      }
      target = i;
    }
    if (target == BK_MODEL_NONE) {
      fail(error, "missing authored D_moza.bmp movie target");
      goto bad;
    }
    BkBlob blob = {0};
    if (bk_resources_read(store, "bk3_18", "poi.avi", &blob, error) !=
        BK_RESOURCE_OK)
      goto bad;
    r->movie =
        bk_avi_texture_create(renderer, blob.data, blob.size, clock_ms, error);
    bk_blob_free(&blob);
    if (!r->movie ||
        !bk_actor_render_texture_surface(r->actors[1], target,
                                         bk_avi_texture_gpu(r->movie), error))
      goto bad;
  }
  return r;
bad:
  bk_selection_render_destroy(r);
  return NULL;
}
int bk_selection_render_movie_step(BkSelectionRender *r, int32_t now,
                                   int32_t restart, char error[256]) {
  if (r && r->uncensored && r->body == bk_selection_world_body(r->world)) return 1;
  if (!r || !r->movie || r->body != bk_selection_world_body(r->world))
    return fail(error, "missing movie or retired body");
  return bk_avi_texture_step(r->movie, now, restart, error);
}
uint32_t bk_selection_render_movie_frame(const BkSelectionRender *r) {
  return r ? bk_avi_texture_frame(r->movie) : UINT32_MAX;
}
int bk_selection_render_prepare(BkSelectionRender *r,
                                const BkMenuCamera *camera, const BkFog *fog,
                                char error[256]) {
  if (!r)
    return fail(error, "missing owner");
  r->ready = 0;
  if (!camera || !fog || !bk_fog_validate(fog, error) ||
      r->body != bk_selection_world_body(r->world))
    return fail(error, "invalid state or body replaced before GPU retirement");
  BkActorForest *forest = bk_selection_world_forest(r->world);
  const float *anchor = bk_actor_forest_world(forest, 1);
  for (unsigned i = 0; i < 16; ++i)
    /* Multiplication by the identity parent can change signed zero. */
    if (camera->pose.world[i] != anchor[i])
      return fail(error, "camera does not match installed world anchor");
  float projection[16];
  if (!bk_camera_projection(projection,
                            &(BkCameraLens){camera->fov, .75f, .5f, 126384}))
    return fail(error, "invalid camera lens");
  BkLightingPass pass;
  if (!bk_selection_world_pass(r->world, &pass, error))
    return 0;
  unsigned index = 0, flushes = 0;
  BkSceneLighting *lights = bk_selection_world_lighting(r->world);
  for (unsigned c = 0; c < pass.count; ++c) {
    const BkLightingCommand *command = &pass.commands[c];
    if (command->kind == BK_PASS_AMBIENT ||
        command->kind == BK_PASS_LIGHT_ENABLE) {
      if (!bk_scene_lighting_command(lights, command))
        return fail(error, "invalid light command");
    } else if (command->kind == BK_PASS_OBJECT) {
      if (index >= 2 || index != flushes ||
          command->target != bk_selection_world_root(r->world, 2 + index))
        return fail(error, "unexpected selection root order");
      const BkFrameVisit *walk;
      uint32_t count, submitted = 0;
      if (!bk_actor_forest_draw(forest, command->target, &walk, &count, error))
        return 0;
      for (uint32_t v = 0; v < count; ++v) {
        uint32_t actor, frame;
        if (!walk[v].submit ||
            !bk_actor_forest_binding(forest, walk[v].node, &actor, &frame))
          continue;
        if (actor != index + 2 || submitted >= r->capacity)
          return fail(error, "unexpected cross-root submission");
        r->visits[submitted++] = (BkActorRenderVisit){r->actors[index], frame};
      }
      const float *view = bk_actor_forest_view(forest);
      BkActorPose *body = bk_selection_world_pose(r->world, BK_SELECTION_BODY);
      BkLighting snapshot;
      if (!bk_scene_lighting_values(lights, bk_actor_pose_frame(body, 0),
                                    bk_actor_pose_model(body)->frame_count * 16,
                                    &snapshot, error) ||
          !bk_light_set_update(r->renderer, r->lights[index], &snapshot,
                               error) ||
          !bk_light_set_view(r->renderer, r->lights[index],
                             camera->pose.world + 12, error) ||
          !bk_light_set_fog(r->renderer, r->lights[index], fog, view, error) ||
          !bk_actor_render_prepare(
              r->actors[index], bk_selection_world_pose(r->world, index + 2),
              NULL, index ? bk_selection_actor_assets_face(r->body) : NULL,
              view, projection, error) ||
          !bk_actor_render_batch_prepare_visits(r->batches[index], r->visits,
                                                submitted, error))
        return 0;
      ++index;
    } else if (command->kind == BK_PASS_FLUSH) {
      if (flushes >= index)
        return fail(error, "unexpected empty flush");
      ++flushes;
    } else
      return fail(error, "unsupported selection draw event");
  }
  if (index != 2 || flushes != 2)
    return fail(error, "incomplete draw plan");
  r->ready = 1;
  return 1;
}
int bk_selection_render_draw(BkSelectionRender *r, char error[256]) {
  if (!r || !r->ready)
    return fail(error, "no prepared frame");
  return bk_actor_render_batch_draw(r->batches[0], r->lights[0], error) &&
         bk_actor_render_batch_draw(r->batches[1], r->lights[1], error);
}
int bk_selection_render_stats(const BkSelectionRender *r, unsigned object,
                              BkActorRenderStats *out) {
  return r && object < 2 && bk_actor_render_stats(r->actors[object], out);
}
