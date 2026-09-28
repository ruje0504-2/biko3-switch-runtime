#include "resource/bom.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void word(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (8 * i));
}
int main(void) {
  uint8_t bytes[16384] = {0};
  size_t size = 4, offsets[34];
  word(bytes, 0x12345678);
  for (unsigned i = 0; i < 34; i++) {
    offsets[i] = size;
    const char *s = i == 10 ? "" : i == 9 ? "-" : "fixture";
    size_t n = strlen(s) + 3;
    word(bytes + size, n);
    memcpy(bytes + size + 4, s, n - 2);
    memcpy(bytes + size + 4 + n - 2, "XY", 2);
    size += n + 4;
  }
  char e[256];
  BkBomConfig config, old;
  assert(bk_bom_decode(bytes, size, &config, e));
  assert(config.mode == 0x12345678 && config.count == 1);
  assert(!strcmp(config.bindings[0].selection, "-") &&
         !config.bindings[1].parent[0]);
  old = config;
  for (size_t i = 0; i < size; i++) {
    assert(!bk_bom_decode(bytes, i, &config, e));
    assert(!memcmp(&old, &config, sizeof(old)));
  }
  for (unsigned i = 0; i < 34; i++) {
    uint8_t saved[4];
    memcpy(saved, bytes + offsets[i], 4);
    word(bytes + offsets[i], UINT32_MAX);
    assert(!bk_bom_decode(bytes, size, &config, e));
    assert(!memcmp(&old, &config, sizeof(old)));
    word(bytes + offsets[i], 0);
    assert(!bk_bom_decode(bytes, size, &config, e));
    memcpy(bytes + offsets[i], saved, 4);
  }
  memset(bytes + 8, 'A', 10);
  assert(!bk_bom_decode(bytes, size, &config, e));
  assert(!memcmp(&old, &config, sizeof(old)));
  puts("PASS BOM: early group termination, embedded NUL, bounds and atomic "
       "decode");
  return 0;
}
