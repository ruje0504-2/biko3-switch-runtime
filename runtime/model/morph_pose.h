#ifndef BK_MODEL_MORPH_POSE_H
#define BK_MODEL_MORPH_POSE_H
#include "model/morph.h"
typedef struct BkMorphMesh BkMorphMesh;
typedef struct BkMorphBinding BkMorphBinding;
typedef struct {
  int blend;
  float from, to, weight;
} BkMorphSample;
/* Mutable CPU vertex instance. Multiple morph bindings may target this
 * same mesh (e.g. separate eye/mouth subsets); submission order matters. */
BkMorphMesh *bk_morph_mesh_create(const BkModelSubmesh *mesh, char error[256]);
void bk_morph_mesh_destroy(BkMorphMesh *mesh);
const BkModelVertex *bk_morph_mesh_vertices(const BkMorphMesh *mesh);
uint32_t bk_morph_mesh_count(const BkMorphMesh *mesh);
/* Borrows decoded keys and target; destroy binding before either. Target
 * vertex count must match every key. Native loop policy starts enabled. */
BkMorphBinding *bk_morph_binding_create(const BkModelMorph *morph,
                                        uint32_t track, BkMorphMesh *target,
                                        char error[256]);
void bk_morph_binding_destroy(BkMorphBinding *binding);
int bk_morph_binding_loop(BkMorphBinding *binding, int loop, char error[256]);
/* Copy raw ordered indices. Repeats are preserved: the second blend pass
 * can update a repeated vertex more than once. enabled0 applies all vertices,
 * enabled1/count0 applies none. Failure preserves the previous selection. */
int bk_morph_binding_selection(BkMorphBinding *binding, int enabled,
                                const uint16_t *indices, size_t count,
                                char error[256]);
/* Native VIX byte stream: uint16 LE, odd final byte ignored. Empty files
 * leave the current selection unchanged; positive byte count enables it
 * even when floor(size/2)==0. A missing resource must not call this. */
int bk_morph_binding_vix(BkMorphBinding *binding, const uint8_t *data,
                         size_t size, char error[256]);
/* 0x4316be/0x432642. Plain exact keys copy60bytes; interpolated keys only
 * change position, normal and nearest-key UV0. Blend first applies from,
 * then to: exact-to leaves normals from the first pass; interpolated-to
 * blends normals and uses its upper key's UV0 when weight>=.5.
 * No normalization, matrix transform, skinning or GPU upload is added.
 * Invalid/overflowing operations preserve target vertices and plain time. */
int bk_morph_binding_apply(BkMorphBinding *binding, const BkMorphSample *sample,
                            char error[256]);
/* Ordered batch, at most64 submissions. Shared meshes and repeated bindings
 * retain sequential semantics; all vertices and times commit together.
 * Uses existing scratch buffers, without per-frame heap allocations. */
int bk_morph_bindings_apply(BkMorphBinding *const *bindings,
                             const BkMorphSample *samples, size_t count,
                             char error[256]);
float bk_morph_binding_time(const BkMorphBinding *binding);
#endif
