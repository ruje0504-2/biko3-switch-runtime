#ifndef BK_MODEL_MORPH_GROUP_H
#define BK_MODEL_MORPH_GROUP_H
#include "model/morph_pose.h"
typedef struct BkMorphGroup BkMorphGroup;
/* Original433a7d whole-model MORP sampling. Owns decoded keys, target meshes,
 * ordered bindings and scratch; model can be released after construction.
 * Target validation follows the MORP decoder. Requires a MORP chunk. */
BkMorphGroup *bk_morph_group_create(const BkModel *, char error[256]);
void bk_morph_group_destroy(BkMorphGroup *);
/* NULL mask/count0 submits every track. Otherwise mask_count must equal the
 * decoded track count; any nonzero value enables that track. The group last
 * time starts0; repeated time skips ALL submissions even if the mask changed.
 * All enabled tracks and group time commit together. No skinning, upload or
 * material animation. Follows original per-track looping. */
int bk_morph_group_sample(BkMorphGroup *, float source, const uint32_t *mask,
                          size_t mask_count, char error[256]);
/*433cbe ordered blend. Always submits enabled tracks, even at repeated
 * endpoints/weight; preserves group and per-track plain sample caches.
 * Same mask policy as sample; vertices commit together on success. */
int bk_morph_group_blend(BkMorphGroup *, float from, float to, float weight,
                         const uint32_t *mask, size_t mask_count,
                         char error[256]);
const BkMorphMesh *bk_morph_group_mesh(const BkMorphGroup *, uint32_t submesh);
uint32_t bk_morph_group_tracks(const BkMorphGroup *);
float bk_morph_group_time(const BkMorphGroup *);
#endif
