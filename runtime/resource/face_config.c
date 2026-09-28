#include "resource/face_config.h"
#include <stdio.h>
#include <string.h>
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static int fail(char *error, const char *reason) {
  snprintf(error, 256, "FAM: %s", reason);
  return 0;
}
int bk_face_config_decode(const uint8_t *data, size_t size, BkFaceConfig *out,
                          char error[256]) {
  if (!data || !out || size < 4)
    return fail(error, "missing header/output");
  const uint8_t *rows[30];
  size_t lengths[30], offset = 4;
  for (unsigned i = 0; i < 30; i++) {
    if (size - offset < 4)
      return fail(error, "truncated string length");
    uint32_t n = u32(data + offset);
    offset += 4;
    if (!n || n > 260 || n > size - offset)
      return fail(error, "invalid string storage");
    const uint8_t *end = memchr(data + offset, 0, n);
    if (!end || end - (data + offset) >= 256)
      return fail(error, "unterminated/oversized string");
    rows[i] = data + offset;
    lengths[i] = (size_t)(end - rows[i]);
    offset += n;
  }
  BkFaceConfig next = {.texture_mode = u32(data)};
  char *headers[] = {next.actor_clip,       next.source_clip,
                     next.eye_materials[0], next.eye_materials[1],
                     next.eye_textures[0],  next.eye_textures[1]};
  for (unsigned i = 0; i < 6; i++)
    memcpy(headers[i], rows[i], lengths[i]);
  for (unsigned group = 0; group < 2; group++)
    for (unsigned i = 0; i < 4; i++) {
      unsigned row = 6 + group * 12 + i * 3;
      if (!lengths[row])
        break;
      BkFaceSlot *s = &next.slots[group][i];
      memcpy(s->target, rows[row], lengths[row]);
      memcpy(s->source, rows[row + 1], lengths[row + 1]);
      memcpy(s->selection, rows[row + 2], lengths[row + 2]);
      next.counts[group]++;
    }
  *out = next;
  return 1;
}
