#ifndef BK_MODEL_SKIN_H
#define BK_MODEL_SKIN_H
#include "model/model.h"
typedef struct BkModelSkin BkModelSkin;
typedef struct {
  uint32_t index;
  float position[3], normal[3], weight;
} BkSkinInfluence;
typedef struct {
  uint32_t frame, count;
  const BkSkinInfluence *influences;
} BkSkinBone;
typedef struct {
  uint32_t frame, submesh, bone_count, vertex_count;
} BkSkinEntry;
/* Original ENVL loader0x41b454: group72, entry80, bone8, then separate
 * position/normal/index/weight arrays of count*(12+12+4+4) bytes.
 * All decoded arrays are owned; original model can be released. Bone and
 * entry order is significant. No normalization or weight sorting occurs. */
BkModelSkin *bk_model_skin_create(const BkModel *model, char error[256]);
void bk_model_skin_destroy(BkModelSkin *skin);
uint32_t bk_model_skin_count(const BkModelSkin *skin);
const BkSkinEntry *bk_model_skin_entry(const BkModelSkin *skin, uint32_t entry);
const BkSkinBone *bk_model_skin_bone(const BkModelSkin *skin, uint32_t entry,
                                     uint32_t bone);
typedef struct BkSkinMesh BkSkinMesh;
/* Mesh instance borrows skin and owns base/output/scratch vertices. */
BkSkinMesh *bk_skin_mesh_create(const BkModelSkin *skin, uint32_t entry,
                                const BkModelSubmesh *mesh, char error[256]);
void bk_skin_mesh_destroy(BkSkinMesh *mesh);
const BkModelVertex *bk_skin_mesh_vertices(const BkSkinMesh *mesh);
uint32_t bk_skin_mesh_count(const BkSkinMesh *mesh);
/* 0x410a0a clear indexed beta, then0x41075e ordered accumulation. Bone inputs
 * are cached/published WORLD matrices in model frame order, affine only.
 * Output positions are in WORLD space: draw with identity, as0x42afbb does.
 * Normals use each bone's linear3x3, not inverse-transpose or normalization.
 * Optional source supplies current UV/beta/unweighted vertices; NULL uses
 * owned base vertices and requires source_count0. It may alias current output.
 * Failure preserves output. Source MORP offsets are not implicitly skinned:
 * the original ENVL uses its own authored bone-relative positions/normals. */
int bk_skin_mesh_apply(BkSkinMesh *mesh, const float *world, size_t float_count,
                       const BkModelVertex *source, size_t source_count,
                       char error[256]);
#endif
