#include "world/bom_motion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "BOM motion: %s", why);
  return 0;
}
static float clamp(float x, float radius) {
  if (x < -(double)radius)
    return -radius;
  if (x > radius)
    return radius;
  return x;
}
static int angles(float out[2], const float xy[2], float degrees, int32_t flip,
                  char e[256]) {
  const double pi = (double)3.1415927410125732f;
  out[1] = (float)((double)xy[0] * 10 * ((double)degrees / 180 * pi) * -1);
  out[0] = (float)((double)xy[1] * 10 * ((double)degrees / 180 * pi));
  if (!flip)
    for (unsigned i = 0; i < 2; i++)
      out[i] = (float)((double)out[i] + pi);
  return (isfinite(out[0]) && isfinite(out[1])) || fail(e, "angle overflow");
}
static int apply(BkNodeReference *node, const float ref[16], const float xy[2],
                 const float angle[2], char e[256]) {
  return bk_node_reference_position(node, ref, (float[3]){xy[0], xy[1], 0},
                                    e) &&
         bk_node_reference_rotation(node, ref, (float[3]){0, 1, 0},
                                    (float)sin((double)angle[1]), e) &&
         bk_node_local_rotation(node, 1, (float[3]){1, 0, 0},
                                (float)sin((double)angle[0]), e);
}
static int align(BkNodeReference *node, const float ref[16], char e[256]) {
  return bk_node_reference_position(node, ref, (float[3]){0, 0, 0}, e) &&
         bk_node_reference_orientation(node, ref, (float[3]){0, 0, 1},
                                       (float[3]){0, 1, 0}, e);
}
int bk_bom_manual_step(BkBomManual *s, BkBomManualKind kind,
                       BkNodeReference *node, const float reference[16],
                       int32_t dx, int32_t dy, float radius, float degrees,
                       int32_t flip, char e[256]) {
  if (!s || (unsigned)kind > BK_BOM_MANUAL_DIRECT)
    return fail(e, "invalid manual state/kind");
  if (!node || !reference)
    return 1;
  if (!isfinite(radius) || !isfinite(degrees) || !isfinite(s->offset[0]) ||
      !isfinite(s->offset[1]))
    return fail(e, "invalid manual values");
  BkBomManual next = *s;
  BkNodeReference n = *node;
  if (reference == node->world)
    reference = n.world;
  unsigned x = kind == BK_BOM_MANUAL_DIRECT ? 1 : 0, y = 1 - x;
  double delta = (double)dx * (double).005f;
  if (kind == BK_BOM_MANUAL_SECOND)
    delta = -delta;
  next.offset[x] = (float)(delta + s->offset[x]);
  next.offset[y] = (float)(-(double)dy * (double).005f + s->offset[y]);
  for (unsigned i = 0; i < 2; i++)
    next.offset[i] = clamp(next.offset[i], radius);
  double length = (double)next.offset[0] * next.offset[0] +
                  (double)next.offset[1] * next.offset[1];
  if (length - 1 < -(double)1e-5f || length - 1 > (double)1e-5f) {
    if (length <= (double)1e-10f)
      next.offset[0] = next.offset[1] = 0;
    else {
      double inv = 1 / sqrt(length);
      for (unsigned i = 0; i < 2; i++)
        next.offset[i] = (float)(next.offset[i] * inv);
    }
  }
  for (unsigned i = 0; i < 2; i++)
    next.offset[i] = (float)((double)next.offset[i] * radius);
  if (!angles(next.angles, next.offset, degrees, flip, e) ||
      !apply(&n, reference, next.offset, next.angles, e))
    return 0;
  *s = next;
  *node = n;
  return 1;
}
int bk_bom_oscillator_step(BkBomOscillator *s, unsigned axis, float distance,
                           uint32_t ms, float *offset, int *done, char e[256]) {
  if (!s || !offset || !done || axis > 3 || !isfinite(distance) ||
      !isfinite(s->amplitude) || !isfinite(s->travel) || !isfinite(s->bound) ||
      !isfinite(s->start))
    return fail(e, "invalid oscillator");
  BkBomOscillator n = *s;
  float v = *offset;
  if (n.fresh) {
    n.amplitude = fabsf(distance);
    n.direction = distance < 0 ? 0 : 1;
    n.bound = (float)((double)n.amplitude * .9f + n.amplitude);
    n.start = n.amplitude;
    n.travel = 0;
    n.fresh = 0;
  }
  n.travel = (float)(((double)n.amplitude * (axis % 2 ? .2f : .4f) +
                      (axis % 2 ? .0045f : .009f)) *
                         ((double)ms / 30) +
                     n.travel);
  if (n.travel >= n.bound) {
    n.amplitude = (float)((double)n.bound - n.start);
    n.start = n.amplitude;
    n.bound = (float)((double)n.amplitude * .9f + n.amplitude);
    n.travel = 0;
    n.direction ^= 1;
  }
  if ((double)n.bound - n.start <= (axis % 2 ? .004f : .008f)) {
    *s = (BkBomOscillator){.fresh = 1};
    *done = 1;
    return 1;
  }
  if (!n.direction)
    v = n.travel < n.start ? (float)((double)n.start - n.travel)
                           : (float)(-((double)n.travel - n.start));
  else
    v = n.travel < n.start ? (float)(-((double)n.start - n.travel))
                           : (float)((double)n.travel - n.start);
  if (!isfinite(n.amplitude) || !isfinite(n.travel) || !isfinite(n.bound) ||
      !isfinite(n.start) || !isfinite(v))
    return fail(e, "oscillator overflow");
  *s = n;
  *offset = v;
  *done = 0;
  return 1;
}
void bk_bom_return_init(BkBomReturn *s) {
  if (s) {
    memset(s, 0, sizeof(*s));
    for (unsigned i = 0; i < 4; i++)
      s->axes[i].fresh = 1;
  }
}
static int spring(BkBomReturn *s, unsigned index, BkNodeReference *node,
                  const float ref[16], const float scene[16], float degrees,
                  int32_t flip, uint32_t ms, char e[256]) {
  float a[3], b[3], angle[2];
  if (!bk_node_reference_offset(a, ref, scene, e) ||
      !bk_node_reference_offset(b, node->world, scene, e))
    return 0;
  for (unsigned j = 0; j < 2; j++)
    if (!s->complete[index][j]) {
      int done;
      if (!bk_bom_oscillator_step(s->axes + index * 2 + j, index * 2 + j,
                                  (float)((double)a[j] - b[j]), ms,
                                  s->offset[index] + j, &done, e))
        return 0;
      s->complete[index][j] = (uint32_t)done;
    }
  return angles(angle, s->offset[index], degrees, flip, e) &&
         apply(node, ref, s->offset[index], angle, e);
}
int bk_bom_return_single(BkBomReturn *s, BkNodeReference *node,
                         const float ref[16], const float scene[16],
                         float degrees, int32_t flip, uint32_t ms,
                         int32_t reset, int *done, char e[256]) {
  if (!s || !done)
    return fail(e, "invalid single return");
  *done = 0;
  if (!node || !ref)
    return 1;
  BkBomReturn next = *s;
  BkNodeReference n = *node;
  if (ref == node->world)
    ref = n.world;
  if (reset) {
    next.complete[0][0] = next.complete[0][1] = 0;
    if (!align(&n, ref, e))
      return 0;
    *s = next;
    *node = n;
    *done = 1;
    return 1;
  }
  /*49b28f checks both completion latches BEFORE applying transient angles. */
  float a[3], b[3], angle[2];
  if (!bk_node_reference_offset(a, ref, scene, e) ||
      !bk_node_reference_offset(b, n.world, scene, e))
    return 0;
  for (unsigned j = 0; j < 2; j++)
    if (!next.complete[0][j]) {
      int value;
      if (!bk_bom_oscillator_step(next.axes + j, j,
                                  (float)((double)a[j] - b[j]), ms,
                                  next.offset[0] + j, &value, e))
        return 0;
      next.complete[0][j] = (uint32_t)value;
    }
  if (next.complete[0][0] && next.complete[0][1]) {
    next.complete[0][0] = next.complete[0][1] = 0;
    if (!align(&n, ref, e))
      return 0;
    *done = 1;
  } else if (!angles(angle, next.offset[0], degrees, flip, e) ||
             !apply(&n, ref, next.offset[0], angle, e))
    return 0;
  *s = next;
  *node = n;
  return 1;
}
int bk_bom_return_multiple(BkBomReturn *s, BkNodeReference *const nodes[2],
                           const float *const refs[2], unsigned count,
                           const float scene[16], float degrees, int32_t flip,
                           uint32_t ms, int *done, char e[256]) {
  if (!s || !nodes || !refs || !done || !count || count > 2)
    return fail(e, "invalid multiple return");
  *done = 0;
  for (unsigned i = 0; i < count; i++) {
    if (!nodes[i] || !refs[i])
      return 1;
    BkBomReturn next = *s;
    BkNodeReference n = *nodes[i];
    const float *ref = refs[i] == nodes[i]->world ? n.world : refs[i];
    if (!spring(&next, i, &n, ref, scene, degrees, flip, ms, e))
      return 0;
    *s = next;
    *nodes[i] = n;
  }
  for (unsigned i = 0; i < count; i++)
    for (unsigned j = 0; j < 2; j++)
      if (!s->complete[i][j])
        return 1;
  for (unsigned i = 0; i < count; i++) {
    s->complete[i][0] = s->complete[i][1] = 0;
    if (!align(nodes[i], refs[i], e))
      return 0;
  }
  *done = 1;
  return 1;
}
