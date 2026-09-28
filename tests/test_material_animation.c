#include "model/material_animation.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void word(unsigned char *p, uint32_t x) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (unsigned char)(x >> (i * 8));
}
static void number(unsigned char *p, float x) {
  uint32_t b;
  memcpy(&b, &x, 4);
  word(p, b);
}
int main(void) {
  unsigned char raw[72 + 16 + 3 * 72] = {0}, saved[sizeof(raw)];
  word(raw + 68, 1);
  word(raw + 72, 17);
  word(raw + 80, 3);
  const unsigned time[] = {10, 0, 10};
  for (unsigned k = 0; k < 3; k++) {
    unsigned char *p = raw + 88 + k * 72;
    word(p, time[k]);
    for (unsigned j = 0; j < 17; j++)
      number(p + 4 + j * 4, (float)(k + j) * .1f);
  }
  memcpy(saved, raw, sizeof(raw));
  BkModelChunk chunk = {.tag = "MATA", .size = sizeof(raw)};
  BkModelMaterial mat = {
      .name = "material", .id = 17, .diffuse = {1, 1, 1, 1}, .unknown = 73};
  BkModelFrame frame = {.parent_index = BK_MODEL_NONE,
                        .mesh_index = BK_MODEL_NONE};
  BkModel m = {.source = raw,
               .source_size = sizeof(raw),
               .chunks = &chunk,
               .chunk_count = 1,
               .materials = &mat,
               .material_count = 1,
               .frames = &frame,
               .frame_count = 1};
  char error[256];
  BkMaterialAnimation *a = bk_material_animation_create(&m, error);
  assert(a);
  assert(bk_material_animation_track(a, 0)->key_count == 2);
  assert(bk_material_animation_key(a, 0, 0)->values.diffuse[0] == .1f);
  assert(bk_material_animation_key(a, 0, 1)->values.diffuse[0] == .2f);
  BkMaterialPose *pose = bk_material_pose_create(&m, error);
  assert(pose);
  assert(bk_material_animation_sample(a, 0, pose, error));
  assert(bk_material_pose_material(pose, 0)->diffuse[0] == 1);
  assert(bk_material_animation_sample(a, 5, pose, error));
  const BkModelMaterial *v = bk_material_pose_material(pose, 0);
  assert(fabsf(v->diffuse[0] - .15f) < 1e-7f && v->ambient[0] == 1 &&
         v->ambient[3] == 0);
  assert(v->specular[3] == 0 && v->emissive[3] == 0 && v->unknown == 73);
  BkModelMaterial before = *v;
  assert(!bk_material_animation_sample(a, NAN, pose, error));
  assert(!bk_material_animation_sample(a, -1, pose, error));
  assert(!bk_material_animation_sample(a, 2147483648.f, pose, error));
  assert(!memcmp(&before, bk_material_pose_material(pose, 0), sizeof(before)));
  assert(bk_material_animation_time(a) == 5);
  BkMaterialValuesEdit edits[2] = {
      {.index = 0, .id = 17, .values = {.diffuse = {0, 0, 0, 1}}},
      {.index = 1, .id = 17}};
  assert(!bk_material_pose_values(pose, edits, 2, error));
  assert(!memcmp(&before, bk_material_pose_material(pose, 0), sizeof(before)));
  assert(bk_material_pose_values(pose, edits, 1, error));
  assert(bk_material_animation_sample(a, 5, pose, error));
  assert(bk_material_pose_material(pose, 0)->diffuse[0] == 0);
  mat.id = 18;
  assert(bk_material_animation_sample(a, 6, pose, error)); /* Pose owns ID17. */
  BkMaterialPose *other = bk_material_pose_create(&m, error);
  assert(other);
  assert(!bk_material_animation_sample(a, 7, other, error) &&
         bk_material_animation_time(a) == 6);
  bk_material_pose_destroy(other);
  mat.id = 17;
  memset(raw, 0xa5, sizeof(raw)); /* Keys own their source. */
  assert(bk_material_animation_restore(a, pose, error));
  assert(bk_material_pose_material(pose, 0)->diffuse[0] == .1f);
  bk_material_animation_destroy(a);
  bk_material_pose_destroy(pose);
  for (unsigned n = 0; n < sizeof(raw); n++) {
    memcpy(raw, saved, sizeof(raw));
    chunk.size = n;
    assert(!bk_material_animation_create(&m, error));
  }
  chunk.size = sizeof(raw);
  uint32_t random = 0x42f5f8;
  for (unsigned n = 0; n < 5000; n++) {
    memcpy(raw, saved, sizeof(raw));
    for (unsigned j = 0; j < 1 + n % 7; j++) {
      random ^= random << 13;
      random ^= random >> 17;
      random ^= random << 5;
      raw[random % sizeof(raw)] ^= (unsigned char)(random >> 24);
    }
    a = bk_material_animation_create(&m, error);
    if (a) {
      pose = bk_material_pose_create(&m, error);
      assert(pose);
      bk_material_animation_sample(a, 123.5f, pose, error);
      bk_material_animation_restore(a, pose, error);
      bk_material_pose_destroy(pose);
      bk_material_animation_destroy(a);
    }
  }
  puts("PASS MATA key normalization, cache, values identity/atomicity, "
       "ownership, truncation and5000 mutations");
  return 0;
}
