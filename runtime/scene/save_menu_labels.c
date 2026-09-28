#include "scene/save_menu_labels.h"
#include <stdio.h>
#include <string.h>
static void full(uint8_t *out, size_t *n, uint8_t ascii) {
  uint16_t code = ascii >= '0' && ascii <= '9' ? 0x824f + ascii - '0'
                  : ascii == '/'               ? 0x815e
                  : ascii == ':'               ? 0x8146
                                               : 0x817c;
  out[(*n)++] = (uint8_t)(code >> 8);
  out[(*n)++] = (uint8_t)code;
}
int bk_save_menu_labels(const BkSaveMenuLabel records[10], uint8_t out[512],
                        size_t *size, char e[256]) {
  if (!records || !out || !size) {
    snprintf(e, 256, "save labels: invalid input/output");
    return 0;
  }
  uint8_t tmp[512] = {0};
  size_t n = 0;
  for (unsigned i = 0; i < 10; ++i) {
    const BkSaveMenuLabel *r = &records[i];
    full(tmp, &n, (uint8_t)('0' + (i + 1) / 10));
    full(tmp, &n, (uint8_t)('0' + (i + 1) % 10));
    full(tmp, &n, '-');
    if (!r->stamp[0]) {
      for (unsigned k = 0; k < 19; ++k)
        full(tmp, &n, '-');
      continue;
    }
    if (r->area > 8) {
      snprintf(e, 256, "save labels: invalid occupied area");
      return 0;
    }
    /* Japanese UI: エリア. Keep the recovered three-glyph prefix width.
     * The analyzed Chinese binary says 场景号; this is an explicit locale
     * adaptation, not evidence of the packed Japanese executable's literals. */
    static const uint8_t prefix[] = {0x83, 0x47, 0x83, 0x8a, 0x83, 0x41};
    memcpy(tmp + n, prefix, sizeof(prefix));
    n += sizeof(prefix);
    full(tmp, &n, (uint8_t)('1' + r->area));
    full(tmp, &n, '-');
    for (unsigned k = 2; k < 16; ++k) {
      uint8_t ch = (uint8_t)r->stamp[k];
      if ((ch >= '0' && ch <= '9') || ch == '/' || ch == '-' || ch == ':')
        full(tmp, &n, ch);
    }
  }
  memcpy(out, tmp, sizeof(tmp));
  *size = n;
  return 1;
}
