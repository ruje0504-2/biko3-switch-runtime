#include "model/static_model.h"
#include "core/matrix.h"
#include "model/draw_order.h"
#include <math.h>
#include <stdlib.h>
static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
void bk_static_model_destroy(BkStaticModel *s) {
  if (!s)
    return;
  free(s->parts);
  free(s->world);
  free(s->instances);
  free(s);
}
BkStaticModel *bk_static_model_create(const BkModel *m, const int *alpha,
                                      char error[256]) {
  if (!m || !m->frame_count || (m->texture_count && !alpha)) {
    snprintf(error, 256, "invalid static model inputs");
    return NULL;
  }
  BkStaticModel *s = calloc(1, sizeof(*s));
  uint32_t *next = NULL, *child = NULL, *stack = NULL;
  if (!s)
    goto oom;
  s->model = m;
  s->parts = calloc(m->submesh_count, sizeof(*s->parts));
  s->world = calloc((size_t)m->frame_count * 16, sizeof(float));
  next = malloc((size_t)m->frame_count * sizeof(*next));
  child = malloc(((size_t)m->frame_count + 1) * sizeof(*child));
  stack = malloc((size_t)m->frame_count * sizeof(*stack));
  if (!s->parts || !s->world || !next || !child || !stack)
    goto oom;
  if (!bk_model_world_matrices(m, s->world, (size_t)m->frame_count * 16, error))
    goto fail;
  for (uint32_t i = 0; i < m->submesh_count; i++) {
    const BkModelSubmesh *sub = &m->submeshes[i];
    const uint8_t *h = m->source + sub->source_header_offset;
    /* The immutable decoded model has already bounded this 332-byte header. */
    if (read32(h) || read32(h + 56) > 2 || read32(h + 60) ||
        sub->texture_count > 1 || sub->material_index == BK_MODEL_NONE ||
        (sub->texture_count == 1 && sub->texture_indices[0] == BK_MODEL_NONE)) {
      snprintf(error, 256,
               "submesh %u: unsupported static topology/texture/state", i);
      goto fail;
    }
    BkStaticPart *part = &s->parts[i];
    int ta = sub->texture_count ? alpha[sub->texture_indices[0]] : 0;
    if (!bk_material_state(&m->materials[sub->material_index], ta,
                           &part->material, error))
      goto fail;
    part->priority = read32(h + 76);
    part->sort_bias = read32(h + 80);
    float a = part->material.encoded_alpha;
    int translucent = !(a > 0.999999f && a < 1.000001f);
    /* 0x4228e4..0x4229ce; untextured parts stay in the first queue. */
    part->sorted = sub->texture_count && (translucent || ta);
    part->depth_write = !(sub->texture_count && translucent);
  }
  uint64_t count = 0;
  for (uint32_t i = 0; i < m->frame_count; i++) {
    uint32_t mi = m->frames[i].mesh_index;
    if (mi != BK_MODEL_NONE)
      count += m->meshes[mi].submesh_count;
  }
  /* Native sorting is quadratic. Bound this first static adapter explicitly. */
  if (!count || count > 4096) {
    snprintf(error, 256, "static scene instance count out of supported range");
    goto fail;
  }
  s->instances = calloc((size_t)count, sizeof(*s->instances));
  if (!s->instances)
    goto oom;
  for (uint32_t i = 0; i <= m->frame_count; i++)
    child[i] = BK_MODEL_NONE;
  for (uint32_t i = m->frame_count; i--;) {
    uint32_t parent = m->frames[i].parent_index;
    if (parent == BK_MODEL_NONE)
      parent = m->frame_count;
    next[i] = child[parent];
    child[parent] = i;
  }
  /* File-order siblings, depth-first pre-order, no recursion. */
  uint32_t depth = 0, node = child[m->frame_count];
  while (node != BK_MODEL_NONE || depth) {
    if (node == BK_MODEL_NONE) {
      node = stack[--depth];
      continue;
    }
    uint32_t mi = m->frames[node].mesh_index;
    if (mi != BK_MODEL_NONE) {
      const BkModelMesh *mesh = &m->meshes[mi];
      for (uint32_t j = 0; j < mesh->submesh_count; j++) {
        uint32_t part = mesh->first_submesh + j;
        s->instances[s->instance_count++] = (BkStaticInstance){node, part};
        s->opaque_count += !s->parts[part].sorted;
      }
    }
    if (child[node] != BK_MODEL_NONE) {
      if (next[node] != BK_MODEL_NONE)
        stack[depth++] = next[node];
      node = child[node];
    } else
      node = next[node];
  }
  free(next);
  free(child);
  free(stack);
  return s;
oom:
  snprintf(error, 256, "static model allocation failed");
fail:
  free(next);
  free(child);
  free(stack);
  bk_static_model_destroy(s);
  return NULL;
}
void bk_static_sort_keys(uint32_t *order, const float *distances,
                         const uint32_t *priorities, uint32_t count) {
  for (unsigned pass = 0; pass < 2; pass++)
    for (uint32_t i = 0; i < count; i++)
      for (uint32_t j = i + 1; j < count; j++) {
        uint32_t a = order[i], b = order[j];
        if (pass ? priorities[a] < priorities[b]
                 : distances[a] < distances[b]) {
          order[i] = b;
          order[j] = a;
        }
      }
}
int bk_static_model_order_textures(const BkStaticModel *s, const float view[16],
                                   const uint32_t *textures, uint32_t *order,
                                   float *distances, char error[256]) {
  BkDrawKey keys[4096];
  float scratch[4096];
  if (!s || !view || !order || !distances || s->instance_count > 4096) {
    snprintf(error, 256, "invalid static sort inputs");
    return 0;
  }
  for (uint32_t i = 0; i < s->instance_count; ++i) {
    const BkStaticInstance *inst = &s->instances[i];
    const BkStaticPart *p = &s->parts[inst->submesh];
    const BkModelSubmesh *sub = &s->model->submeshes[inst->submesh];
    uint32_t key = 0;
    if (sub->texture_count) {
      uint32_t index = sub->texture_indices[0];
      key = textures ? textures[index] : index + 1;
    }
    keys[i] = (BkDrawKey){
        .sorted = p->sorted, .priority = p->priority, .texture_key = key};
    if (!bk_draw_distance(&keys[i].distance, s->world + inst->frame * 16, view,
                          p->sort_bias)) {
      snprintf(error, 256, "nonfinite static sort distance");
      return 0;
    }
  }
  if (!bk_draw_order(keys, s->instance_count, order, scratch))
    return 0;
  for (uint32_t i = 0; i < s->instance_count; ++i)
    distances[i] = keys[i].distance;
  return 1;
}
int bk_static_model_order(const BkStaticModel *s, const float view[16],
                          uint32_t *order, float *distances, char error[256]) {
  return bk_static_model_order_textures(s, view, NULL, order, distances, error);
}
