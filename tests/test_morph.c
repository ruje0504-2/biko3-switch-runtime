#include "model/morph.h"
#include "model/morph_pose.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void word(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (i * 8));
}
static void number(uint8_t *p, float n) {
  uint32_t bits;
  memcpy(&bits, &n, 4);
  word(p, bits);
}
int main(void) {
  uint8_t raw[72 + 24 + 2 * (12 + 60 + 64)] = {0};
  word(raw + 68, 1);
  word(raw + 72, 123);
  word(raw + 92, 2);
  for (unsigned k = 0; k < 2; k++) {
    uint8_t *p = raw + 96 + k * 136;
    number(p, (float)k);
    word(p + 8, 1);
    for (unsigned j = 0; j < 15; j++)
      number(p + 12 + j * 4, (float)(k + j));
    for (unsigned j = 0; j < 4; j++)
      number(p + 72 + j * 20, 1);
  }
  BkModelSubmesh submesh = {.id = 123, .vertex_count = 1};
  BkModelChunk chunk = {.tag = "MORP", .size = sizeof(raw)};
  BkModel model = {.source = raw, .source_size = sizeof(raw), .chunks = &chunk,
                  .chunk_count = 1, .submeshes = &submesh, .submesh_count = 1};
  char error[256];
  BkModelMorph *m = bk_model_morph_create(&model, error);
  assert(m && bk_model_morph_count(m) == 1);
  assert(bk_model_morph_track(m, 0)->submesh == 0);
  assert(bk_model_morph_track(m, 0)->key_count == 2);
  const BkMorphKey *key = bk_model_morph_key(m, 0, 1);
  assert(key && key->vertices[0].uv[3][1] == 15 && key->matrix[15] == 1);
  assert(!bk_model_morph_track(m, 1) && !bk_model_morph_key(m, 0, 2));
  assert(!bk_model_morph_key(m, 1, 0) && !bk_model_morph_count(NULL));
  for (unsigned i = 0; i < sizeof(raw); i++) {
    chunk.size = i;
    assert(!bk_model_morph_create(&model, error));
  }
  chunk.size = sizeof(raw);
  word(raw + 144, UINT32_MAX); /* Unused UV1 exporter residue is opaque. */
  BkModelMorph *residue = bk_model_morph_create(&model, error);
  assert(residue);
  uint32_t bits;
  memcpy(&bits, &bk_model_morph_key(residue, 0, 0)->vertices[0].uv[1][0], 4);
  assert(bits == UINT32_MAX);
  bk_model_morph_destroy(residue);
  const unsigned offsets[] = {68, 72, 76, 92, 96, 104, 108, 168,
                               232, 240, 244, 304};
  for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
    uint8_t saved[4];
    memcpy(saved, raw + offsets[i], 4);
    word(raw + offsets[i], UINT32_MAX);
    assert(!bk_model_morph_create(&model, error));
    memcpy(raw + offsets[i], saved, 4);
  }
  number(raw + 232, 0);
  assert(!bk_model_morph_create(&model, error));
  number(raw + 232, 1);
  model.source_size--;
  assert(!bk_model_morph_create(&model, error));
  model.source_size++;
  BkModelVertex initial = {0};
  submesh.vertices = &initial;
  BkMorphMesh *mesh = bk_morph_mesh_create(&submesh, error);
  assert(mesh && bk_morph_mesh_count(mesh) == 1);
  BkMorphBinding *first = bk_morph_binding_create(m, 0, mesh, error);
  BkMorphBinding *second = bk_morph_binding_create(m, 0, mesh, error);
  assert(first && second);
  const uint16_t repeated[] = {0, 0}, invalid[] = {1};
  assert(bk_morph_binding_selection(first, 1, repeated, 2, error));
  assert(!bk_morph_binding_selection(first, 1, invalid, 1, error));
  BkMorphSample sample = {1, 0, 1, .5f};
  assert(bk_morph_binding_apply(first, &sample, error));
  const BkModelVertex *evaluated = bk_morph_mesh_vertices(mesh);
  /* Two visits in the second pass produce .75, not .5. The exact-to branch
   * leaves first-pass normals/beta unchanged but selects destination UV0. */
  assert(evaluated->position[0] == .75f && evaluated->normal[0] == 4 &&
         evaluated->beta == 3 && evaluated->uv[0][0] == 8);
  assert(bk_morph_binding_time(first) == 0);
  sample = (BkMorphSample){0, .25f, 0, 0};
  assert(bk_morph_binding_apply(second, &sample, error));
  evaluated = bk_morph_mesh_vertices(mesh);
  assert(evaluated->position[0] == .25f && evaluated->normal[0] == 4.25f &&
         evaluated->uv[0][0] == 7 && evaluated->beta == 3);
  assert(bk_morph_binding_time(second) == .25f &&
         bk_morph_binding_time(first) == 0);
  BkModelVertex saved_vertex = *evaluated;
  sample = (BkMorphSample){1, 0, NAN, .5f};
  assert(!bk_morph_binding_apply(second, &sample, error));
  assert(!memcmp(&saved_vertex, bk_morph_mesh_vertices(mesh), sizeof(saved_vertex)));
  BkMorphMesh *other = bk_morph_mesh_create(&submesh, error);
  BkMorphBinding *third = bk_morph_binding_create(m, 0, other, error);
  assert(other && third);
  BkMorphBinding *batch[] = {second, first, third};
  BkMorphSample samples[] = {{0, .125f, 0, 0}, {1, 0, 1, .5f}, {0, NAN, 0, 0}};
  float held_time = bk_morph_binding_time(second);
  assert(!bk_morph_bindings_apply(batch, samples, 3, error));
  assert(!memcmp(&saved_vertex, bk_morph_mesh_vertices(mesh), sizeof(saved_vertex)));
  assert(bk_morph_binding_time(second) == held_time &&
         !memcmp(&initial, bk_morph_mesh_vertices(other), sizeof(initial)));
  samples[2].from = .25f;
  assert(bk_morph_bindings_apply(batch, samples, 3, error));
  assert(bk_morph_mesh_vertices(mesh)->position[0] == .75f &&
         bk_morph_mesh_vertices(other)->position[0] == .25f &&
         bk_morph_binding_time(second) == .125f);
  const uint8_t vix[] = {0, 0, 0, 0, 0xff};
  assert(bk_morph_binding_vix(second, vix, sizeof(vix), error));
  assert(bk_morph_binding_vix(second, NULL, 0, error));
  assert(bk_morph_binding_apply(second, &samples[1], error));
  assert(bk_morph_mesh_vertices(mesh)->position[0] == .75f);
  assert(bk_morph_binding_vix(second, vix, 1, error));
  assert(bk_morph_binding_apply(second, &samples[0], error));
  assert(bk_morph_mesh_vertices(mesh)->position[0] == .75f);
  bk_morph_binding_destroy(third);
  bk_morph_mesh_destroy(other);
  /* Restore the held state for the remaining single-binding checks. */
  assert(bk_morph_binding_selection(second, 0, NULL, 0, error));
  sample = (BkMorphSample){0, .25f, 0, 0};
  assert(bk_morph_binding_apply(second, &sample, error));
  assert(bk_morph_binding_time(second) == .25f);
  assert(bk_morph_binding_selection(second, 1, NULL, 0, error));
  sample = (BkMorphSample){0, .75f, 0, 0};
  assert(bk_morph_binding_apply(second, &sample, error));
  assert(!memcmp(&saved_vertex, bk_morph_mesh_vertices(mesh), sizeof(saved_vertex)));
  assert(bk_morph_binding_time(second) == .75f);
  assert(bk_morph_binding_loop(second, 0, error));
  assert(!bk_morph_binding_loop(second, 2, error));
  assert(bk_morph_binding_selection(second, 0, NULL, 0, error));
  sample.from = 1.25f;
  assert(bk_morph_binding_apply(second, &sample, error));
  assert(bk_morph_mesh_vertices(mesh)->position[0] == 1);
  assert(bk_morph_binding_loop(second, 1, error));
  assert(bk_morph_binding_apply(second, &sample, error));
  assert(bk_morph_mesh_vertices(mesh)->position[0] == 0);
  saved_vertex = *bk_morph_mesh_vertices(mesh);
  sample.from = 1e30f;
  assert(!bk_morph_binding_apply(second, &sample, error));
  assert(!memcmp(&saved_vertex, bk_morph_mesh_vertices(mesh), sizeof(saved_vertex)));
  bk_morph_binding_destroy(first);
  bk_morph_binding_destroy(second);
  bk_morph_mesh_destroy(mesh);
  memset(raw, 0, sizeof(raw));
  assert(key->vertices[0].uv[3][1] == 15 && key->matrix[15] == 1);
  bk_model_morph_destroy(m);
  bk_model_morph_destroy(NULL);
  puts("PASS: MORP ownership, layout, truncated buffers, malformed late keys and cleanup");
  return 0;
}
