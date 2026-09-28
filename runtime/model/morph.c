#include "model/morph.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkMorphTrack info;
  BkMorphKey *keys;
} Track;
struct BkModelMorph {
  uint32_t count;
  Track *tracks;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "MORP: %s", message);
  return 0;
}
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static float f32(const uint8_t *p) {
  uint32_t bits = u32(p);
  float value;
  memcpy(&value, &bits, 4);
  return value;
}
void bk_model_morph_destroy(BkModelMorph *m) {
  if (!m)
    return;
  for (uint32_t t = 0; t < m->count; t++) {
    if (m->tracks[t].keys)
      for (uint32_t k = 0; k < m->tracks[t].info.key_count; k++)
        free((void *)m->tracks[t].keys[k].vertices);
    free(m->tracks[t].keys);
  }
  free(m->tracks);
  free(m);
}
BkModelMorph *bk_model_morph_create(const BkModel *model, char error[256]) {
  if (!model) {
    fail(error, "missing model");
    return NULL;
  }
  const BkModelChunk *chunk = bk_model_chunk(model, "MORP");
  if (!chunk || chunk->size < 72 || chunk->offset > model->source_size ||
      chunk->size > model->source_size - chunk->offset || !model->source ||
      !model->submeshes) {
    fail(error, "missing or truncated chunk/submesh table");
    return NULL;
  }
  const uint8_t *data = model->source + chunk->offset;
  uint32_t count = u32(data + 68);
  if (!count || count > 65536 || count > (chunk->size - 72) / 24) {
    fail(error, "invalid track count");
    return NULL;
  }
  BkModelMorph *m = calloc(1, sizeof(*m));
  if (!m) {
    fail(error, "allocation failed");
    return NULL;
  }
  m->tracks = calloc(count, sizeof(*m->tracks));
  if (!m->tracks) {
    fail(error, "track allocation failed");
    goto bad;
  }
  m->count = count;
  size_t pos = 72;
  for (uint32_t t = 0; t < count; t++) {
    if (chunk->size - pos < 24) {
      fail(error, "truncated track");
      goto bad;
    }
    Track *track = m->tracks + t;
    track->info.submesh = BK_MODEL_NONE;
    uint32_t id = u32(data + pos);
    for (uint32_t j = 0; j < model->submesh_count; j++)
      if (model->submeshes[j].id == id) {
        if (track->info.submesh != BK_MODEL_NONE) {
          fail(error, "ambiguous submesh ID");
          goto bad;
        }
        track->info.submesh = j;
      }
    if (track->info.submesh == BK_MODEL_NONE) {
      fail(error, "missing target submesh");
      goto bad;
    }
    for (uint32_t j = 0; j < t; j++)
      if (m->tracks[j].info.submesh == track->info.submesh) {
        fail(error, "duplicate track target");
        goto bad;
      }
    for (unsigned j = 4; j < 20; j += 4)
      if (u32(data + pos + j)) {
        fail(error, "unsupported track header");
        goto bad;
      }
    track->info.key_count = u32(data + pos + 20);
    pos += 24;
    if (!track->info.key_count || track->info.key_count > 1000000 ||
        track->info.key_count > (chunk->size - pos) / 76) {
      fail(error, "invalid key count");
      goto bad;
    }
    track->keys = calloc(track->info.key_count, sizeof(*track->keys));
    if (!track->keys) {
      fail(error, "key allocation failed");
      goto bad;
    }
    for (uint32_t k = 0; k < track->info.key_count; k++) {
      if (chunk->size - pos < 76) {
        fail(error, "truncated key");
        goto bad;
      }
      BkMorphKey *key = track->keys + k;
      key->time = f32(data + pos);
      key->interpolation = u32(data + pos + 4);
      key->vertex_count = u32(data + pos + 8);
      pos += 12;
      if (!isfinite(key->time) || key->time < 0 ||
          (k && key->time <= track->keys[k - 1].time) ||
          !key->vertex_count ||
          key->vertex_count != model->submeshes[track->info.submesh].vertex_count ||
          key->vertex_count > (chunk->size - pos - 64) / 60) {
        fail(error, "invalid key time, vertex count or bounds");
        goto bad;
      }
      BkModelVertex *vertices = malloc((size_t)key->vertex_count * sizeof(*vertices));
      if (!vertices) {
        fail(error, "vertex allocation failed");
        goto bad;
      }
      key->vertices = vertices;
      for (uint32_t v = 0; v < key->vertex_count; v++) {
        float fields[15];
        for (unsigned j = 0; j < 15; j++) {
          fields[j] = f32(data + pos + j * 4);
          /* Extra UV slots contain original exporter residue, including
           * NaN bit patterns. Match the base mesh decoder: validate the
           * used position/beta/normal/UV0 fields and preserve the rest. */
          if (j < 9 && !isfinite(fields[j])) {
            fail(error, "nonfinite vertex");
            goto bad;
          }
        }
        memcpy(vertices + v, fields, sizeof(fields));
        pos += 60;
      }
      for (unsigned j = 0; j < 16; j++) {
        key->matrix[j] = f32(data + pos + j * 4);
        if (!isfinite(key->matrix[j])) {
          fail(error, "nonfinite key matrix");
          goto bad;
        }
      }
      pos += 64;
    }
  }
  if (pos != chunk->size) {
    fail(error, "unconsumed chunk bytes");
    goto bad;
  }
  return m;
bad:
  bk_model_morph_destroy(m);
  return NULL;
}
uint32_t bk_model_morph_count(const BkModelMorph *m) {
  return m ? m->count : 0;
}
const BkMorphTrack *bk_model_morph_track(const BkModelMorph *m, uint32_t t) {
  return m && t < m->count ? &m->tracks[t].info : NULL;
}
const BkMorphKey *bk_model_morph_key(const BkModelMorph *m, uint32_t t,
                                    uint32_t k) {
  return m && t < m->count && k < m->tracks[t].info.key_count
             ? m->tracks[t].keys + k : NULL;
}
