#include "scene/special_render.h"
#include "scene/avi_texture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkSpecialRender {
  BkRenderer *renderer;
  BkSpecialWorld *world;
  BkActorRender *actor;
  BkActorRenderBatch *batch;
  BkLightSet *lights;
  BkAviTexture *movie;
  int uncensored;
  BkActorRenderVisit *visits;
  uint32_t capacity;
  int ready;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "special render: %s", why);
  return 0;
}
void bk_special_render_destroy(BkSpecialRender *r) {
  if (!r) return;
  bk_actor_render_batch_destroy(r->batch);
  bk_actor_render_destroy(r->actor);
  bk_light_set_destroy(r->renderer, r->lights);
  bk_avi_texture_destroy(r->movie);
  free(r->visits);
  free(r);
}
static BkSpecialRender *create(BkRenderer *renderer,
    BkResourceStore *store, BkSpecialWorld *world, int32_t clock_ms,
    BkAviClockRead read, void *context, char e[256]) {
  if (!renderer || !store || !world) {
    fail(e, "missing services"); return NULL;
  }
  BkSpecialRender *r = calloc(1, sizeof(*r));
  if (!r) { fail(e, "allocation failed"); return NULL; }
  r->renderer = renderer; r->world = world;
  const BkModel *m = bk_actor_pose_model(bk_special_world_pose(world, BK_SPECIAL_WORLD_BODY));
  r->capacity = bk_frame_tree_count(bk_actor_forest_tree(bk_special_world_forest(world)));
  r->visits = calloc(r->capacity, sizeof(*r->visits));
  if (!r->visits) { fail(e, "visit allocation failed"); goto bad; }
  uint64_t capacity = 0;
  for (uint32_t f = 0; f < m->frame_count; ++f)
    if (m->frames[f].mesh_index != BK_MODEL_NONE)
      capacity += m->meshes[m->frames[f].mesh_index].submesh_count;
  if (!capacity || capacity > UINT32_MAX) { fail(e, "invalid mesh capacity"); goto bad; }
  r->actor = bk_actor_render_create(renderer, store, "bk3_14", m,
                                    bk_special_world_eyes(world), e);
  r->batch = bk_actor_render_batch_create(renderer, (uint32_t)capacity, e);
  r->lights = bk_light_set_create(renderer, &(BkLighting){0}, e);
  if (!r->actor || !r->batch || !r->lights) goto bad;
  r->uncensored = !!(bk_resources_patch_flags(store) & BK_PATCH_UNCENSORED);
  if (!r->uncensored && bk_special_world_needs_movie(world)) {
    uint32_t target = BK_MODEL_NONE;
    for (uint32_t i = 0; i < m->texture_count; ++i)
      if (!strcmp(m->textures[i].filename, "D_moza.bmp")) {
        if (target != BK_MODEL_NONE) { fail(e, "ambiguous movie surface"); goto bad; }
        target = i;
      }
    if (target == BK_MODEL_NONE) { fail(e, "missing D_moza.bmp surface"); goto bad; }
    BkBlob raw = {0};
    if (bk_resources_read(store, "bk3_18", "poi.avi", &raw, e) != BK_RESOURCE_OK) goto bad;
    r->movie = read ? bk_avi_texture_create_clock(renderer, raw.data, raw.size, read, context, e)
                    : bk_avi_texture_create(renderer, raw.data, raw.size, clock_ms, e);
    bk_blob_free(&raw);
    if (!r->movie || !bk_actor_render_texture_surface(r->actor, target,
                                              bk_avi_texture_gpu(r->movie), e)) goto bad;
  }
  return r;
bad:
  bk_special_render_destroy(r); return NULL;
}
BkSpecialRender *bk_special_render_create(BkRenderer *r, BkResourceStore *store,
    BkSpecialWorld *world, int32_t ms, char e[256]) {
  return create(r, store, world, ms, NULL, NULL, e);
}
BkSpecialRender *bk_special_render_create_clock(BkRenderer *r, BkResourceStore *store,
    BkSpecialWorld *world, BkAviClockRead read, void *context, char e[256]) {
  if (!read) { fail(e, "missing live movie clock"); return NULL; }
  return create(r, store, world, 0, read, context, e);
}
int bk_special_render_movie_step(BkSpecialRender *r, int32_t now, int32_t restart, char e[256]) {
  if (r && r->uncensored) return 1;
  return r && r->movie ? bk_avi_texture_step(r->movie, now, restart, e)
                        : fail(e, "movie is not loaded");
}
int bk_special_render_movie_poll(BkSpecialRender *r, BkAviClockRead read, void *context, char e[256]) {
  if (r && r->uncensored) return 1;
  return r && r->movie ? bk_avi_texture_poll(r->movie, read, context, e)
                        : fail(e, "movie is not loaded");
}
uint32_t bk_special_render_movie_frame(const BkSpecialRender *r) {
  return r ? bk_avi_texture_frame(r->movie) : UINT32_MAX;
}
const BkImage *bk_special_render_movie_image(const BkSpecialRender *r) {
  return r ? bk_avi_texture_image(r->movie) : NULL;
}
BkActorRender *bk_special_render_actor(BkSpecialRender *r) { return r ? r->actor : NULL; }
uint32_t bk_special_render_submissions(const BkSpecialRender *r) {
  return r && r->ready ? bk_actor_render_batch_count(r->batch) : 0;
}
int bk_special_render_prepare(BkSpecialRender *r, const BkMenuCamera *camera,
                               const BkFog *fog, char e[256]) {
  if (!r) return fail(e, "missing owner");
  r->ready = 0;
  if (!camera || !fog || !bk_fog_validate(fog, e)) return fail(e, "invalid camera/fog");
  BkActorForest *forest = bk_special_world_forest(r->world);
  const float *anchor = bk_actor_forest_world(forest, 1);
  for (unsigned i = 0; i < 16; ++i)
    if (camera->pose.world[i] != anchor[i]) return fail(e, "camera anchor mismatch");
  float projection[16];
  if (!bk_camera_projection(projection, &(BkCameraLens){camera->fov, .75f, .5f, 126384}))
    return fail(e, "invalid camera lens");
  BkLightingPass pass;
  if (!bk_special_world_pass(r->world, bk_special_world_group(r->world), &pass, e)) return 0;
  BkSceneLighting *lights = bk_special_world_lighting(r->world);
  BkActorPose *body = bk_special_world_pose(r->world, BK_SPECIAL_WORLD_BODY);
  unsigned objects = 0, flushes = 0;
  for (uint32_t i = 0; i < pass.count; ++i) {
    const BkLightingCommand *c = &pass.commands[i];
    if (c->kind == BK_PASS_AMBIENT || c->kind == BK_PASS_LIGHT_ENABLE) {
      if (!bk_scene_lighting_command(lights, c)) return fail(e, "invalid light command");
    } else if (c->kind == BK_PASS_OBJECT) {
      if (objects || c->target != bk_special_world_root(r->world, BK_SPECIAL_WORLD_BODY))
        return fail(e, "unexpected body root");
      const BkFrameVisit *walk; uint32_t count, submitted = 0;
      if (!bk_actor_forest_draw(forest, c->target, &walk, &count, e)) return 0;
      for (uint32_t j = 0; j < count; ++j) {
        uint32_t actor, frame;
        if (!walk[j].submit || !bk_actor_forest_binding(forest, walk[j].node, &actor, &frame)) continue;
        if (actor != BK_SPECIAL_WORLD_BODY || submitted >= r->capacity)
          return fail(e, "unexpected cross-root submission");
        r->visits[submitted++] = (BkActorRenderVisit){r->actor, frame};
      }
      const float *view = bk_actor_forest_view(forest);
      BkLighting snapshot;
      if (!bk_scene_lighting_values(lights, bk_actor_pose_frame(body, 0),
            (size_t)bk_actor_pose_model(body)->frame_count * 16, &snapshot, e) ||
          !bk_light_set_update(r->renderer, r->lights, &snapshot, e) ||
          !bk_light_set_view(r->renderer, r->lights, camera->pose.world + 12, e) ||
          !bk_light_set_fog(r->renderer, r->lights, fog, view, e) ||
          !bk_actor_render_prepare(r->actor, body, NULL, bk_special_world_face(r->world), view, projection, e) ||
          !bk_actor_render_batch_prepare_visits(r->batch, r->visits, submitted, e)) return 0;
      ++objects;
    } else if (c->kind == BK_PASS_FLUSH) {
      if (flushes >= objects) return fail(e, "empty flush");
      ++flushes;
    } else return fail(e, "unsupported draw event");
  }
  if (objects != 1 || flushes != 1) return fail(e, "incomplete draw plan");
  r->ready = 1; return 1;
}
int bk_special_render_draw(BkSpecialRender *r, char e[256]) {
  return r && r->ready ? bk_actor_render_batch_draw(r->batch, r->lights, e)
                        : fail(e, "no prepared frame");
}
int bk_special_render_stats(const BkSpecialRender *r, BkActorRenderStats *out) {
  return r && bk_actor_render_stats(r->actor, out);
}
