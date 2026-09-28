#include "model/bom_deform.h"
#include "core/matrix.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_BINDINGS 8
#define MAX_MESHES 16
typedef struct {
  uint32_t source, target;
  int32_t group;
  size_t count;
  uint16_t *indices;
  uint32_t *mapping;
} Binding;
struct BkBomDeform {
  size_t count, mesh_count;
  Binding bindings[MAX_BINDINGS];
  uint32_t sizes[MAX_MESHES];
  BkModelVertex *scratch[MAX_MESHES];
};
static int fail(char *error, const char *why) {
  snprintf(error, 256, "BOM deformation: %s", why);
  return 0;
}
static int matrix_valid(const float *m) {
  if (!m)
    return 0;
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(m[i]))
      return 0;
  return 1;
}
static int views_valid(const BkBomMeshView *v, size_t count) {
  if (count > MAX_MESHES || (count && !v))
    return 0;
  for (size_t i = 0; i < count; i++) {
    if (!v[i].vertices || !v[i].count || v[i].count > 2u * 1024 * 1024)
      return 0;
    uintptr_t start = (uintptr_t)v[i].vertices;
    size_t bytes = (size_t)v[i].count * sizeof(BkModelVertex);
    if (start > UINTPTR_MAX - bytes)
      return 0;
    for (size_t j = 0; j < i; j++) {
      uintptr_t other = (uintptr_t)v[j].vertices;
      if (start < other + (size_t)v[j].count * sizeof(BkModelVertex) &&
          other < start + bytes)
        return 0;
    }
  }
  return 1;
}
/* 522b0d stores XYZW to float before its near-unit-W test/divide. */
static int point(float out[3], const float in[3], const float *m) {
  float p[4];
  bk_matrix_point(p, in, m);
  double delta = (double)p[3] - 1;
  if (!isfinite(p[3]))
    return 0;
  if (delta < -(double)1e-5f || delta > (double)1e-5f) {
    if (p[3] == 0)
      return 0;
    double inverse = 1.0 / p[3];
    for (unsigned i = 0; i < 3; i++)
      p[i] = (float)(p[i] * inverse);
  }
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(p[i]))
      return 0;
  memcpy(out, p, 12);
  return 1;
}
void bk_bom_deform_destroy(BkBomDeform *d) {
  if (!d)
    return;
  for (size_t i = 0; i < d->count; i++) {
    free(d->bindings[i].indices);
    free(d->bindings[i].mapping);
  }
  for (size_t i = 0; i < d->mesh_count; i++)
    free(d->scratch[i]);
  free(d);
}
BkBomDeform *bk_bom_deform_create(const BkBomMeshView *v, size_t meshes,
                                  const BkBomDeformBinding *bindings,
                                  size_t count, char error[256]) {
  if (!views_valid(v, meshes) || count > MAX_BINDINGS || (count && !bindings)) {
    fail(error, "invalid registry or binding count");
    return NULL;
  }
  BkBomDeform *d = calloc(1, sizeof(*d));
  if (!d) {
    fail(error, "allocation failed");
    return NULL;
  }
  d->count = count;
  d->mesh_count = meshes;
  for (size_t i = 0; i < meshes; i++) {
    d->sizes[i] = v[i].count;
    d->scratch[i] = malloc((size_t)v[i].count * sizeof(BkModelVertex));
    if (!d->scratch[i]) {
      fail(error, "scratch allocation failed");
      goto bad;
    }
  }
  unsigned transformed[MAX_MESHES] = {0};
  int32_t groups = 0;
  for (size_t i = 0; i < count; i++) {
    Binding *b = &d->bindings[i];
    const BkBomDeformBinding *in = &bindings[i];
    b->source = in->source;
    b->target = in->target;
    b->group = groups;
    for (size_t j = 0; j < i; j++)
      if (d->bindings[j].target == b->target) {
        b->group = d->bindings[j].group;
        break;
      }
    if (b->group == groups)
      groups++;
    if ((b->source != BK_MODEL_NONE && b->source >= meshes) ||
        (b->target != BK_MODEL_NONE && b->target >= meshes)) {
      fail(error, "invalid mesh index");
      goto bad;
    }
    if (b->source == BK_MODEL_NONE || b->target == BK_MODEL_NONE)
      continue;
    if (in->count > 2u * 1024 * 1024 || (in->count && !in->indices) ||
        !matrix_valid(v[b->source].world)) {
      fail(error, "invalid selection or source world");
      goto bad;
    }
    b->count = in->count;
    if (!b->count)
      continue;
    b->indices = malloc(b->count * sizeof(*b->indices));
    b->mapping = malloc(b->count * sizeof(*b->mapping));
    if (!b->indices || !b->mapping) {
      fail(error, "mapping allocation failed");
      goto bad;
    }
    memcpy(b->indices, in->indices, b->count * sizeof(*b->indices));
    /* Initialization reads an unchanging mesh/world. Transform each source
     * once, preserving the original stored-float result for every distance. */
    if (!transformed[b->source]) {
      for (uint32_t s = 0; s < v[b->source].count; s++)
        if (!point(d->scratch[b->source][s].position,
                   v[b->source].vertices[s].position, v[b->source].world)) {
          fail(error, "nonfinite source position");
          goto bad;
        }
      transformed[b->source] = 1;
    }
    for (size_t k = 0; k < b->count; k++) {
      uint32_t index = b->indices[k];
      if (index >= v[b->target].count) {
        fail(error, "target index outside mesh");
        goto bad;
      }
      const float *target = v[b->target].vertices[index].position;
      float nearest = 100000;
      uint32_t mapped = BK_MODEL_NONE;
      for (uint32_t s = 0; s < v[b->source].count; s++) {
        float p[3];
        for (unsigned a = 0; a < 3; a++)
          p[a] =
              (float)((double)target[a] - d->scratch[b->source][s].position[a]);
        float squared = (float)((double)p[0] * p[0] + (double)p[1] * p[1] +
                                (double)p[2] * p[2]);
        float distance = (float)sqrt((double)squared);
        if (!isfinite(distance)) {
          fail(error, "nonfinite vertex distance");
          goto bad;
        }
        if (distance < nearest) {
          nearest = distance;
          mapped = s;
        }
      }
      if (mapped == BK_MODEL_NONE) {
        fail(error, "no source vertex within native search radius");
        goto bad;
      }
      b->mapping[k] = mapped;
    }
  }
  return d;
bad:
  bk_bom_deform_destroy(d);
  return NULL;
}
int bk_bom_deform_mapping(const BkBomDeform *d, size_t binding, int32_t *group,
                          const uint32_t **sources, size_t *count) {
  if (!d || binding >= d->count || !group || !sources || !count)
    return 0;
  *group = d->bindings[binding].group;
  *sources = d->bindings[binding].mapping;
  *count = d->bindings[binding].count;
  return 1;
}
int bk_bom_deform_plan(const BkBomDeform *d, size_t binding,
                       BkBomDeformBinding *out) {
  if (!d || binding >= d->count || !out)
    return 0;
  const Binding *b = d->bindings + binding;
  *out = (BkBomDeformBinding){b->source, b->target, b->indices, b->count};
  return 1;
}
int bk_bom_deform_draw(BkBomDeform *d, uint32_t target, const BkBomMeshView *v,
                       size_t meshes, const int32_t *disabled,
                       char error[256]) {
  if (!d || meshes != d->mesh_count || !views_valid(v, meshes) ||
      (d->count && !disabled))
    return fail(error, "invalid draw registry/flags");
  int32_t group = 0;
  for (size_t i = 0; i < d->count; i++)
    if (d->bindings[i].target == target) {
      group = d->bindings[i].group;
      break;
    }
  unsigned used[MAX_MESHES] = {0}, changed[MAX_MESHES] = {0};
  for (size_t i = 0; i < d->count; i++) {
    const Binding *b = &d->bindings[i];
    if (b->group == group && b->source != BK_MODEL_NONE &&
        b->target != BK_MODEL_NONE && disabled[i] != 1 && b->count)
      used[b->source] = used[b->target] = 1;
  }
  for (size_t i = 0; i < meshes; i++) {
    if (v[i].count != d->sizes[i])
      return fail(error, "changed mesh size");
    if (used[i])
      memcpy(d->scratch[i], v[i].vertices,
             (size_t)v[i].count * sizeof(BkModelVertex));
  }
  for (size_t i = 0; i < d->count; i++) {
    const Binding *b = &d->bindings[i];
    if (b->group != group || b->source == BK_MODEL_NONE ||
        b->target == BK_MODEL_NONE || disabled[i] == 1)
      continue;
    const float *m = v[b->source].world;
    if (!matrix_valid(m))
      return fail(error, "invalid cached source world");
    for (size_t j = 0; j < b->count; j++) {
      BkModelVertex *out = d->scratch[b->target] + b->indices[j];
      const BkModelVertex *in = d->scratch[b->source] + b->mapping[j];
      if (!point(out->position, in->position, m))
        return fail(error, "position transform overflow");
      float normal[3];
      for (unsigned a = 0; a < 3; a++) {
        normal[a] = (float)((double)in->normal[0] * m[a] +
                            (double)in->normal[1] * m[4 + a] +
                            (double)in->normal[2] * m[8 + a]);
        if (!isfinite(normal[a]))
          return fail(error, "normal transform overflow");
      }
      memcpy(out->normal, normal, 12);
    }
    if (b->count)
      changed[b->target] = 1;
  }
  for (size_t i = 0; i < meshes; i++)
    if (changed[i])
      memcpy(v[i].vertices, d->scratch[i],
             (size_t)v[i].count * sizeof(BkModelVertex));
  return 1;
}
