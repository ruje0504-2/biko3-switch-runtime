#include "scene/bom_render.h"
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkBomRender *owner;
  unsigned actor;
} Callback;
struct BkBomRender {
  BkRenderer *renderer;
  const BkBomAssets *assets;
  BkActorRender *actors[2];
  Callback callbacks[2];
  BkVertexTransfer *transfers[4];
  int32_t groups[4], disabled[4];
  uint32_t count;
  uint64_t revisions[2];
  int attached[2], ready;
};
static int callback(void *context, uint32_t submesh, char error[256]) {
  Callback *cb = context;
  BkBomRender *r = cb->owner;
  if (!r->ready || bk_actor_render_revision(r->actors[0]) != r->revisions[0] ||
      bk_actor_render_revision(r->actors[1]) != r->revisions[1]) {
    snprintf(error, 256, "stale BOM render snapshot");
    return 0;
  }
  int32_t group = -1;
  for (uint32_t i = 0; i < r->count; i++) {
    const BkBomAssetBinding *b = bk_bom_assets_binding(r->assets, i);
    const BkBomAssetMesh *m = bk_bom_assets_mesh(r->assets, b->target);
    if (m && m->actor == cb->actor && m->submesh == submesh) {
      group = r->groups[i];
      break;
    }
  }
  if (group < 0)
    return 1;
  BkVertexTransfer *batch[4];
  unsigned count = 0;
  for (uint32_t i = 0; i < r->count; i++)
    if (r->groups[i] == group && r->disabled[i] != 1 && r->transfers[i])
      batch[count++] = r->transfers[i];
  return bk_vertex_transfers_apply(r->renderer, batch, count, error);
}
void bk_bom_render_destroy(BkBomRender *r) {
  if (!r)
    return;
  char error[256];
  for (unsigned i = 0; i < 2; i++)
    if (r->attached[i])
      bk_actor_render_mesh_callback(r->actors[i], NULL, r->callbacks + i,
                                    error);
  for (unsigned i = 0; i < 4; i++)
    bk_vertex_transfer_destroy(r->transfers[i]);
  free(r);
}
BkBomRender *bk_bom_render_create(BkRenderer *renderer,
                                  const BkBomAssets *assets,
                                  BkActorRender *const actors[2],
                                  char error[256]) {
  if (!renderer || !assets || !actors || !actors[0] || !actors[1] ||
      actors[0] == actors[1]) {
    snprintf(error, 256, "invalid BOM render owners");
    return NULL;
  }
  BkBomRender *r = calloc(1, sizeof(*r));
  BkVertexPair *pairs = NULL;
  if (!r) {
    snprintf(error, 256, "BOM render allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  r->assets = assets;
  r->count = bk_bom_assets_count(assets);
  memcpy(r->actors, actors, sizeof(r->actors));
  for (uint32_t i = 0; i < r->count; i++) {
    BkBomDeformBinding plan;
    const uint32_t *sources;
    size_t count;
    if (!bk_bom_assets_plan(assets, i, &plan) ||
        !bk_bom_assets_mapping(assets, i, &r->groups[i], &sources, &count) ||
        count != plan.count)
      goto invalid;
    if (!count)
      continue;
    const BkBomAssetMesh *source = bk_bom_assets_mesh(assets, plan.source),
                         *target = bk_bom_assets_mesh(assets, plan.target);
    if (!source || !target || source->actor >= 2 || target->actor >= 2)
      goto invalid;
    BkGpuMesh *s = bk_actor_render_mesh(
        actors[source->actor],
        bk_actor_pose_model(bk_bom_assets_actor(assets, source->actor)),
        source->submesh);
    BkGpuMesh *t = bk_actor_render_mesh(
        actors[target->actor],
        bk_actor_pose_model(bk_bom_assets_actor(assets, target->actor)),
        target->submesh);
    pairs = malloc(count * sizeof(*pairs));
    if (!pairs) {
      snprintf(error, 256, "BOM pair allocation failed");
      goto bad;
    }
    for (size_t j = 0; j < count; j++)
      pairs[j] = (BkVertexPair){sources[j], plan.indices[j]};
    r->transfers[i] = bk_vertex_transfer_create(renderer, s, t, pairs,
                                                (unsigned)count, error);
    free(pairs);
    pairs = NULL;
    if (!r->transfers[i])
      goto bad;
  }
  for (unsigned i = 0; i < 2; i++) {
    r->callbacks[i] = (Callback){r, i};
    if (!bk_actor_render_mesh_callback(actors[i], callback, r->callbacks + i,
                                       error))
      goto bad;
    r->attached[i] = 1;
  }
  return r;
invalid:
  snprintf(error, 256, "inconsistent BOM GPU mapping/model");
bad:
  free(pairs);
  bk_bom_render_destroy(r);
  return NULL;
}
int bk_bom_render_prepare(BkBomRender *r, const int32_t *disabled, size_t count,
                          char error[256]) {
  if (!r) {
    snprintf(error, 256, "missing BOM renderer");
    return 0;
  }
  r->ready = 0;
  if (count != r->count || (count && !disabled)) {
    snprintf(error, 256, "invalid BOM disable flags");
    return 0;
  }
  for (unsigned i = 0; i < 2; i++) {
    r->revisions[i] = bk_actor_render_revision(r->actors[i]);
    if (!r->revisions[i]) {
      snprintf(error, 256, "unprepared BOM actor");
      return 0;
    }
  }
  for (uint32_t i = 0; i < r->count; i++)
    if (r->transfers[i]) {
      const BkBomAssetBinding *b = bk_bom_assets_binding(r->assets, i);
      const BkBomAssetMesh *m = bk_bom_assets_mesh(r->assets, b->source);
      if (!bk_vertex_transfer_world(
              r->transfers[i],
              bk_actor_pose_frame(bk_bom_assets_actor(r->assets, m->actor),
                                  m->frame),
              error))
        return 0;
    }
  if (count)
    memcpy(r->disabled, disabled, count * sizeof(*disabled));
  r->ready = 1;
  return 1;
}
