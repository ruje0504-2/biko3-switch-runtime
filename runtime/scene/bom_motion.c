#include "scene/bom_assets.h"
#include <stdio.h>
#include <string.h>
typedef struct {
  uint32_t frames[2];
  BkNodeReference values[2], before[2];
  BkNodeReference *nodes[2];
  const float *refs[2];
} Nodes;
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "BOM control: %s", why);
  return 0;
}
static int prepare(BkBomAssets *a, unsigned first, unsigned count, Nodes *out,
                   char e[256]) {
  if (!a || !count || count > 2 || first >= bk_bom_assets_count(a) ||
      count > bk_bom_assets_count(a) - first)
    return fail(e, "invalid binding range");
  memset(out, 0, sizeof(*out));
  BkActorPose *pose = bk_bom_assets_actor(a, 0);
  for (unsigned i = 0; i < count; i++) {
    const BkBomAssetBinding *b = bk_bom_assets_binding(a, first + i);
    out->frames[i] = b->reference;
    if (b->reference == BK_MODEL_NONE || b->parent == BK_MODEL_NONE)
      continue;
    unsigned j;
    for (j = 0; j < i; j++)
      if (out->nodes[j] && out->frames[j] == b->reference)
        break;
    if (j < i)
      out->nodes[i] = out->nodes[j];
    else {
      if (!bk_actor_pose_node_reference(pose, b->reference, out->values + i, e))
        return 0;
      out->before[i] = out->values[i];
      out->nodes[i] = out->values + i;
    }
  }
  for (unsigned i = 0; i < count; i++) {
    const BkBomAssetBinding *b = bk_bom_assets_binding(a, first + i);
    out->refs[i] = bk_actor_pose_frame(pose, b->parent);
    for (unsigned j = 0; j < count; j++)
      if (out->nodes[j] && out->frames[j] == b->parent) {
        out->refs[i] = out->nodes[j]->world;
        break;
      }
  }
  return 1;
}
static int commit(BkBomAssets *a, Nodes *n, unsigned count, char e[256]) {
  for (unsigned i = 0; i < count; i++)
    if (n->nodes[i] == n->values + i &&
        memcmp(n->values + i, n->before + i, sizeof(*n->values)))
      if (!bk_actor_pose_commit_reference(bk_bom_assets_actor(a, 0),
                                          n->frames[i], n->values + i, e))
        return 0;
  return 1;
}
int bk_bom_assets_manual(BkBomAssets *a, BkBomManual *s, BkBomManualKind kind,
                         unsigned binding, int32_t dx, int32_t dy, float radius,
                         float degrees, int32_t flip, char e[256]) {
  if ((unsigned)kind > BK_BOM_MANUAL_SECOND)
    return fail(e, "direct controller needs explicit nodes");
  Nodes n;
  if (!prepare(a, binding, 1, &n, e))
    return 0;
  if (!bk_bom_manual_step(s, kind, n.nodes[0], n.refs[0], dx, dy, radius,
                          degrees, flip, e))
    return 0;
  return commit(a, &n, 1, e);
}
int bk_bom_assets_return_single(BkBomAssets *a, BkBomReturn *s,
                                unsigned binding, const float scene[16],
                                float degrees, int32_t flip, uint32_t ms,
                                int32_t reset, int *done, char e[256]) {
  Nodes n;
  if (!prepare(a, binding, 1, &n, e))
    return 0;
  if (!bk_bom_return_single(s, n.nodes[0], n.refs[0], scene, degrees, flip, ms,
                            reset, done, e))
    return 0;
  return commit(a, &n, 1, e);
}
int bk_bom_assets_return_multiple(BkBomAssets *a, BkBomReturn *s,
                                  unsigned count, const float scene[16],
                                  float degrees, int32_t flip, uint32_t ms,
                                  int *done, char e[256]) {
  Nodes n;
  if (!prepare(a, 0, count, &n, e))
    return 0;
  int ok = bk_bom_return_multiple(s, n.nodes, n.refs, count, scene, degrees,
                                  flip, ms, done, e);
  char saved[256];
  if (!ok)
    memcpy(saved, e, 256);
  if (!commit(a, &n, count, e))
    return 0;
  if (!ok)
    memcpy(e, saved, 256);
  return ok;
}
int bk_bom_assets_follow_references(BkBomAssets *a, char e[256]) {
  if (!a)
    return fail(e, "missing assets");
  for (unsigned i = 0; i < bk_bom_assets_count(a); i++) {
    const BkBomAssetBinding *b = bk_bom_assets_binding(a, i);
    if (b->reference == BK_MODEL_NONE)
      return fail(e, "missing required follow reference");
    if (!bk_actor_pose_align_reference(
            bk_bom_assets_actor(a, 1), b->child,
            bk_actor_pose_frame(bk_bom_assets_actor(a, 0), b->reference), e))
      return 0;
  }
  return 1;
}
