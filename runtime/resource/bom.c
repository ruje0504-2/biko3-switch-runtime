#include "resource/bom.h"
#include <stdio.h>
#include <string.h>
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "BOM: %s", why);
  return 0;
}
int bk_bom_decode(const void *bytes, size_t size, BkBomConfig *out,
                  char e[256]) {
  if (!bytes || !out || size < 4)
    return fail(e, "missing header/output");
  const uint8_t *data = bytes, *rows[34];
  size_t lengths[34], offset = 4;
  for (unsigned i = 0; i < 34; i++) {
    if (size - offset < 4)
      return fail(e, "truncated string length");
    uint32_t n = u32(data + offset);
    offset += 4;
    if (!n || n > 260 || n > size - offset)
      return fail(e, "invalid string storage");
    const uint8_t *end = memchr(data + offset, 0, n);
    if (!end)
      return fail(e, "unterminated string");
    rows[i] = data + offset;
    lengths[i] = (size_t)(end - rows[i]);
    offset += n;
  }
  BkBomConfig next = {.mode = u32(data)};
  memcpy(next.primary_clip, rows[0], lengths[0]);
  memcpy(next.secondary_clip, rows[1], lengths[1]);
  for (unsigned i = 0; i < 4; i++) {
    unsigned first = 2 + 8 * i;
    if (!lengths[first])
      break;
    BkBomBinding *binding = &next.bindings[i];
    char *fields[] = {binding->parent,        binding->reference,
                      binding->child,         binding->primary_aux,
                      binding->secondary_aux, binding->target_mesh,
                      binding->source_mesh,   binding->selection};
    for (unsigned j = 0; j < 8; j++)
      memcpy(fields[j], rows[first + j], lengths[first + j]);
    next.count++;
  }
  *out = next;
  return 1;
}
