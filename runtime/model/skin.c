#include "model/skin.h"
#include "core/matrix.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkSkinEntry info;
  BkSkinBone *bones;
} Entry;
struct BkModelSkin {
  uint32_t count, frame_count;
  Entry *entries;
};
struct BkSkinMesh {
  const BkModelSkin *skin;
  uint32_t entry, count;
  BkModelVertex *base, *vertices, *pending;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "ENVL: %s", message);
  return 0;
}
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static float f32(const uint8_t *p) {
  uint32_t bits = u32(p);
  float result;
  memcpy(&result, &bits, 4);
  return result;
}
static uint32_t frame_id(const BkModel *m, uint32_t id) {
  uint32_t result = BK_MODEL_NONE;
  for (uint32_t i = 0; i < m->frame_count; i++)
    if (m->frames[i].id == id) {
      if (result != BK_MODEL_NONE)
        return BK_MODEL_NONE;
      result = i;
    }
  return result;
}
void bk_model_skin_destroy(BkModelSkin *s) {
  if (!s)
    return;
  for (uint32_t i = 0; i < s->count; i++) {
    Entry *e = &s->entries[i];
    if (e->bones)
      for (uint32_t j = 0; j < e->info.bone_count; j++)
        free((void *)e->bones[j].influences);
    free(e->bones);
  }
  free(s->entries);
  free(s);
}
BkModelSkin *bk_model_skin_create(const BkModel *m, char error[256]) {
  if (!m || !m->frames || !m->submeshes || !m->source) {
    fail(error, "missing model data");
    return NULL;
  }
  const BkModelChunk *c = bk_model_chunk(m, "ENVL");
  if (!c || c->size < 72 || c->offset > m->source_size ||
      c->size > m->source_size - c->offset) {
    fail(error, "missing/truncated chunk");
    return NULL;
  }
  const uint8_t *data = m->source + c->offset;
  uint32_t count = u32(data + 68);
  if (!count || count > 65536 || count > (c->size - 72) / 80) {
    fail(error, "invalid entry count");
    return NULL;
  }
  BkModelSkin *s = calloc(1, sizeof(*s));
  if (!s) {
    fail(error, "allocation failed");
    return NULL;
  }
  s->entries = calloc(count, sizeof(*s->entries));
  if (!s->entries) {
    fail(error, "entry allocation failed");
    goto bad;
  }
  s->count = count;
  s->frame_count = m->frame_count;
  size_t pos = 72;
  for (uint32_t i = 0; i < count; i++) {
    if (c->size - pos < 80) {
      fail(error, "truncated entry");
      goto bad;
    }
    Entry *e = &s->entries[i];
    e->info.frame = frame_id(m, u32(data + pos + 68));
    e->info.submesh = BK_MODEL_NONE;
    uint32_t id = u32(data + pos + 72);
    for (uint32_t j = 0; j < m->submesh_count; j++)
      if (m->submeshes[j].id == id) {
        if (e->info.submesh != BK_MODEL_NONE) {
          fail(error, "ambiguous submesh ID");
          goto bad;
        }
        e->info.submesh = j;
      }
    if (e->info.frame == BK_MODEL_NONE || e->info.submesh == BK_MODEL_NONE) {
      fail(error, "missing/ambiguous frame or submesh");
      goto bad;
    }
    for (uint32_t j = 0; j < i; j++)
      if (s->entries[j].info.submesh == e->info.submesh) {
        fail(error, "duplicate skin target");
        goto bad;
      }
    e->info.vertex_count = m->submeshes[e->info.submesh].vertex_count;
    e->info.bone_count = u32(data + pos + 76);
    pos += 80;
    if (!e->info.vertex_count || !e->info.bone_count ||
        e->info.bone_count > 65536 ||
        e->info.bone_count > (c->size - pos) / 8) {
      fail(error, "invalid target/bone count");
      goto bad;
    }
    e->bones = calloc(e->info.bone_count, sizeof(*e->bones));
    if (!e->bones) {
      fail(error, "bone allocation failed");
      goto bad;
    }
    for (uint32_t j = 0; j < e->info.bone_count; j++) {
      if (c->size - pos < 8) {
        fail(error, "truncated bone");
        goto bad;
      }
      BkSkinBone *b = &e->bones[j];
      b->frame = frame_id(m, u32(data + pos));
      b->count = u32(data + pos + 4);
      pos += 8;
      if (b->frame == BK_MODEL_NONE || b->count > 2u * 1024 * 1024 ||
          b->count > (c->size - pos) / 32) {
        fail(error, "invalid bone frame/influence count");
        goto bad;
      }
      BkSkinInfluence *v = b->count ? calloc(b->count, sizeof(*v)) : NULL;
      if (b->count && !v) {
        fail(error, "influence allocation failed");
        goto bad;
      }
      b->influences = v;
      for (uint32_t k = 0; k < b->count; k++) {
        for (unsigned d = 0; d < 3; d++) {
          v[k].position[d] = f32(data + pos + (size_t)k * 12 + d * 4);
          v[k].normal[d] =
              f32(data + pos + (size_t)b->count * 12 + (size_t)k * 12 + d * 4);
          if (!isfinite(v[k].position[d]) || !isfinite(v[k].normal[d])) {
            fail(error, "nonfinite influence vector");
            goto bad;
          }
        }
        v[k].index = u32(data + pos + (size_t)b->count * 24 + k * 4);
        v[k].weight = f32(data + pos + (size_t)b->count * 28 + k * 4);
        if (v[k].index >= e->info.vertex_count || !isfinite(v[k].weight)) {
          fail(error, "invalid influence index/weight");
          goto bad;
        }
      }
      pos += (size_t)b->count * 32;
    }
  }
  if (pos != c->size) {
    fail(error, "trailing chunk bytes");
    goto bad;
  }
  return s;
bad:
  bk_model_skin_destroy(s);
  return NULL;
}
uint32_t bk_model_skin_count(const BkModelSkin *s) { return s ? s->count : 0; }
const BkSkinEntry *bk_model_skin_entry(const BkModelSkin *s, uint32_t i) {
  return s && i < s->count ? &s->entries[i].info : NULL;
}
const BkSkinBone *bk_model_skin_bone(const BkModelSkin *s, uint32_t i,
                                     uint32_t j) {
  return s && i < s->count && j < s->entries[i].info.bone_count
             ? &s->entries[i].bones[j]
             : NULL;
}
static int finite_vertex(const BkModelVertex *v) {
  float fields[9];
  memcpy(fields, v, sizeof(fields));
  for (unsigned i = 0; i < 9; i++)
    if (!isfinite(fields[i]))
      return 0;
  return 1;
}
void bk_skin_mesh_destroy(BkSkinMesh *m) {
  if (!m)
    return;
  free(m->base);
  free(m->vertices);
  free(m->pending);
  free(m);
}
BkSkinMesh *bk_skin_mesh_create(const BkModelSkin *s, uint32_t entry,
                                const BkModelSubmesh *mesh, char error[256]) {
  const BkSkinEntry *e = bk_model_skin_entry(s, entry);
  if (!e || !mesh || !mesh->vertices || mesh->vertex_count != e->vertex_count ||
      mesh->vertex_count > 2u * 1024 * 1024) {
    fail(error, "invalid mesh binding");
    return NULL;
  }
  for (uint32_t i = 0; i < mesh->vertex_count; i++)
    if (!finite_vertex(mesh->vertices + i)) {
      fail(error, "nonfinite source mesh");
      return NULL;
    }
  BkSkinMesh *m = calloc(1, sizeof(*m));
  if (!m) {
    fail(error, "mesh allocation failed");
    return NULL;
  }
  m->skin = s;
  m->entry = entry;
  m->count = mesh->vertex_count;
  size_t bytes = (size_t)m->count * sizeof(*m->base);
  m->base = malloc(bytes);
  m->vertices = malloc(bytes);
  m->pending = malloc(bytes);
  if (!m->base || !m->vertices || !m->pending) {
    fail(error, "vertex allocation failed");
    bk_skin_mesh_destroy(m);
    return NULL;
  }
  memcpy(m->base, mesh->vertices, bytes);
  memcpy(m->vertices, mesh->vertices, bytes);
  return m;
}
const BkModelVertex *bk_skin_mesh_vertices(const BkSkinMesh *m) {
  return m ? m->vertices : NULL;
}
uint32_t bk_skin_mesh_count(const BkSkinMesh *m) { return m ? m->count : 0; }
int bk_skin_mesh_apply(BkSkinMesh *m, const float *world, size_t float_count,
                       const BkModelVertex *source, size_t source_count,
                       char error[256]) {
  if (!m || !world || float_count != (size_t)m->skin->frame_count * 16 ||
      (source ? source_count != m->count : source_count != 0))
    return fail(error, "invalid pose/source bounds");
  const Entry *e = &m->skin->entries[m->entry];
  for (uint32_t j = 0; j < e->info.bone_count; j++) {
    const float *matrix = world + (size_t)e->bones[j].frame * 16;
    for (unsigned k = 0; k < 16; k++)
      if (!isfinite(matrix[k]))
        return fail(error, "nonfinite bone matrix");
  }
  size_t bytes = (size_t)m->count * sizeof(*m->vertices);
  memcpy(m->pending, source ? source : m->base, bytes);
  for (uint32_t j = 0; j < e->info.bone_count; j++)
    for (uint32_t k = 0; k < e->bones[j].count; k++)
      m->pending[e->bones[j].influences[k].index].beta = 0;
  for (uint32_t j = 0; j < e->info.bone_count; j++) {
    const BkSkinBone *b = &e->bones[j];
    const float *matrix = world + (size_t)b->frame * 16;
    for (uint32_t k = 0; k < b->count; k++) {
      const BkSkinInfluence *in = &b->influences[k];
      BkModelVertex *out = &m->pending[in->index];
      float position[3], normal[3];
      if (!bk_matrix_transform_coord(position, in->position, matrix))
        return fail(error, "invalid bone homogeneous coordinate");
      for (unsigned d = 0; d < 3; d++)
        normal[d] = (float)((double)in->normal[0] * matrix[d] +
                            (double)in->normal[1] * matrix[4 + d] +
                            (double)in->normal[2] * matrix[8 + d]);
      float weight = in->weight;
      int first = out->beta <= .001f;
      int finish = !first && (double)out->beta + weight >= (double).999f;
      if (finish)
        weight = (float)(1.0 - out->beta);
      for (unsigned d = 0; d < 3; d++) {
        out->position[d] = (float)((double)position[d] * weight +
                                   (first ? 0 : (double)out->position[d]));
        out->normal[d] = (float)((double)normal[d] * weight +
                                 (first ? 0 : (double)out->normal[d]));
      }
      out->beta = first    ? weight
                  : finish ? 1
                           : (float)((double)out->beta + weight);
    }
  }
  for (uint32_t i = 0; i < m->count; i++)
    if (!finite_vertex(m->pending + i))
      return fail(error, "nonfinite skin result");
  BkModelVertex *old = m->vertices;
  m->vertices = m->pending;
  m->pending = old;
  return 1;
}
