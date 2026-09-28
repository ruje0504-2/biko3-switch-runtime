#include "model/material_pose.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkMaterialPose {
  const BkModel *model;
  BkModelMaterial *materials, *pending;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "material pose: %s", message);
  return 0;
}
void bk_material_pose_destroy(BkMaterialPose *p) {
  if (!p)
    return;
  free(p->materials);
  free(p->pending);
  free(p);
}
BkMaterialPose *bk_material_pose_create(const BkModel *m, char error[256]) {
  if (!m || !m->frame_count || !m->frames || (m->mesh_count && !m->meshes) ||
      (m->submesh_count && !m->submeshes) ||
      (m->material_count && !m->materials)) {
    fail(error, "invalid model");
    return NULL;
  }
  for (uint32_t i = 0; i < m->frame_count; i++) {
    uint32_t ancestor = i, depth = 0;
    while (ancestor != BK_MODEL_NONE) {
      if (ancestor >= m->frame_count || depth++ >= m->frame_count) {
        fail(error, "invalid/cyclic frame ancestry");
        return NULL;
      }
      ancestor = m->frames[ancestor].parent_index;
    }
    uint32_t mesh = m->frames[i].mesh_index;
    if (mesh != BK_MODEL_NONE && mesh >= m->mesh_count) {
      fail(error, "invalid frame mesh");
      return NULL;
    }
  }
  for (uint32_t i = 0; i < m->mesh_count; i++) {
    const BkModelMesh *mesh = &m->meshes[i];
    if (mesh->first_submesh > m->submesh_count ||
        mesh->submesh_count > m->submesh_count - mesh->first_submesh) {
      fail(error, "invalid mesh children");
      return NULL;
    }
  }
  for (uint32_t i = 0; i < m->submesh_count; i++)
    if (m->submeshes[i].material_index >= m->material_count) {
      /* Native traversal dereferences even a missing material. Fail before
       * any mutation instead of inventing a default for that broken input. */
      fail(error, "missing submesh material");
      return NULL;
    }
  BkMaterialPose *p = calloc(1, sizeof(*p));
  if (!p) {
    fail(error, "allocation failed");
    return NULL;
  }
  p->model = m;
  size_t bytes = (size_t)m->material_count * sizeof(*p->materials);
  p->materials = malloc(bytes ? bytes : 1);
  p->pending = malloc(bytes ? bytes : 1);
  if (!p->materials || !p->pending) {
    fail(error, "allocation failed");
    goto bad;
  }
  if (bytes)
    memcpy(p->materials, m->materials, bytes);
  for (uint32_t i = 0; i < m->material_count; i++) {
    BkMaterialState state;
    if (!memchr(p->materials[i].name, 0, sizeof(p->materials[i].name))) {
      fail(error, "unterminated material name");
      goto bad;
    }
    if (!bk_material_state(&p->materials[i], 0, &state, error))
      goto bad;
    p->materials[i].diffuse[3] = state.encoded_alpha;
  }
  return p;
bad:
  bk_material_pose_destroy(p);
  return NULL;
}
const BkModelMaterial *bk_material_pose_material(const BkMaterialPose *p,
                                                 uint32_t index) {
  return p && index < p->model->material_count ? &p->materials[index] : NULL;
}
int bk_material_pose_values(BkMaterialPose *p,
                            const BkMaterialValuesEdit *edits, size_t count,
                            char error[256]) {
  if (!p || (count && !edits))
    return fail(error, "invalid value edits");
  if (!count)
    return 1;
  memcpy(p->pending, p->materials,
         (size_t)p->model->material_count * sizeof(*p->materials));
  for (size_t i = 0; i < count; i++) {
    const BkMaterialValuesEdit *edit = edits + i;
    if (edit->index >= p->model->material_count ||
        p->pending[edit->index].id != edit->id)
      return fail(error, "value edit target mismatch");
    BkModelMaterial *m = p->pending + edit->index;
    memcpy(m->diffuse, edit->values.diffuse, sizeof(m->diffuse));
    memcpy(m->ambient, edit->values.ambient, sizeof(m->ambient));
    memcpy(m->specular, edit->values.specular, sizeof(m->specular));
    memcpy(m->emissive, edit->values.emissive, sizeof(m->emissive));
    m->power = edit->values.power;
    BkMaterialState state;
    if (!bk_material_state(m, 0, &state, error))
      return 0;
    m->diffuse[3] = state.encoded_alpha;
  }
  BkModelMaterial *previous = p->materials;
  p->materials = p->pending;
  p->pending = previous;
  return 1;
}
int bk_material_pose_alpha(BkMaterialPose *p, const BkMaterialAlphaEdit *edits,
                           size_t count, char error[256]) {
  if (!p || (count && !edits))
    return fail(error, "invalid alpha edits");
  const BkModel *m = p->model;
  size_t bytes = (size_t)m->material_count * sizeof(*p->materials);
  memcpy(p->pending, p->materials, bytes);
  for (size_t i = 0; i < count; i++) {
    const BkMaterialAlphaEdit *edit = &edits[i];
    if (edit->frame >= m->frame_count || !isfinite(edit->alpha) ||
        (edit->rule_count && !edit->rules))
      return fail(error, "invalid subtree/alpha/rules");
    for (size_t j = 0; j < edit->rule_count; j++)
      if (!edit->rules[j].name || !isfinite(edit->rules[j].alpha))
        return fail(error, "invalid alpha rule");
    for (uint32_t f = 0; f < m->frame_count; f++) {
      uint32_t ancestor = f;
      while (ancestor != BK_MODEL_NONE && ancestor != edit->frame)
        ancestor = m->frames[ancestor].parent_index;
      if (ancestor == BK_MODEL_NONE || m->frames[f].mesh_index == BK_MODEL_NONE)
        continue;
      const BkModelMesh *mesh = &m->meshes[m->frames[f].mesh_index];
      for (uint32_t s = 0; s < mesh->submesh_count; s++) {
        uint32_t index = m->submeshes[mesh->first_submesh + s].material_index;
        BkModelMaterial *material = &p->pending[index];
        float alpha = edit->alpha;
        for (size_t r = 0; r < edit->rule_count; r++)
          if (!strcmp(material->name, edit->rules[r].name)) {
            alpha = edit->rules[r].alpha;
            break;
          }
        material->diffuse[3] = alpha;
        BkMaterialState state;
        if (!bk_material_state(material, 0, &state, error))
          return 0;
        material->diffuse[3] = state.encoded_alpha;
      }
    }
  }
  BkModelMaterial *old = p->materials;
  p->materials = p->pending;
  p->pending = old;
  return 1;
}
