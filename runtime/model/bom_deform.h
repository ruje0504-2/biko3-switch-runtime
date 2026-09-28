#ifndef BK_MODEL_BOM_DEFORM_H
#define BK_MODEL_BOM_DEFORM_H
#include "model/model.h"
/* One registry mesh per index. Caller owns mutable per-instance vertices;
 * source world is the mesh parent's published cache, not its new local pose.
 * Distinct indices must not overlap storage; source==target uses one index. */
typedef struct {
  BkModelVertex *vertices;
  uint32_t count;
  const float *world;
} BkBomMeshView;
typedef struct {
  uint32_t source, target; /* BK_MODEL_NONE preserves a native NULL binding. */
  const uint16_t *indices;
  size_t count;
} BkBomDeformBinding;
typedef struct BkBomDeform BkBomDeform;
/* 4aab5e: groups by target identity (including NULL), then maps each ordered
 * VIX target vertex to the nearest transformed source vertex. First ties win;
 * distance must be strictly below100000. Owns indices/maps/scratch, borrows
 * nothing after creation. No target vertices are changed here. */
BkBomDeform *bk_bom_deform_create(const BkBomMeshView *, size_t mesh_count,
                                  const BkBomDeformBinding *, size_t count,
                                  char error[256]);
void bk_bom_deform_destroy(BkBomDeform *);
int bk_bom_deform_mapping(const BkBomDeform *, size_t binding, int32_t *group,
                          const uint32_t **sources, size_t *count);
/* Borrow the validated, owned VIX plan until destroy. Together with mapping,
 * this supplies source/destination indices for a later GPU callback without
 * exposing mutable registry storage or reading vertices back from the GPU.
 * A NULL source/target yields count0, as no callback can consume its indices. */
int bk_bom_deform_plan(const BkBomDeform *, size_t binding,
                       BkBomDeformBinding *out);
/* 4aaebb: matching target selects its group; unknown target selects group0.
 * Apply all group bindings in order, skipping only disabled[i]==1. Vertices
 * and cached worlds are read fresh. Repeated indices and source/target alias
 * preserve sequential writes. Only position/normal XYZ change. Preallocated
 * scratch makes invalid geometry/overflow atomic; no per-draw allocation. */
int bk_bom_deform_draw(BkBomDeform *, uint32_t target, const BkBomMeshView *,
                       size_t mesh_count, const int32_t *disabled,
                       char error[256]);
#endif
