#include "model/model.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_SOURCE (256u * 1024 * 1024)
#define MAX_RECORDS 65536u
#define MAX_VERTICES (2u * 1024 * 1024)
#define MAX_INDICES (6u * 1024 * 1024)
_Static_assert(sizeof(BkModelVertex) == 60,
               "vertex layout must match the file");
static uint32_t u32(const uint8_t *p) {
  return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static float f32(const uint8_t *p) {
  uint32_t bits = u32(p);
  float f;
  memcpy(&f, &bits, 4);
  return f;
}
static int fail(char *error, const char *message) {
  snprintf(error, 256, "OBJM: %s", message);
  return 0;
}
static int name_read(char out[65], const uint8_t *p, char *error) {
  const uint8_t *end = memchr(p, 0, 64);
  if (!end)
    return fail(error, "unterminated name");
  size_t n = (size_t)(end - p);
  memcpy(out, p, n);
  out[n] = 0;
  return 1;
}
static void *array(uint32_t n, size_t item, char *error) {
  if (n > MAX_RECORDS) {
    fail(error, "record count exceeds limit");
    return NULL;
  }
  void *p = calloc(n ? n : 1, item);
  if (!p)
    fail(error, "allocation failed");
  return p;
}
/* Records are bounded by both count and source size before any allocation. */
static int append(void **items, uint32_t *count, size_t size, char *error) {
  if (*count >= MAX_RECORDS)
    return fail(error, "record count exceeds limit");
  void *next = realloc(*items, ((size_t)*count + 1) * size);
  if (!next)
    return fail(error, "allocation failed");
  *items = next;
  memset((uint8_t *)next + (size_t)*count * size, 0, size);
  (*count)++;
  return 1;
}
const BkModelChunk *bk_model_chunk(const BkModel *m, const char tag[4]) {
  for (uint32_t i = 0; i < m->chunk_count; i++)
    if (!memcmp(m->chunks[i].tag, tag, 4))
      return &m->chunks[i];
  return NULL;
}
void bk_model_destroy(BkModel *m) {
  if (!m)
    return;
  for (uint32_t i = 0; i < m->submesh_count; i++) {
    free(m->submeshes[i].vertices);
    free(m->submeshes[i].indices);
  }
  free(m->submeshes);
  free(m->meshes);
  free(m->materials);
  free(m->textures);
  free(m->frames);
  free(m->chunks);
  free(m->source);
  free(m);
}
static int tables(BkModel *m, char *error) {
  const BkModelChunk *c = bk_model_chunk(m, "MATE");
  if (c) {
    if (c->size % 140)
      return fail(error, "MATE stride mismatch");
    m->material_count = c->size / 140;
    m->materials = array(m->material_count, sizeof(*m->materials), error);
    if (!m->materials)
      return 0;
    for (uint32_t i = 0; i < m->material_count; i++) {
      const uint8_t *p = m->source + c->offset + i * 140;
      BkModelMaterial *v = &m->materials[i];
      if (!name_read(v->name, p, error))
        return 0;
      v->id = u32(p + 64);
      float fields[18];
      for (unsigned j = 0; j < 18; j++) {
        fields[j] = f32(p + 68 + j * 4);
        if (!isfinite(fields[j]))
          return fail(error, "nonfinite material");
      }
      memcpy(v->diffuse, fields, 16);
      memcpy(v->ambient, fields + 4, 16);
      memcpy(v->specular, fields + 8, 16);
      memcpy(v->emissive, fields + 12, 16);
      v->power = fields[16];
      v->unknown = fields[17];
    }
  }
  c = bk_model_chunk(m, "TEXT");
  if (c) {
    if (c->size % 204)
      return fail(error, "TEXT stride mismatch");
    m->texture_count = c->size / 204;
    m->textures = array(m->texture_count, sizeof(*m->textures), error);
    if (!m->textures)
      return 0;
    for (uint32_t i = 0; i < m->texture_count; i++) {
      const uint8_t *p = m->source + c->offset + i * 204;
      BkModelTexture *v = &m->textures[i];
      v->id = u32(p + 64);
      if (!name_read(v->name, p, error) ||
          !name_read(v->filename, p + 68, error))
        return 0;
      if (!*v->filename || strchr(v->filename, '/') ||
          strchr(v->filename, '\\') || !strcmp(v->filename, ".") ||
          !strcmp(v->filename, ".."))
        return fail(error, "unsafe texture resource name");
    }
  }
  c = bk_model_chunk(m, "FRAM");
  if (c) {
    if (c->size % 396)
      return fail(error, "FRAM stride mismatch");
    m->frame_count = c->size / 396;
    m->frames = array(m->frame_count, sizeof(*m->frames), error);
    if (!m->frames)
      return 0;
    for (uint32_t i = 0; i < m->frame_count; i++) {
      const uint8_t *p = m->source + c->offset + i * 396;
      BkModelFrame *v = &m->frames[i];
      if (!name_read(v->name, p, error))
        return 0;
      v->id = u32(p + 64);
      v->parent_id = u32(p + 172);
      v->mesh_id = u32(p + 176);
      for (unsigned j = 0; j < 16; j++) {
        v->local[j] = f32(p + 68 + j * 4);
        if (!isfinite(v->local[j]))
          return fail(error, "nonfinite frame matrix");
      }
    }
  }
  return 1;
}
static int geometry(BkModel *m, char *error) {
  const BkModelChunk *c = bk_model_chunk(m, "MESH");
  if (!c)
    return fail(error, "MESH chunk missing");
  size_t pos = c->offset, end = pos + c->size;
  while (pos < end) {
    if (end - pos < 72)
      return fail(error, "truncated mesh header");
    if (!append((void **)&m->meshes, &m->mesh_count, sizeof(*m->meshes), error))
      return 0;
    BkModelMesh *mesh = &m->meshes[m->mesh_count - 1];
    const uint8_t *p = m->source + pos;
    if (!name_read(mesh->name, p, error))
      return 0;
    mesh->id = u32(p + 64);
    mesh->submesh_count = u32(p + 68);
    mesh->first_submesh = m->submesh_count;
    pos += 72;
    if (mesh->submesh_count > 4096)
      return fail(error, "too many submeshes");
    for (uint32_t j = 0; j < mesh->submesh_count; j++) {
      if (!append((void **)&m->submeshes, &m->submesh_count,
                  sizeof(*m->submeshes), error))
        return 0;
      BkModelSubmesh *sub = &m->submeshes[m->submesh_count - 1];
      sub->mesh_index = m->mesh_count - 1;
      if (mesh->submesh_count == 1) {
        memcpy(sub->name, mesh->name, sizeof(sub->name));
        sub->id = mesh->id;
      } else {
        if (end - pos < 72 || u32(m->source + pos + 68) != 1)
          return fail(error, "invalid grouped child header");
        if (!name_read(sub->name, m->source + pos, error))
          return 0;
        sub->id = u32(m->source + pos + 64);
        pos += 72;
      }
      if (end - pos < 332)
        return fail(error, "truncated submesh header");
      p = m->source + pos;
      sub->source_header_offset = (uint32_t)pos;
      sub->material_id = u32(p + 4);
      sub->texture_count = u32(p + 72);
      for (unsigned k = 0; k < 4; k++)
        sub->texture_ids[k] = u32(p + 8 + k * 4);
      sub->vertex_count = u32(p + 64);
      sub->index_count = u32(p + 68);
      pos += 332;
      if (sub->texture_count > 4 || sub->index_count % 3 ||
          sub->vertex_count > MAX_VERTICES - m->vertex_count ||
          sub->index_count > MAX_INDICES - m->triangle_count * 3)
        return fail(error, "invalid geometry/texture count");
      uint64_t bytes =
          (uint64_t)sub->vertex_count * 60 + (uint64_t)sub->index_count * 2;
      if (bytes > end - pos)
        return fail(error, "truncated vertex/index data");
      sub->vertices = calloc(sub->vertex_count ? sub->vertex_count : 1,
                             sizeof(*sub->vertices));
      sub->indices = calloc(sub->index_count ? sub->index_count : 1,
                            sizeof(*sub->indices));
      if (!sub->vertices || !sub->indices)
        return fail(error, "geometry allocation failed");
      for (uint32_t k = 0; k < sub->vertex_count; k++) {
        float fields[15];
        for (unsigned f = 0; f < 15; f++) {
          fields[f] = f32(m->source + pos + k * 60 + f * 4);
          if (f < 9 && !isfinite(fields[f]))
            return fail(error, "nonfinite vertex");
        }
        memcpy(&sub->vertices[k], fields, 60);
      }
      pos += (size_t)sub->vertex_count * 60;
      for (uint32_t k = 0; k < sub->index_count; k++) {
        uint16_t index = (uint16_t)(m->source[pos + k * 2] |
                                    m->source[pos + k * 2 + 1] << 8);
        if (index >= sub->vertex_count)
          return fail(error, "vertex index out of range");
        sub->indices[k] = index;
      }
      pos += (size_t)sub->index_count * 2;
      m->vertex_count += sub->vertex_count;
      m->triangle_count += sub->index_count / 3;
    }
  }
  return 1;
}
typedef struct {
  uint32_t id, index;
} Id;
static int compare_id(const void *a, const void *b) {
  uint32_t x = ((const Id *)a)->id, y = ((const Id *)b)->id;
  return (x > y) - (x < y);
}
static Id *id_map(const void *records, uint32_t n, size_t stride, size_t offset,
                  char *error) {
  Id *ids = array(n, sizeof(*ids), error);
  if (!ids)
    return NULL;
  for (uint32_t i = 0; i < n; i++) {
    memcpy(&ids[i].id, (const uint8_t *)records + i * stride + offset, 4);
    ids[i].index = i;
  }
  qsort(ids, n, sizeof(*ids), compare_id);
  for (uint32_t i = 0; i < n; i++)
    if (!ids[i].id || (i && ids[i].id == ids[i - 1].id)) {
      free(ids);
      fail(error, "zero or duplicate object ID");
      return NULL;
    }
  return ids;
}
static uint32_t lookup(const Id *ids, uint32_t n, uint32_t id) {
  if (!id)
    return BK_MODEL_NONE;
  Id key = {id, 0};
  const Id *found = bsearch(&key, ids, n, sizeof(*ids), compare_id);
  return found ? found->index : BK_MODEL_NONE;
}
static int references(BkModel *m, char *error) {
  Id *materials = id_map(m->materials, m->material_count, sizeof(*m->materials),
                         offsetof(BkModelMaterial, id), error);
  Id *textures = id_map(m->textures, m->texture_count, sizeof(*m->textures),
                        offsetof(BkModelTexture, id), error);
  Id *meshes = id_map(m->meshes, m->mesh_count, sizeof(*m->meshes),
                      offsetof(BkModelMesh, id), error);
  Id *frames = id_map(m->frames, m->frame_count, sizeof(*m->frames),
                      offsetof(BkModelFrame, id), error);
  uint8_t *state = calloc(m->frame_count ? m->frame_count : 1, 1);
  int ok = 0;
  if (!materials || !textures || !meshes || !frames || !state)
    goto done;
  for (uint32_t i = 0; i < m->submesh_count; i++) {
    BkModelSubmesh *sub = &m->submeshes[i];
    sub->material_index =
        lookup(materials, m->material_count, sub->material_id);
    if (sub->material_id && sub->material_index == BK_MODEL_NONE) {
      fail(error, "missing material reference");
      goto done;
    }
    for (unsigned j = 0; j < 4; j++) {
      sub->texture_indices[j] =
          j < sub->texture_count
              ? lookup(textures, m->texture_count, sub->texture_ids[j])
              : BK_MODEL_NONE;
      if (j < sub->texture_count && sub->texture_ids[j] &&
          sub->texture_indices[j] == BK_MODEL_NONE) {
        fail(error, "missing texture reference");
        goto done;
      }
    }
  }
  for (uint32_t i = 0; i < m->frame_count; i++) {
    BkModelFrame *f = &m->frames[i];
    f->parent_index = lookup(frames, m->frame_count, f->parent_id);
    f->mesh_index = lookup(meshes, m->mesh_count, f->mesh_id);
    if ((f->parent_id && f->parent_index == BK_MODEL_NONE) ||
        (f->mesh_id && f->mesh_index == BK_MODEL_NONE)) {
      fail(error, "missing frame parent or mesh reference");
      goto done;
    }
  }
  /* Iterative tri-colour traversal: detects cycles without recursion/depth
   * risk. */
  for (uint32_t i = 0; i < m->frame_count; i++) {
    uint32_t j = i;
    while (j != BK_MODEL_NONE && !state[j]) {
      state[j] = 1;
      j = m->frames[j].parent_index;
    }
    if (j != BK_MODEL_NONE && state[j] == 1) {
      fail(error, "frame hierarchy cycle");
      goto done;
    }
    j = i;
    while (j != BK_MODEL_NONE && state[j] == 1) {
      state[j] = 2;
      j = m->frames[j].parent_index;
    }
  }
  ok = 1;
done:
  if (!state)
    fail(error, "hierarchy allocation failed");
  free(state);
  free(materials);
  free(textures);
  free(meshes);
  free(frames);
  return ok;
}
BkModelResult bk_model_decode(const uint8_t *data, size_t size, BkModel **out,
                              char error[256]) {
  *out = NULL;
  if (data && size >= 16 && !memcmp(data, "xof 0302txt 0032", 15)) {
    snprintf(error, 256, "DirectX text model requires a separate parser");
    return BK_MODEL_UNSUPPORTED;
  }
  if (!data || size < 12 || size > MAX_SOURCE || memcmp(data, "OBJM", 4)) {
    fail(error, "invalid signature/size");
    return BK_MODEL_INVALID;
  }
  if (u32(data + 4) || u32(data + 8)) {
    snprintf(error, 256, "unsupported OBJM header revision");
    return BK_MODEL_UNSUPPORTED;
  }
  BkModel *m = calloc(1, sizeof(*m));
  if (!m) {
    fail(error, "model allocation failed");
    return BK_MODEL_INVALID;
  }
  m->source = malloc(size);
  m->source_size = size;
  if (!m->source) {
    fail(error, "source allocation failed");
    goto bad;
  }
  memcpy(m->source, data, size);
  for (size_t pos = 12; pos < size;) {
    if (size - pos < 8) {
      fail(error, "truncated chunk header");
      goto bad;
    }
    uint32_t n = u32(data + pos + 4);
    if (n > size - pos - 8) {
      fail(error, "chunk exceeds model bounds");
      goto bad;
    }
    for (unsigned j = 0; j < 4; j++)
      if (data[pos + j] < 32 || data[pos + j] > 126) {
        fail(error, "invalid chunk tag");
        goto bad;
      }
    if (bk_model_chunk(m, (const char *)(data + pos))) {
      fail(error, "duplicate chunk");
      goto bad;
    }
    if (m->chunk_count >= 128) {
      fail(error, "chunk count exceeds limit");
      goto bad;
    }
    if (!append((void **)&m->chunks, &m->chunk_count, sizeof(*m->chunks),
                error))
      goto bad;
    BkModelChunk *c = &m->chunks[m->chunk_count - 1];
    memcpy(c->tag, data + pos, 4);
    c->offset = (uint32_t)pos + 8;
    c->size = n;
    pos += 8 + n;
  }
  if (!tables(m, error) || !geometry(m, error) || !references(m, error))
    goto bad;
  if (!m->mesh_count || !m->submesh_count || !m->triangle_count) {
    fail(error, "model has no triangle geometry");
    goto bad;
  }
  *out = m;
  return BK_MODEL_OK;
bad:
  bk_model_destroy(m);
  return BK_MODEL_INVALID;
}

int bk_model_pose_world_matrices(const BkModel *m, const float *local,
                                 float *output, size_t count, char error[256]) {
  return bk_model_pose_world_matrices_under(m, local, BK_MODEL_NONE, NULL,
                                            output, count, error);
}
int bk_model_pose_world_matrices_under(const BkModel *m, const float *local,
                                       uint32_t root, const float external[16],
                                       float *output, size_t count,
                                       char error[256]) {
  if (!m || !output || count != (size_t)m->frame_count * 16)
    return fail(error, "invalid world-matrix output");
  if (external &&
      (root >= m->frame_count || m->frames[root].parent_index != BK_MODEL_NONE))
    return fail(error, "invalid external parent root");
  uint8_t *state = calloc(m->frame_count ? m->frame_count : 1, 1);
  uint32_t *stack = array(m->frame_count, sizeof(*stack), error);
  int ok = 0;
  if (!state || !stack) {
    fail(error, "matrix traversal allocation failed");
    goto done;
  }
  for (uint32_t i = 0; i < m->frame_count; i++) {
    uint32_t j = i, depth = 0;
    while (j != BK_MODEL_NONE) {
      if (j >= m->frame_count) {
        fail(error, "invalid frame parent index");
        goto done;
      }
      if (state[j])
        break;
      state[j] = 1;
      stack[depth++] = j;
      j = m->frames[j].parent_index;
    }
    if (j != BK_MODEL_NONE && state[j] == 1) {
      fail(error, "frame hierarchy cycle");
      goto done;
    }
    while (depth) {
      j = stack[--depth];
      const BkModelFrame *f = &m->frames[j];
      float *dest = output + j * 16;
      const float *matrix = local ? local + j * 16 : f->local;
      const float *parent = j == root && external ? external
                            : f->parent_index == BK_MODEL_NONE
                                ? NULL
                                : output + f->parent_index * 16;
      if (!parent)
        memcpy(dest, matrix, 64);
      else {
        for (unsigned r = 0; r < 4; r++)
          for (unsigned c = 0; c < 4; c++) {
            double sum = 0;
            for (unsigned k = 0; k < 4; k++)
              sum += (double)matrix[r * 4 + k] * parent[k * 4 + c];
            dest[r * 4 + c] = (float)sum;
          }
      }
      for (unsigned k = 0; k < 16; k++)
        if (!isfinite(dest[k])) {
          fail(error, "world matrix overflow");
          goto done;
        }
      state[j] = 2;
    }
  }
  ok = 1;
done:
  free(stack);
  free(state);
  return ok;
}

/*425904 visits depth-first in insertion order, including an empty query.
 * Model topology has already been validated by decoding. */
int bk_model_find_frame_first(const BkModel *m, uint32_t root, const char *name,
                              uint32_t *out, char e[256]) {
  if (!m || root >= m->frame_count || !name || !out)
    return fail(e, "invalid subtree binding");
  *out = BK_MODEL_NONE;
  uint32_t i = root;
  while (i != BK_MODEL_NONE) {
    const char *space = strchr(m->frames[i].name, ' ');
    if (!strcmp(name, space ? space + 1 : m->frames[i].name)) {
      *out = i;
      return 1;
    }
    uint32_t next = BK_MODEL_NONE;
    for (uint32_t j = 0; j < m->frame_count; j++)
      if (m->frames[j].parent_index == i) {
        next = j;
        break;
      }
    if (next != BK_MODEL_NONE) {
      i = next;
      continue;
    }
    while (i != root) {
      uint32_t parent = m->frames[i].parent_index;
      for (uint32_t j = i + 1; j < m->frame_count; j++)
        if (m->frames[j].parent_index == parent) {
          next = j;
          break;
        }
      if (next != BK_MODEL_NONE)
        break;
      i = parent;
    }
    i = next;
  }
  return 1;
}

int bk_model_find_frame(const BkModel *m, const char *name, uint32_t *index,
                        char error[256]) {
  if (!m || !name || !*name || !index)
    return fail(error, "invalid frame binding");
  uint32_t match = BK_MODEL_NONE;
  for (uint32_t i = 0; i < m->frame_count; i++) {
    const char *space = strchr(m->frames[i].name, ' ');
    const char *candidate = space ? space + 1 : m->frames[i].name;
    if (!strcmp(name, candidate)) {
      if (match != BK_MODEL_NONE)
        return fail(error, "ambiguous frame binding");
      match = i;
    }
  }
  if (match == BK_MODEL_NONE)
    return fail(error, "frame binding not found");
  *index = match;
  return 1;
}

int bk_model_world_matrices(const BkModel *m, float *output, size_t count,
                            char error[256]) {
  return bk_model_pose_world_matrices(m, NULL, output, count, error);
}
