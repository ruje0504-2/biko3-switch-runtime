#ifndef BK_MODEL_MATERIAL_POSE_H
#define BK_MODEL_MATERIAL_POSE_H
#include "model/material.h"
/* Per-instance material values; the immutable decoded model is borrowed.
 * Shared material references inside one model remain shared by index. */
typedef struct BkMaterialPose BkMaterialPose;
typedef struct {
  float diffuse[4], ambient[4], specular[4], emissive[4], power;
} BkMaterialValues;
typedef struct {
  uint32_t index, id;
  BkMaterialValues values;
} BkMaterialValuesEdit;
/* Ordered complete 43041a values, with target identity validation and atomic
 * commit. Names, IDs and the opaque trailing MATE field are preserved. */
int bk_material_pose_values(BkMaterialPose *, const BkMaterialValuesEdit *,
                            size_t count, char error[256]);
typedef struct {
  const char *name;
  float alpha;
} BkMaterialAlphaRule;
typedef struct {
  uint32_t frame;
  float alpha;
  const BkMaterialAlphaRule *rules;
  size_t rule_count;
} BkMaterialAlphaEdit;
BkMaterialPose *bk_material_pose_create(const BkModel *model, char error[256]);
void bk_material_pose_destroy(BkMaterialPose *pose);
/* The returned pointer is valid until the next successful edit/destroy. */
const BkModelMaterial *bk_material_pose_material(const BkMaterialPose *pose,
                                                 uint32_t index);
/* Apply ordered subtree alpha edits, mirroring native frame/material
 * traversal. First exact case-sensitive material-name rule wins; otherwise
 * use edit.alpha. Each assignment uses original material setter ranges.
 * The complete batch is atomic, including failure after preceding edits.
 * This does not upload GPU state or change node visibility. */
int bk_material_pose_alpha(BkMaterialPose *pose,
                           const BkMaterialAlphaEdit *edits, size_t count,
                           char error[256]);
#endif
