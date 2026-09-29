#include "model/skin.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void word(uint8_t *p, uint32_t value) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(value >> (i * 8));
}
static void number(uint8_t *p, float value) {
  uint32_t bits;
  memcpy(&bits, &value, 4);
  word(p, bits);
}
int main(void) {
  uint8_t raw[72 + 80 + 2 * (8 + 4 * 32)] = {0};
  word(raw + 68, 1);
  word(raw + 72 + 68, 20);
  word(raw + 72 + 72, 100);
  word(raw + 72 + 76, 2);
  const float weights[2][4] = {{.001f, .6f, .999f, .2f},
                               {.5f, .3995f, .2f, .3f}};
  for (unsigned b = 0; b < 2; b++) {
    uint8_t *p = raw + 152 + b * 136;
    word(p, 20 + b);
    word(p + 4, 4);
    p += 8;
    for (unsigned i = 0; i < 4; i++) {
      number(p + i * 12, b ? 3 : 1);
      number(p + 48 + i * 12 + 4, 2);
      word(p + 96 + i * 4, i);
      number(p + 112 + i * 4, weights[b][i]);
    }
  }
  BkModelFrame frames[2] = {{.id = 20}, {.id = 21}};
  BkModelVertex vertices[5] = {0};
  vertices[4].position[0] = 999;
  vertices[4].beta = .7f;
  vertices[0].uv[0][0] = .3f;
  vertices[0].uv[1][0] = NAN;
  BkModelSubmesh sub = {.id = 100, .vertex_count = 5, .vertices = vertices};
  BkModelChunk chunk = {.tag = "ENVL", .size = sizeof(raw)};
  BkModel model = {.source = raw,
                   .source_size = sizeof(raw),
                   .chunks = &chunk,
                   .chunk_count = 1,
                   .frames = frames,
                   .frame_count = 2,
                   .submeshes = &sub,
                   .submesh_count = 1};
  char error[256];
  BkModelSkin *skin = bk_model_skin_create(&model, error);
  assert(skin && bk_model_skin_count(skin) == 1);
  assert(bk_model_skin_entry(skin, 0)->bone_count == 2);
  assert(bk_model_skin_bone(skin, 0, 1)->frame == 1);
  assert(!bk_model_skin_entry(skin, 1) && !bk_model_skin_bone(skin, 0, 2));
  for (unsigned i = 0; i < sizeof(raw); i++) {
    chunk.size = i;
    assert(!bk_model_skin_create(&model, error));
  }
  chunk.size = sizeof(raw);
  const unsigned offsets[] = {68,  140, 144, 148, 152, 156, 160, 208,
                              256, 272, 288, 292, 296, 344, 392, 408};
  for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
    uint8_t saved[4];
    memcpy(saved, raw + offsets[i], 4);
    word(raw + offsets[i], UINT32_MAX);
    assert(!bk_model_skin_create(&model, error));
    memcpy(raw + offsets[i], saved, 4);
  }
  BkSkinMesh *mesh = bk_skin_mesh_create(skin, 0, &sub, error);
  assert(mesh && bk_skin_mesh_count(mesh) == 5);
  memset(raw, 0, sizeof(raw)); /* Owned skin arrays remain usable. */
  float world[32] = {0};
  for (unsigned b = 0; b < 2; b++)
    for (unsigned d = 0; d < 4; d++)
      world[b * 16 + d * 5] = 1;
  world[16 + 12] = 10;
  world[16 + 5] = 3;
  assert(bk_skin_mesh_apply(mesh, world, 32, NULL, 0, error));
  const BkModelVertex *out = bk_skin_mesh_vertices(mesh);
  assert(out[0].position[0] == 6.5f && out[0].beta == .5f);
  assert(out[0].normal[1] == 3 && out[0].uv[0][0] == .3f &&
         isnan(out[0].uv[1][0]));
  assert(fabsf(out[1].position[0] - 5.8f) < 1e-6f && out[1].beta == 1);
  assert(fabsf(out[2].position[0] - 1.012f) < 1e-6f && out[2].beta == 1);
  assert(fabsf(out[3].position[0] - 4.1f) < 1e-6f && out[3].beta == .5f);
  assert(out[4].position[0] == 999 && out[4].beta == .7f);
  BkModelVertex saved[5];
  memcpy(saved, out, sizeof(saved));
  assert(bk_skin_mesh_apply(mesh, world, 32, out, 5, error));
  assert(!memcmp(saved, bk_skin_mesh_vertices(mesh), sizeof(saved)));
  world[28] = NAN;
  assert(!bk_skin_mesh_apply(mesh, world, 32, NULL, 0, error));
  assert(!memcmp(saved, bk_skin_mesh_vertices(mesh), sizeof(saved)));
  world[28] = 10;
  const float homogeneous[] = {nextafterf(1, 0), nextafterf(1, 2), 2, .5f, -2};
  const float expected[] = {6.5f, 6.5f, 3.25f, 13, -3.25f};
  for (unsigned i = 0; i < sizeof(homogeneous) / sizeof(homogeneous[0]); i++) {
    world[31] = homogeneous[i];
    assert(bk_skin_mesh_apply(mesh, world, 32, NULL, 0, error));
    out = bk_skin_mesh_vertices(mesh);
    assert(out[0].position[0] == expected[i] && out[0].normal[1] == 3);
    assert(out[4].position[0] == 999 && out[4].beta == .7f);
  }
  world[19] = .25f;
  world[31] = 1;
  assert(bk_skin_mesh_apply(mesh, world, 32, NULL, 0, error));
  out = bk_skin_mesh_vertices(mesh);
  assert(fabsf(out[0].position[0] - 26.0f / 7.0f) < 1e-6f);
  assert(out[0].normal[1] == 3);
  memcpy(saved, out, sizeof(saved));
  world[31] = -.75f; /* p.x=3 produces W=0 only on the second influence. */
  assert(!bk_skin_mesh_apply(mesh, world, 32, NULL, 0, error));
  assert(!memcmp(saved, bk_skin_mesh_vertices(mesh), sizeof(saved)));
  world[19] = 0;
  world[31] = 0;
  assert(!bk_skin_mesh_apply(mesh, world, 32, NULL, 0, error));
  assert(!memcmp(saved, bk_skin_mesh_vertices(mesh), sizeof(saved)));
  world[31] = 1;
  assert(!bk_skin_mesh_apply(mesh, world, 31, NULL, 0, error));
  assert(!bk_skin_mesh_apply(mesh, world, 32, vertices, 4, error));
  assert(!memcmp(saved, bk_skin_mesh_vertices(mesh), sizeof(saved)));
  bk_skin_mesh_destroy(mesh);
  bk_model_skin_destroy(skin);
  puts("PASS: owned ENVL, truncated/invalid arrays, ordered weight thresholds "
       "and atomic mesh updates");
  return 0;
}
