#include "model/clip.h"
#include "model/morph_group.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void word(unsigned char *p, uint32_t x) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (unsigned char)(x >> (i * 8));
}
static void number(unsigned char *p, float x) {
  uint32_t bits;
  memcpy(&bits, &x, 4);
  word(p, bits);
}
static void clock_tests(void) {
  unsigned char raw[0x5190] = {0};
  memcpy(raw, "frame.x", 8);
  memcpy(raw + 256, "frame.x", 8);
  unsigned char *d = raw + 512 + 0x190;
  word(raw + 512 + 0x164, 1);
  word(d + 0x50, 2);
  number(d + 0x54, 10);
  number(d + 0x58, 20);
  number(d + 0x60, 10);
  number(d + 0x5c, 4.95f);
  word(d + 0x68, 3);
  word(d + 0x6c, 1);
  word(d + 0x70, 1);
  word(d + 0x74, 1);
  word(d + 156 + 0x50, 10);
  number(d + 156 + 0x54, 30);
  number(d + 156 + 0x58, 40);
  char error[256];
  BkClipSet *s = bk_clip_set_decode(raw, sizeof(raw), error);
  assert(s);
  BkClipPlayer *p = bk_clip_player_create_loaded(s, error);
  assert(p);
  BkClipSample out;
  BkClipState before, after;
  int32_t interval, counter;
  assert(bk_clip_advance_frame(p, &out, error) && out.from == 10 && !out.blend);
  assert(bk_clip_frame_clock(p, 0, &interval, &counter) && interval == 3 &&
         counter == 2);
  assert(bk_clip_advance(p, 0, &out, error));
  assert(bk_clip_frame_clock(p, 0, &interval, &counter) && counter == 0);
  assert(bk_clip_state(p, &before));
  assert(bk_clip_request_active(p, 0, error));
  assert(bk_clip_request_active(p, 127, error)); /* Empty: no operation. */
  assert(bk_clip_state(p, &after) && !memcmp(&before, &after, sizeof(before)));
  for (unsigned i = 0; i < 6; i++)
    assert(bk_clip_advance_frame(p, &out, error));
  assert(bk_clip_state(p, &after) && after.slot == 1 && after.requested == 0 &&
         !after.ended && after.source == 30 && out.from < 20 && out.from > 19);
  /* Active comparison differs from the usual requested-action latch. */
  assert(bk_clip_request_active(p, 1, error));
  assert(bk_clip_state(p, &before) && before.requested == 0);
  assert(bk_clip_request_active(p, 0, error));
  assert(bk_clip_state(p, &after) && after.slot == 0 && after.source == 10);
  assert(bk_clip_advance_frame(p, &out, error));
  assert(bk_clip_frame_clock(p, 0, &interval, &counter) && counter == 1);
  bk_clip_player_destroy(p);
  bk_clip_set_destroy(s);
  /* Native signed counter wrap; no implementation-defined signed addition. */
  word(d + 0x6c, INT32_MAX);
  s = bk_clip_set_decode(raw, sizeof(raw), error);
  p = bk_clip_player_create_loaded(s, error);
  assert(p && bk_clip_advance_frame(p, &out, error));
  assert(bk_clip_frame_clock(p, 0, &interval, &counter) &&
         counter == INT32_MIN);
  bk_clip_player_destroy(p);
  bk_clip_set_destroy(s);
  /* Invalid source evaluation must leave counter, clock and sample intact. */
  word(d + 0x68, 0);
  word(d + 0x6c, 17);
  number(d + 0x5c, 3e38f);
  s = bk_clip_set_decode(raw, sizeof(raw), error);
  p = bk_clip_player_create_loaded(s, error);
  assert(p && bk_clip_state(p, &before));
  BkClipSample saved = out;
  assert(!bk_clip_advance_frame(p, &out, error));
  assert(!memcmp(&saved, &out, sizeof(out)));
  assert(bk_clip_state(p, &after) && !memcmp(&before, &after, sizeof(before)));
  assert(bk_clip_frame_clock(p, 0, &interval, &counter) && counter == 17);
  bk_clip_player_destroy(p);
  bk_clip_set_destroy(s);
}
static void group_tests(void) {
  unsigned char raw[72 + 2 * (24 + 2 * 136)] = {0};
  word(raw + 68, 2);
  size_t offset = 72;
  BkModelVertex base[2] = {0};
  BkModelSubmesh meshes[2] = {0};
  for (unsigned t = 0; t < 2; t++) {
    meshes[t].id = t + 1;
    meshes[t].vertex_count = 1;
    meshes[t].vertices = base + t;
    word(raw + offset, t + 1);
    word(raw + offset + 20, 2);
    offset += 24;
    for (unsigned k = 0; k < 2; k++) {
      number(raw + offset, (float)k * 10);
      word(raw + offset + 8, 1);
      number(raw + offset + 12, (float)(t * 100 + k * 10));
      for (unsigned j = 0; j < 16; j++)
        number(raw + offset + 72 + j * 4, j % 5 == 0 ? 1 : 0);
      offset += 136;
    }
  }
  BkModelChunk chunk = {.offset = 0, .size = sizeof(raw)};
  memcpy(chunk.tag, "MORP", 4);
  BkModel model = {.source = raw,
                   .source_size = sizeof(raw),
                   .chunks = &chunk,
                   .chunk_count = 1,
                   .submeshes = meshes,
                   .submesh_count = 2};
  char error[256];
  BkMorphGroup *g = bk_morph_group_create(&model, error);
  assert(g && bk_morph_group_tracks(g) == 2);
  /* All keys and base vertices are owned; destroy the source contents. */
  memset(raw, 0xa5, sizeof(raw));
  memset(base, 0xa5, sizeof(base));
  uint32_t mask[2] = {UINT32_MAX, 0};
  assert(bk_morph_group_sample(g, 0, NULL, 0, error));
  assert(bk_morph_mesh_vertices(bk_morph_group_mesh(g, 1))[0].position[0] == 0);
  assert(bk_morph_group_sample(g, 10, mask, 2, error));
  assert(bk_morph_mesh_vertices(bk_morph_group_mesh(g, 0))[0].position[0] ==
         10);
  assert(bk_morph_group_sample(g, 10, NULL, 0, error));
  assert(bk_morph_mesh_vertices(bk_morph_group_mesh(g, 1))[0].position[0] == 0);
  assert(bk_morph_group_sample(g, 20, NULL, 0, error));
  assert(bk_morph_mesh_vertices(bk_morph_group_mesh(g, 0))[0].position[0] == 0);
  assert(bk_morph_mesh_vertices(bk_morph_group_mesh(g, 1))[0].position[0] ==
         100);
  mask[0] = mask[1] = 0;
  assert(bk_morph_group_sample(g, 10, mask, 2, error));
  assert(bk_morph_group_time(g) == 10);
  assert(!bk_morph_group_sample(g, 11, mask, 1, error));
  assert(!bk_morph_group_sample(g, NAN, NULL, 0, error));
  assert(bk_morph_group_time(g) == 10);
  assert(bk_morph_mesh_vertices(bk_morph_group_mesh(g, 1))[0].position[0] ==
         100);
  assert(bk_morph_group_blend(g, 0, 10, .5f, NULL, 0, error));
  assert(bk_morph_group_time(g) == 10);
  float blended =
      bk_morph_mesh_vertices(bk_morph_group_mesh(g, 1))[0].position[0];
  assert(blended == 105);
  assert(bk_morph_group_sample(g, 10, NULL, 0, error));
  assert(bk_morph_mesh_vertices(bk_morph_group_mesh(g, 1))[0].position[0] ==
         blended);
  assert(!bk_morph_group_blend(g, 0, 10, NAN, NULL, 0, error));
  assert(!bk_morph_group_blend(g, 0, 10, .5f, mask, 1, error));
  assert(bk_morph_group_time(g) == 10 &&
         bk_morph_mesh_vertices(bk_morph_group_mesh(g, 1))[0].position[0] ==
             blended);
  /* A repeated blend is NOT suppressed by the preceding plain cache. */
  assert(bk_morph_group_blend(g, 0, 10, 0, NULL, 0, error));
  assert(bk_morph_mesh_vertices(bk_morph_group_mesh(g, 1))[0].position[0] ==
         100);
  bk_morph_group_destroy(g);
}
int main(void) {
  clock_tests();
  group_tests();
  puts("PASS fixed clock, active requests, wrap/rejection, MORP "
       "ownership/masks/cache");
  return 0;
}
