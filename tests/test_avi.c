#ifdef NDEBUG
#undef NDEBUG
#endif
#include "media/avi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t data[2048];
static size_t used;
static void word(size_t at, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    data[at + i] = (uint8_t)(n >> (8 * i));
}
static size_t begin(const char *tag) {
  size_t at = used;
  memcpy(data + at, tag, 4);
  used += 8;
  return at;
}
static void end(size_t at) {
  word(at + 4, (uint32_t)(used - at - 8));
  if (used & 1)
    ++used;
}
static void add(const void *p, size_t size) {
  memcpy(data + used, p, size);
  used += size;
}
static void fixture(void) {
  memset(data, 0, sizeof(data));
  used = 0;
  size_t riff = begin("RIFF");
  add("AVI ", 4);
  size_t hdrl = begin("LIST");
  add("hdrl", 4);
  size_t avih = begin("avih");
  word(used, 33333);
  word(used + 16, 5);
  word(used + 24, 1);
  word(used + 32, 8);
  word(used + 36, 8);
  used += 56;
  end(avih);
  size_t strl = begin("LIST");
  add("strl", 4);
  size_t strh = begin("strh");
  memcpy(data + used, "vidsmsvc", 8);
  word(used + 20, 1);
  word(used + 24, 30);
  word(used + 32, 5);
  used += 56;
  end(strh);
  size_t strf = begin("strf");
  word(used, 40);
  word(used + 4, 8);
  word(used + 8, 8);
  data[used + 12] = 1;
  data[used + 14] = 16;
  memcpy(data + used + 16, "CRAM", 4);
  used += 40;
  end(strf);
  end(strl);
  end(hdrl);
  size_t junk = begin("JUNK");
  add("X", 1);
  end(junk);
  size_t movi = begin("LIST");
  add("movi", 4);
  size_t rec = begin("LIST");
  add("rec ", 4);
  const uint8_t key[] = {31, 128, 224, 131, 0, 252, 255, 255, 0, 0};
  const uint8_t delta[] = {0x5a, 0x5a, 1, 128, 2, 0,    3, 0, 4, 0,  5, 0, 6,
                           0,    7,    0, 8,   0, 0x33, 0, 9, 0, 10, 0, 2, 132};
  const uint8_t skip[] = {4, 132};
  const uint8_t bad[] = {0, 0, 1, 128, 2, 0};
  const void *packets[] = {key, delta, skip, bad, key};
  const size_t sizes[] = {sizeof(key), sizeof(delta), sizeof(skip), sizeof(bad),
                          sizeof(key)};
  size_t offsets[5];
  for (unsigned i = 0; i < 5; i++) {
    offsets[i] = begin(i ? "00dc" : "00db");
    add(packets[i], sizes[i]);
    end(offsets[i]);
  }
  end(rec);
  end(movi);
  size_t index = begin("idx1");
  for (unsigned i = 0; i < 5; i++) {
    memcpy(data + used, data + offsets[i], 4);
    word(used + 4, i == 0 || i == 4 ? 16 : 0);
    word(used + 8, (uint32_t)(offsets[i] - movi - 8));
    word(used + 12, (uint32_t)sizes[i]);
    used += 16;
  }
  end(index);
  end(riff);
}
int main(void) {
  char error[256];
  fixture();
  BkAvi *a = bk_avi_open(data, used, error);
  assert(a);
  BkAviDecoder *d = bk_avi_decoder_create(a, error);
  BkAviDecoder *independent = bk_avi_decoder_create(a, error);
  assert(d && independent && !bk_avi_decoder_pixels(d));
  assert(bk_avi_decoder_index(d) == UINT32_MAX);
  assert(bk_avi_decoder_frame(d, 0, error));
  const uint16_t *p = bk_avi_decoder_pixels(d);
  for (unsigned y = 0; y < 8; y++)
    for (unsigned x = 0; x < 8; x++)
      assert(p[y * 8 + x] ==
             (y < 4 ? (x < 4 ? 0x7c00 : 0x7fff) : (x < 4 ? 31 : 0x3e0)));
  assert(bk_avi_decoder_frame(d, 1, error));
  p = bk_avi_decoder_pixels(d);
  const unsigned expected[16] = {5, 6, 7, 8, 6, 5, 8, 7,
                                 1, 2, 3, 4, 2, 1, 4, 3};
  for (unsigned i = 0; i < 16; i++) {
    assert(p[(4 + i / 4) * 8 + i % 4] == expected[i]);
    assert(p[(4 + i / 4) * 8 + 4 + i % 4] ==
           (i / 4 >= 2 && i % 4 < 2 ? 9 : 10));
    assert(p[i / 4 * 8 + i % 4] == 0x7c00);
    assert(p[i / 4 * 8 + 4 + i % 4] == 0x7fff);
  }
  uint16_t before[64];
  memcpy(before, p, sizeof(before));
  assert(bk_avi_decoder_frame(d, 2, error));
  assert(!memcmp(before, bk_avi_decoder_pixels(d), sizeof(before)));
  const uint16_t *published = bk_avi_decoder_pixels(d);
  assert(!bk_avi_decoder_frame(d, 3, error));
  assert(bk_avi_decoder_index(d) == 2 && published == bk_avi_decoder_pixels(d));
  assert(!memcmp(before, published, sizeof(before)));
  assert(!bk_avi_decoder_frame(d, 5, error));
  assert(bk_avi_decoder_frame(d, 4, error)); /* seek past a corrupt chain */
  assert(bk_avi_decoder_frame(d, 1, error)); /* backwards seek */
  assert(!memcmp(before, bk_avi_decoder_pixels(d), sizeof(before)));
  assert(bk_avi_decoder_frame(independent, 2, error));
  assert(!memcmp(before, bk_avi_decoder_pixels(independent), sizeof(before)));
  memset(data, 255, used); /* source ownership */
  assert(bk_avi_decoder_frame(d, 4, error));
  bk_avi_decoder_destroy(independent);
  bk_avi_decoder_destroy(d);
  bk_avi_destroy(a);
  fixture();
  for (size_t size = 0; size < used; size++)
    assert(!bk_avi_open(data, size, error));
  a = bk_avi_open(data, sizeof(data), error); /* external PP padding */
  assert(a);
  bk_avi_destroy(a);
  uint8_t original[sizeof(data)];
  memcpy(original, data, sizeof(data));
  uint32_t random = 20260928;
  for (unsigned mutation = 0; mutation < 6000; mutation++) {
    memcpy(data, original, sizeof(data));
    random = random * 1664525 + 1013904223;
    data[random % used] ^= (uint8_t)((random >> 24) | 1);
    a = bk_avi_open(data, used, error);
    if (a) {
      d = bk_avi_decoder_create(a, error);
      assert(d);
      uint32_t frames = bk_avi_info(a)->frames;
      for (uint32_t i = 0; i < frames; i++)
        (void)bk_avi_decoder_frame(d, i, error);
      bk_avi_decoder_destroy(d);
      bk_avi_destroy(a);
    }
  }
  assert(!bk_avi_open(NULL, 0, error));
  assert(!bk_avi_decoder_create(NULL, error));
  assert(!bk_avi_decoder_frame(NULL, 0, error));
  bk_avi_decoder_destroy(NULL);
  bk_avi_destroy(NULL);
  puts("PASS AVI MSV1 block colors/orientation, skips, nested records, seeks, "
       "ownership, atomic failure, truncation and6000 mutations");
}
