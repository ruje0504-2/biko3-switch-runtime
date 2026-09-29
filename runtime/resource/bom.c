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
int bk_bom_binding_set_append(BkBomBindingSet *out, const BkBomConfig *file,
                              char e[256]) {
  if (!out || !file || out->count > BK_BOM_SET_CAPACITY || file->count > 4 ||
      file->count > BK_BOM_SET_CAPACITY - out->count)
    return fail(e, "binding set capacity exceeded or invalid input");
  for (uint32_t i = 0; i < file->count; ++i) {
    const BkBomBinding *b = &file->bindings[i];
    const char *fields[] = {b->parent, b->reference, b->child, b->primary_aux,
                            b->secondary_aux, b->target_mesh, b->source_mesh,
                            b->selection};
    if (!b->parent[0])
      return fail(e, "empty parent inside declared binding rows");
    for (unsigned j = 0; j < 8; ++j)
      if (!memchr(fields[j], 0, 260))
        return fail(e, "unterminated appended binding field");
  }
  /* Copy first, so a caller alias cannot make the append overwrite its input. */
  BkBomConfig source = *file;
  memcpy(out->bindings + out->count, source.bindings,
         (size_t)source.count * sizeof(*source.bindings));
  out->count += source.count;
  out->mode = source.mode;
  return 1;
}
int bk_bom_decode_append(const void *bytes, size_t size, BkBomBindingSet *out,
                         char e[256]) {
  BkBomConfig file;
  return bk_bom_decode(bytes, size, &file, e) &&
         bk_bom_binding_set_append(out, &file, e);
}
