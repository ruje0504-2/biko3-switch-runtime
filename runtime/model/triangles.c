#include "model/triangles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t word(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
void bk_model_triangles_free(BkModelTriangles *out) {
  if (out) {
    free(out->indices);
    memset(out, 0, sizeof(*out));
  }
}
int bk_model_triangles(const BkModel *m, uint32_t index, BkModelTriangles *out,
                       char error[256]) {
  if (!out)
    return 0;
  memset(out, 0, sizeof(*out));
  if (!m || index >= m->submesh_count)
    goto invalid;
  const BkModelSubmesh *s = &m->submeshes[index];
  if (!m->source || s->source_header_offset > m->source_size ||
      m->source_size - s->source_header_offset < 332 || s->vertex_count < 3 ||
      s->vertex_count > 65536)
    goto invalid;
  const uint8_t *h = m->source + s->source_header_offset;
  uint32_t topology = word(h + 56);
  if (word(h) || word(h + 60) || topology > 2)
    goto invalid;
  uint32_t count = topology ? (s->vertex_count - 2) * 3 : s->index_count;
  if (!count || count % 3 || count > 6000000)
    goto invalid;
  if (!topology) {
    if (!s->indices)
      goto invalid;
    for (uint32_t i = 0; i < count; i++)
      if (s->indices[i] >= s->vertex_count)
        goto invalid;
  }
  uint16_t *indices = malloc((size_t)count * sizeof(*indices));
  if (!indices) {
    snprintf(error, 256, "triangle index allocation failed");
    return 0;
  }
  if (!topology)
    memcpy(indices, s->indices, (size_t)count * sizeof(*indices));
  else
    for (uint32_t i = 0; i < s->vertex_count - 2; i++) {
      indices[i * 3] = topology == 2 ? 0 : i + (i & 1);
      indices[i * 3 + 1] = topology == 2 ? i + 1 : i + 1 - (i & 1);
      indices[i * 3 + 2] = i + 2;
    }
  *out = (BkModelTriangles){indices, count};
  return 1;
invalid:
  snprintf(error, 256, "invalid or unsupported mesh triangle topology/state");
  return 0;
}
