#include "model/model.h"
#include "resource/face_config.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void word(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (i * 8));
}
int main(void) {
  uint8_t bytes[1024] = {0};
  size_t size = 4, offsets[30];
  word(bytes, 7);
  const char *rows[30] = {"ignored.xan", "source.xan", "eyeL",    "eyeR",
                          "L.bmp",       "R.bmp",      "mesh",    "source",
                          "-",           "",           "ignored", "ignored",
                          "ignored",     "ignored",    "ignored"};
  for (unsigned i = 0; i < 30; i++) {
    const char *s = rows[i] ? rows[i] : "";
    size_t n = strlen(s);
    offsets[i] = size;
    word(bytes + size, (uint32_t)n + 3);
    memcpy(bytes + size + 4, s, n + 1);
    memcpy(bytes + size + 4 + n + 1, "XY", 2);
    size += n + 7;
  }
  BkFaceConfig config, before;
  char error[256];
  assert(bk_face_config_decode(bytes, size, &config, error));
  assert(config.texture_mode == 7 && config.counts[0] == 1 &&
         config.counts[1] == 0);
  assert(!strcmp(config.slots[0][0].selection, "-") &&
         !config.slots[0][1].source[0] &&
         !strcmp(config.eye_textures[1], "R.bmp"));
  before = config;
  for (size_t i = 0; i < size; i++) {
    assert(!bk_face_config_decode(bytes, i, &config, error));
    assert(!memcmp(&config, &before, sizeof(config)));
  }
  assert(bk_face_config_decode(bytes, size + 1, &config, error));
  for (unsigned i = 0; i < 30; i++) {
    uint8_t saved[4];
    memcpy(saved, bytes + offsets[i], 4);
    word(bytes + offsets[i], UINT32_MAX);
    assert(!bk_face_config_decode(bytes, size, &config, error));
    assert(!memcmp(&config, &before, sizeof(config)));
    word(bytes + offsets[i], 0);
    assert(!bk_face_config_decode(bytes, size, &config, error));
    memcpy(bytes + offsets[i], saved, 4);
  }
  memset(bytes + 8, 'A', strlen(rows[0]) + 3);
  assert(!bk_face_config_decode(bytes, size, &config, error));
  BkModelMesh parents[2] = {
      {.name = "Case", .submesh_count = 2},
      {.name = "Solo", .first_submesh = 2, .submesh_count = 1}};
  BkModelSubmesh subs[3] = {{.name = "ignored", .mesh_index = 0},
                            {.name = "Case_  1", .mesh_index = 0},
                            {.name = "unused", .mesh_index = 1}};
  BkModel model = {.meshes = parents,
                   .mesh_count = 2,
                   .submeshes = subs,
                   .submesh_count = 3};
  char name[256];
  uint32_t index = 99;
  assert(bk_model_submesh_name(&model, 1, "h01_80.x", name, error));
  assert(!strcmp(name, "Case_1@H01_80.X"));
  assert(bk_model_find_submesh(&model, "h01_80.x", name, &index, error) &&
         index == 1);
  assert(!bk_model_find_submesh(&model, "h01_80.x", "case_1@H01_80.X", &index,
                                error));
  assert(index == 1);
  assert(bk_model_submesh_name(&model, 2, "a.x", name, error));
  assert(!strcmp(name, "Solo@A.X"));
  assert(!bk_model_submesh_name(&model, 0, "dir/a.x", name, error));
  puts("PASS: FAM bounds/atomic decoding and native mesh naming");
  return 0;
}
