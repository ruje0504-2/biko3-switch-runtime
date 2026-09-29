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
  const BkBomDualAssets *dual;
  BkActorRender *actors[3];
  Callback callbacks[3];
  BkVertexTransfer *transfers[BK_BOM_SET_CAPACITY];
  int32_t groups[BK_BOM_SET_CAPACITY], disabled[BK_BOM_SET_CAPACITY];
  uint32_t count, actor_count;
  uint64_t revisions[3];
  int attached[3], ready;
};
static const BkBomAssetBinding *binding(const BkBomRender *r, uint32_t index) {
  if (!r->dual) return bk_bom_assets_binding(r->assets, index);
  const BkBomDualBinding *b = bk_bom_dual_assets_binding(r->dual, index);
  return b ? &b->nodes : NULL;
}
static const BkBomAssetMesh *mesh(const BkBomRender *r, uint32_t index) {
  return r->dual ? bk_bom_dual_assets_mesh(r->dual, index)
                 : bk_bom_assets_mesh(r->assets, index);
}
static BkActorPose *pose(const BkBomRender *r, unsigned actor) {
  return r->dual ? bk_bom_dual_assets_actor(r->dual, actor)
                 : bk_bom_assets_actor(r->assets, actor);
}
static int plan(const BkBomRender *r, uint32_t index, BkBomDeformBinding *out) {
  return r->dual ? bk_bom_dual_assets_plan(r->dual, index, out)
                 : bk_bom_assets_plan(r->assets, index, out);
}
static int mapping(const BkBomRender *r, uint32_t index, int32_t *group,
                     const uint32_t **sources, size_t *count) {
  return r->dual ? bk_bom_dual_assets_mapping(r->dual, index, group, sources, count)
                 : bk_bom_assets_mapping(r->assets, index, group, sources, count);
}
static int callback(void *context, uint32_t submesh, char error[256]) {
  Callback *cb = context;
  BkBomRender *r = cb->owner;
  if (!r->ready) {
    snprintf(error, 256, "stale BOM render snapshot");
    return 0;
  }
  for (unsigned i = 0; i < r->actor_count; ++i)
    if (r->actors[i] && bk_actor_render_revision(r->actors[i]) != r->revisions[i]) {
      snprintf(error, 256, "stale BOM render snapshot");
      return 0;
    }
  int32_t group = -1;
  for (uint32_t i = 0; i < r->count; i++) {
    const BkBomAssetBinding *b = binding(r, i);
    const BkBomAssetMesh *m = b ? mesh(r, b->target) : NULL;
    if (m && m->actor == cb->actor && m->submesh == submesh) {
      group = r->groups[i];
      break;
    }
  }
  if (group < 0)
    return 1;
  BkVertexTransfer *batch[BK_BOM_SET_CAPACITY];
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
  for (unsigned i = 0; i < r->actor_count; i++)
    if (r->attached[i])
      bk_actor_render_mesh_callback(r->actors[i], NULL, r->callbacks + i,
                                    error);
  for (unsigned i = 0; i < BK_BOM_SET_CAPACITY; i++)
    bk_vertex_transfer_destroy(r->transfers[i]);
  free(r);
}
static BkBomRender *create(BkRenderer *renderer, const BkBomAssets *assets,
                            const BkBomDualAssets *dual,
                            BkActorRender *const *actors, char error[256]) {
  if (!renderer || (!assets == !dual) || !actors || !actors[0]) {
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
  r->dual = dual;
  r->actor_count = dual ? 3 : 2;
  r->count = dual ? bk_bom_dual_assets_count(dual) : bk_bom_assets_count(assets);
  if (r->count > BK_BOM_SET_CAPACITY) goto invalid;
  memcpy(r->actors, actors, r->actor_count * sizeof(*actors));
  for (unsigned i = 0; i < r->actor_count; ++i) {
    if (!!actors[i] != !!pose(r, i)) goto invalid;
    for (unsigned j = 0; j < i; ++j)
      if (actors[i] && actors[i] == actors[j]) goto invalid;
  }
  for (uint32_t i = 0; i < r->count; i++) {
    BkBomDeformBinding transfer;
    const uint32_t *sources;
    size_t count;
    if (!plan(r, i, &transfer) ||
        !mapping(r, i, &r->groups[i], &sources, &count) ||
        count != transfer.count || count > UINT32_MAX || count > SIZE_MAX / sizeof(*pairs))
      goto invalid;
    if (!count)
      continue;
    const BkBomAssetMesh *source = mesh(r, transfer.source),
                         *target = mesh(r, transfer.target);
    if (!source || !target || source->actor >= r->actor_count ||
        target->actor >= r->actor_count || !actors[source->actor] || !actors[target->actor])
      goto invalid;
    BkGpuMesh *s = bk_actor_render_mesh(
        actors[source->actor],
        bk_actor_pose_model(pose(r, source->actor)),
        source->submesh);
    BkGpuMesh *t = bk_actor_render_mesh(
        actors[target->actor],
        bk_actor_pose_model(pose(r, target->actor)),
        target->submesh);
    pairs = malloc(count * sizeof(*pairs));
    if (!pairs) {
      snprintf(error, 256, "BOM pair allocation failed");
      goto bad;
    }
    for (size_t j = 0; j < count; j++)
      pairs[j] = (BkVertexPair){sources[j], transfer.indices[j]};
    r->transfers[i] = bk_vertex_transfer_create(renderer, s, t, pairs,
                                                (unsigned)count, error);
    free(pairs);
    pairs = NULL;
    if (!r->transfers[i])
      goto bad;
  }
  for (unsigned i = 0; i < r->actor_count; i++) {
    if (!actors[i]) continue;
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
BkBomRender *bk_bom_render_create(BkRenderer *renderer, const BkBomAssets *assets,
                                  BkActorRender *const actors[2], char error[256]) {
  return create(renderer, assets, NULL, actors, error);
}
BkBomRender *bk_bom_render_create_dual(BkRenderer *renderer, const BkBomDualAssets *assets,
                                       BkActorRender *const actors[3], char error[256]) {
  return create(renderer, NULL, assets, actors, error);
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
  for (unsigned i = 0; i < r->actor_count; i++) {
    if (!r->actors[i]) continue;
    r->revisions[i] = bk_actor_render_revision(r->actors[i]);
    if (!r->revisions[i]) {
      snprintf(error, 256, "unprepared BOM actor");
      return 0;
    }
  }
  for (uint32_t i = 0; i < r->count; i++)
    if (r->transfers[i]) {
      const BkBomAssetBinding *b = binding(r, i);
      const BkBomAssetMesh *m = b ? mesh(r, b->source) : NULL;
      if (!m) {
        snprintf(error, 256, "missing live BOM source binding");
        return 0;
      }
      if (!bk_vertex_transfer_world(
              r->transfers[i],
              bk_actor_pose_frame(pose(r, m->actor), m->frame),
              error))
        return 0;
    }
  if (count)
    memcpy(r->disabled, disabled, count * sizeof(*disabled));
  r->ready = 1;
  return 1;
}
