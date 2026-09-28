#include "scene/npc_material.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static float alpha(BkMaterialPose *pose, uint32_t i) {
  return bk_material_pose_material(pose, i)->diffuse[3];
}
int main(void) {
  BkModelMaterial materials[4] = {
      {.name = "S2_magane_renzu", .diffuse = {1, 1, 1, .5f}},
      {.name = "G_reiko_kami_u", .diffuse = {1, 1, 1, .7f}},
      {.name = "NULL", .diffuse = {1, 1, 1, .8f}},
      {.name = "unreferenced", .diffuse = {1, 1, 1, 1.00001f}}};
  BkModelSubmesh submeshes[4] = {{.material_index = 0},
                                 {.material_index = 1},
                                 {.material_index = 2},
                                 {.material_index = 0}};
  BkModelMesh meshes[3] = {{.first_submesh = 0, .submesh_count = 2},
                           {.first_submesh = 2, .submesh_count = 1},
                           {.first_submesh = 3, .submesh_count = 1}};
  BkModelFrame frames[3] = {{.parent_index = BK_MODEL_NONE, .mesh_index = 0},
                            {.parent_index = 0, .mesh_index = 1},
                            {.parent_index = 1, .mesh_index = 2}};
  BkModel model = {.frames = frames,
                   .frame_count = 3,
                   .meshes = meshes,
                   .mesh_count = 3,
                   .submeshes = submeshes,
                   .submesh_count = 4,
                   .materials = materials,
                   .material_count = 4};
  char error[256];
  BkMaterialPose *p = bk_material_pose_create(&model, error);
  BkMaterialPose *independent = bk_material_pose_create(&model, error);
  assert(p && independent && alpha(p, 3) == 0);
  assert(!bk_material_pose_material(p, 4));
  uint32_t marks[4] = {2, 2, 2, 2};
  float opacity = .75f;
  assert(bk_npc_fade_apply(&opacity, 0, 1, 0, p, 0, marks, error));
  assert(opacity == .75f && alpha(p, 0) == 2 && alpha(p, 1) == .99f &&
         alpha(p, 2) == 0 && alpha(p, 3) == 0);
  /* Child marker and root mesh share a material; the ordered final edit
   * must reach both. A second actor and source model remain independent. */
  assert(alpha(independent, 0) == .5f && materials[0].diffuse[3] == .5f);
  BkMaterialState gpu;
  assert(bk_material_state(bk_material_pose_material(p, 0), 0, &gpu, error));
  assert(gpu.blend == BK_MATERIAL_ADDITIVE && gpu.diffuse[3] == 1);
  assert(bk_npc_fade_apply(&opacity, 1, 1, 1, p, 0, marks, error));
  assert(opacity == 0 && alpha(p, 0) == .2f && alpha(p, 1) == .99f);
  BkMaterialAlphaRule rules[] = {{"NULL", -.5f}, {"NULL", 1}};
  BkMaterialAlphaEdit edit = {0, 7, rules, 2};
  assert(bk_material_pose_alpha(p, &edit, 1, error));
  assert(alpha(p, 0) == 2 && alpha(p, 2) == -.5f);
  assert(bk_material_state(bk_material_pose_material(p, 2), 0, &gpu, error));
  assert(gpu.blend == BK_MATERIAL_INVERSE_COLOR);
  BkModelMaterial saved[4];
  for (unsigned i = 0; i < 4; i++)
    saved[i] = *bk_material_pose_material(p, i);
  BkMaterialAlphaEdit invalid[] = {{0, 0, NULL, 0}, {3, 1, NULL, 0}};
  assert(!bk_material_pose_alpha(p, invalid, 2, error));
  opacity = .5f;
  marks[3] = 3;
  assert(!bk_npc_fade_apply(&opacity, 0, 0, 1, p, 0, marks, error));
  assert(opacity == .5f);
  for (unsigned i = 0; i < 4; i++)
    assert(
        !memcmp(&saved[i], bk_material_pose_material(p, i), sizeof(saved[i])));
  marks[3] = 2;
  const float invalid_seconds[] = {NAN, INFINITY, -1};
  for (unsigned i = 0; i < 3; i++)
    assert(!bk_npc_fade_apply(&opacity, 0, 0, invalid_seconds[i], p, 0, marks,
                              error));
  assert(bk_npc_fade_apply(&opacity, 255, 0, 100, p, 0, marks, error));
  assert(opacity == .5f && alpha(p, 2) == -.5f);
  opacity = 1;
  assert(bk_npc_fade_apply(&opacity, 0, 0, 100, p, 0, marks, error));
  assert(alpha(p, 2) == -.5f);
  bk_material_pose_destroy(p);
  bk_material_pose_destroy(independent);
  frames[0].parent_index = 2;
  assert(!bk_material_pose_create(&model, error));
  frames[0].parent_index = BK_MODEL_NONE;
  submeshes[3].material_index = BK_MODEL_NONE;
  assert(!bk_material_pose_create(&model, error));
  submeshes[3].material_index = 0;
  memset(materials[3].name, 'x', sizeof(materials[3].name));
  assert(!bk_material_pose_create(&model, error));
  assert(strstr(error, "unterminated"));
  puts(
      "PASS: material sharing/isolation, ordered subtree alpha, fade policies, "
      "encoded blending, atomic rollback and malformed input cleanup");
  return 0;
}
