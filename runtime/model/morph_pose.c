#include "model/morph_pose.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkMorphMesh {
  uint32_t count;
  BkModelVertex *vertices, *pending;
};
struct BkMorphBinding {
  const BkModelMorph *morph;
  uint32_t track;
  BkMorphMesh *target;
  uint16_t *selection;
  size_t selection_count;
  int selected, loop;
  float time;
};
typedef struct {
  const BkMorphKey *a, *b;
  float time, ratio;
  int uv_right;
} Range;
static int fail(char *error, const char *message) {
  snprintf(error, 256, "MORP pose: %s", message);
  return 0;
}
static int finite_vertex(const BkModelVertex *v) {
  float fields[9];
  memcpy(fields, v, sizeof(fields));
  for (unsigned j = 0; j < 9; j++)
    if (!isfinite(fields[j]))
      return 0;
  return 1;
}
void bk_morph_mesh_destroy(BkMorphMesh *m) {
  if (m) {
    free(m->vertices);
    free(m->pending);
    free(m);
  }
}
BkMorphMesh *bk_morph_mesh_create(const BkModelSubmesh *source,
                                  char error[256]) {
  if (!source || !source->vertices || !source->vertex_count ||
      source->vertex_count > 2u * 1024 * 1024) {
    fail(error, "invalid mesh");
    return NULL;
  }
  for (uint32_t i = 0; i < source->vertex_count; i++)
    if (!finite_vertex(source->vertices + i)) {
      fail(error, "nonfinite mesh");
      return NULL;
    }
  BkMorphMesh *m = calloc(1, sizeof(*m));
  if (!m) {
    fail(error, "allocation failed");
    return NULL;
  }
  m->count = source->vertex_count;
  size_t bytes = (size_t)m->count * sizeof(*m->vertices);
  m->vertices = malloc(bytes);
  m->pending = malloc(bytes);
  if (!m->vertices || !m->pending) {
    fail(error, "vertex allocation failed");
    bk_morph_mesh_destroy(m);
    return NULL;
  }
  memcpy(m->vertices, source->vertices, bytes);
  return m;
}
const BkModelVertex *bk_morph_mesh_vertices(const BkMorphMesh *m) {
  return m ? m->vertices : NULL;
}
uint32_t bk_morph_mesh_count(const BkMorphMesh *m) {
  return m ? m->count : 0;
}
BkMorphBinding *bk_morph_binding_create(const BkModelMorph *m, uint32_t track,
                                        BkMorphMesh *target, char error[256]) {
  const BkMorphTrack *t = bk_model_morph_track(m, track);
  if (!t || !target) {
    fail(error, "missing source track or target");
    return NULL;
  }
  for (uint32_t k = 0; k < t->key_count; k++)
    if (bk_model_morph_key(m, track, k)->vertex_count != target->count) {
      fail(error, "retarget vertex count mismatch");
      return NULL;
    }
  BkMorphBinding *b = calloc(1, sizeof(*b));
  if (!b) {
    fail(error, "binding allocation failed");
    return NULL;
  }
  b->morph = m;
  b->track = track;
  b->target = target;
  b->loop = 1;
  return b;
}
void bk_morph_binding_destroy(BkMorphBinding *b) {
  if (b) {
    free(b->selection);
    free(b);
  }
}
int bk_morph_binding_loop(BkMorphBinding *b, int loop, char error[256]) {
  if (!b || (loop != 0 && loop != 1))
    return fail(error, "invalid loop policy");
  b->loop = loop;
  return 1;
}
int bk_morph_binding_selection(BkMorphBinding *b, int enabled,
                                const uint16_t *indices, size_t count,
                                char error[256]) {
  if (!b || (enabled != 0 && enabled != 1) || (count && !indices) ||
      count > 2u * 1024 * 1024)
    return fail(error, "invalid selection");
  for (size_t i = 0; i < count; i++)
    if (indices[i] >= b->target->count)
      return fail(error, "selection outside target");
  uint16_t *next = count ? malloc(count * sizeof(*next)) : NULL;
  if (count && !next)
    return fail(error, "selection allocation failed");
  if (count)
    memcpy(next, indices, count * sizeof(*next));
  free(b->selection);
  b->selection = next;
  b->selection_count = count;
  b->selected = enabled;
  return 1;
}
static float lerp(float a, float b, float t) {
  return (float)(((double)b - a) * t + a);
}
static float interpolate(float a, float b, float t, uint32_t mode) {
  float x = lerp(a, b, t);
  if (mode == 1) {
    float y = lerp(x, b, t);
    x = lerp(a, x, t);
    return lerp(x, y, t);
  }
  if (mode == 2)
    return lerp(a, lerp(x, b, t), t);
  if (mode == 3)
    return lerp(lerp(a, x, t), b, t);
  return x;
}
static int locate(const BkMorphBinding *b, float time, Range *r, char *error) {
  if (!isfinite(time) || time < 0)
    return fail(error, "invalid source time");
  const BkMorphTrack *track = bk_model_morph_track(b->morph, b->track);
  const BkMorphKey *last = bk_model_morph_key(b->morph, b->track, track->key_count - 1);
  if (b->loop && last->time >= 1 && time > last->time) {
    if ((double)time >= INT32_MAX || (double)last->time >= INT32_MAX)
      return fail(error, "loop conversion overflow");
    time = (float)((int32_t)time % (int32_t)last->time);
  }
  uint32_t lo = 0, hi = track->key_count;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (bk_model_morph_key(b->morph, b->track, mid)->time <= time)
      lo = mid + 1;
    else
      hi = mid;
  }
  if (!lo)
    return fail(error, "time before initial key");
  r->a = bk_model_morph_key(b->morph, b->track, lo - 1);
  r->b = lo < track->key_count && r->a->time != time
             ? bk_model_morph_key(b->morph, b->track, lo) : NULL;
  r->time = time;
  double ratio = r->b ? ((double)time - r->a->time) /
                           ((double)r->b->time - r->a->time) : 0;
  r->ratio = (float)ratio;
  r->uv_right = ratio >= .5;
  return 1;
}
static void plain_vertex(BkModelVertex *out, uint32_t i, const Range *r) {
  const BkModelVertex *a = r->a->vertices + i;
  if (!r->b) {
    memcpy(out, a, sizeof(*out));
    return;
  }
  const BkModelVertex *b = r->b->vertices + i;
  for (unsigned j = 0; j < 3; j++) {
    out->position[j] = interpolate(a->position[j], b->position[j], r->ratio,
                                    r->a->interpolation);
    out->normal[j] = interpolate(a->normal[j], b->normal[j], r->ratio,
                                  r->a->interpolation);
  }
  memcpy(out->uv[0], (r->uv_right ? b : a)->uv[0], 8);
}
static int evaluate(BkMorphBinding *b, const BkMorphSample *sample,
                    float *time, char error[256]) {
  if (!b || !sample || (sample->blend != 0 && sample->blend != 1) ||
      (sample->blend && !isfinite(sample->weight)))
    return fail(error, "invalid sample");
  Range from, to;
  if (!locate(b, sample->from, &from, error) ||
      (from.b && from.a->interpolation > 3))
    return fail(error, "unsupported source interpolation/time");
  if (sample->blend && (!locate(b, sample->to, &to, error) ||
                         to.a->interpolation > 3))
    return fail(error, "unsupported destination interpolation/time");
  BkMorphMesh *m = b->target;
  size_t count = b->selected ? b->selection_count : m->count;
  for (size_t j = 0; j < count; j++) {
    uint32_t i = b->selected ? b->selection[j] : (uint32_t)j;
    plain_vertex(m->pending + i, i, &from);
  }
  if (sample->blend)
    for (size_t j = 0; j < count; j++) {
      uint32_t i = b->selected ? b->selection[j] : (uint32_t)j;
      BkModelVertex *out = m->pending + i;
      const BkModelVertex *a = to.a->vertices + i;
      const BkModelVertex *z = to.b ? to.b->vertices + i : a;
      for (unsigned c = 0; c < 3; c++) {
        float destination = to.b ? interpolate(a->position[c], z->position[c],
                                                 to.ratio, to.a->interpolation)
                                  : a->position[c];
        out->position[c] = interpolate(out->position[c], destination,
                                        sample->weight, to.a->interpolation);
        if (to.b) {
          destination = interpolate(a->normal[c], z->normal[c], to.ratio,
                                      to.a->interpolation);
          out->normal[c] = interpolate(out->normal[c], destination,
                                        sample->weight, to.a->interpolation);
        }
      }
      if (sample->weight >= .5f)
        memcpy(out->uv[0], z->uv[0], 8);
    }
  for (size_t j = 0; j < count; j++) {
    uint32_t i = b->selected ? b->selection[j] : (uint32_t)j;
    if (!finite_vertex(m->pending + i))
      return fail(error, "nonfinite evaluated vertex");
  }
  *time = from.time;
  return 1;
}
int bk_morph_bindings_apply(BkMorphBinding *const *bindings,
                             const BkMorphSample *samples, size_t count,
                             char error[256]) {
  if (count > 64 || (count && (!bindings || !samples)))
    return fail(error, "invalid batch");
  BkMorphMesh *meshes[64];
  float times[64];
  size_t mesh_count = 0;
  for (size_t i = 0; i < count; i++) {
    if (!bindings[i])
      return fail(error, "missing batch binding");
    BkMorphMesh *m = bindings[i]->target;
    size_t j = 0;
    while (j < mesh_count && meshes[j] != m)
      j++;
    if (j == mesh_count) {
      meshes[mesh_count++] = m;
      memcpy(m->pending, m->vertices, (size_t)m->count * sizeof(*m->vertices));
    }
  }
  for (size_t i = 0; i < count; i++)
    if (!evaluate(bindings[i], &samples[i], &times[i], error))
      return 0;
  for (size_t i = 0; i < mesh_count; i++) {
    BkModelVertex *old = meshes[i]->vertices;
    meshes[i]->vertices = meshes[i]->pending;
    meshes[i]->pending = old;
  }
  for (size_t i = 0; i < count; i++)
    if (!samples[i].blend)
      bindings[i]->time = times[i];
  return 1;
}
int bk_morph_binding_apply(BkMorphBinding *b, const BkMorphSample *sample,
                            char error[256]) {
  return bk_morph_bindings_apply(&b, sample, 1, error);
}
int bk_morph_binding_vix(BkMorphBinding *b, const uint8_t *data,
                         size_t size, char error[256]) {
  if (!b || (size && !data) || size / 2 > 2u * 1024 * 1024)
    return fail(error, "invalid VIX");
  if (!size)
    return 1;
  size_t count = size / 2;
  uint16_t *indices = count ? malloc(count * sizeof(*indices)) : NULL;
  if (count && !indices)
    return fail(error, "VIX allocation failed");
  for (size_t i = 0; i < count; i++)
    indices[i] = (uint16_t)((uint16_t)data[i * 2] |
                            (uint16_t)data[i * 2 + 1] << 8);
  int ok = bk_morph_binding_selection(b, 1, indices, count, error);
  free(indices);
  return ok;
}
float bk_morph_binding_time(const BkMorphBinding *b) {
  return b ? b->time : 0;
}
