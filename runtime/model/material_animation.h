#ifndef BK_MODEL_MATERIAL_ANIMATION_H
#define BK_MODEL_MATERIAL_ANIMATION_H
#include "model/material_pose.h"
typedef struct BkMaterialAnimation BkMaterialAnimation;
typedef struct {
  uint32_t material_index, material_id, loop, key_count;
} BkMaterialTrack;
typedef struct {
  float time;
  BkMaterialValues values;
} BkMaterialKey;
/* 419c86 MATA load, including the initial material key at time0, ordered key
 * replacement and signed integer file times converted to float. Owns all data;
 * source model may be released. Missing/ambiguous targets fail explicitly. */
BkMaterialAnimation *bk_material_animation_create(const BkModel *,
                                                  char error[256]);
void bk_material_animation_destroy(BkMaterialAnimation *);
uint32_t bk_material_animation_tracks(const BkMaterialAnimation *);
const BkMaterialTrack *bk_material_animation_track(const BkMaterialAnimation *,
                                                   uint32_t);
const BkMaterialKey *bk_material_animation_key(const BkMaterialAnimation *,
                                               uint32_t, uint32_t);
/* 4300e0/42f5f8: same-time group cache (initial0), integer modulo(last+1)
 * when loop!=0, exact full key or native RGB/alpha/power interpolation.
 * Nonlooping beyond-last extrapolates last->first, not a clamp. Native
 * interpolated specular/emissive W are uninitialized and unused by shading;
 * portable policy defines those two fields as0. Exact keys preserve them.
 * Material batch and group time commit together; no allocation per sample. */
int bk_material_animation_sample(BkMaterialAnimation *, float time,
                                 BkMaterialPose *, char error[256]);
float bk_material_animation_time(const BkMaterialAnimation *);
/* Explicit 42f3b0 restoration before destroying a bound instance: submit each
 * track's first key, which may have replaced the original MATE time0 value.
 * Does not reset the group clock. destroy alone never changes an external pose.
 */
int bk_material_animation_restore(BkMaterialAnimation *, BkMaterialPose *,
                                  char error[256]);
#endif
