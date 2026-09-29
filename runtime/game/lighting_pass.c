#include "game/lighting_pass.h"
#include <math.h>
#include <string.h>
static int valid_light(const BkPassLight *l) {
  if (l->group < 1 || l->group > 2 || l->ambient_rank < 0 ||
      l->ambient_rank > 2)
    return 0;
  for (unsigned j = 0; j < 3; ++j)
    /* Authored selection lights exceed1 and third-ending lights are signed.
     * Bound the original signed integer conversion, not the light color. */
    if (!isfinite(l->diffuse[j]) ||
        (double)l->diffuse[j] * 255 < INT32_MIN ||
        (double)l->diffuse[j] * 255 > INT32_MAX)
      return 0;
  return 1;
}
static uint32_t color(const BkPassLight *l) {
  uint32_t result = 0xff000000;
  for (unsigned i = 0; i < 3; ++i)
    result |= (uint32_t)(int32_t)((double)l->diffuse[i] * 255) << (16 - i * 8);
  return result;
}
static unsigned lower(unsigned c) {
  return c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c;
}
int bk_light_group(const char *parent, const char *key, int32_t *out) {
  if (!parent || !key || !out)
    return 0;
  size_t n = 0, k = 0;
  while (n <= 64 && parent[n])
    ++n;
  while (k <= 64 && key[k])
    ++k;
  if (n > 64 || k > 64)
    return 0;
  size_t b = 0;
  while (b < n && parent[b] != 'B')
    ++b;
  int same = n - b == k;
  for (size_t i = 0; same && i < k; ++i)
    same = lower((unsigned char)parent[b + i]) == lower((unsigned char)key[i]);
  *out = same ? 1 : 2;
  return 1;
}
int bk_light_ambient_initialize(BkPassLight *lights, const uint8_t *names,
                                uint32_t count, uint32_t *argb) {
  if (!lights || !names || !argb || count > BK_PASS_LIGHTS)
    return 0;
  for (uint32_t i = 0; i < count; ++i)
    if (!valid_light(&lights[i]) || names[i] > 1)
      return 0;
  int selected = -1;
  /* Original two traversals prioritize an ambient attached to BK3_L. */
  for (uint32_t i = 0; i < count; ++i)
    if (names[i] && lights[i].group == 1) {
      if (selected < 0)
        selected = (int)i;
      lights[i].ambient_rank = 1;
    }
  for (uint32_t i = 0; i < count; ++i)
    if (names[i]) {
      if (selected < 0)
        selected = (int)i;
      lights[i].ambient_rank = 1;
    }
  *argb = selected < 0 ? 0xff000000 : color(&lights[selected]);
  return 1;
}
static void emit(BkLightingPass *p, uint32_t kind, uint32_t target,
                 uint32_t value) {
  p->commands[p->count++] = (BkLightingCommand){kind, target, value};
}
static void lights(BkLightingPass *p, const BkLightingPassInput *in,
                   unsigned selected) {
  for (uint32_t i = 0; i < in->light_count; ++i)
    if (in->lights[i].ambient_rank != 1)
      emit(p, BK_PASS_LIGHT_ENABLE, i,
           !selected || (unsigned)in->lights[i].group == selected);
}
static void objects(BkLightingPass *p, const BkLightingPassInput *in,
                    unsigned first, unsigned count) {
  for (unsigned i = first; i < first + count; ++i)
    if (in->objects[i]) {
      emit(p, BK_PASS_OBJECT, in->objects[i], 0);
      emit(p, BK_PASS_FLUSH, 0, 1);
    }
}
int bk_lighting_pass(const BkLightingPassInput *in, BkLightingPass *out) {
  if (!in || !out || in->light_count > BK_PASS_LIGHTS)
    return 0;
  for (uint32_t i = 0; i < in->light_count; ++i)
    if (!valid_light(&in->lights[i]))
      return 0;
  BkLightingPass p = {0};
  emit(&p, BK_PASS_AMBIENT, 0, 0xff000000);
  int ambient = -1;
  for (int rank = 1; rank <= 2 && ambient < 0; ++rank)
    for (uint32_t i = 0; i < in->light_count; ++i)
      if (in->lights[i].ambient_rank == rank) {
        ambient = (int)i;
        break;
      }
  if (ambient >= 0)
    emit(&p, BK_PASS_AMBIENT, 0, color(&in->lights[ambient]));
  switch (in->mode) {
  case 0:
    lights(&p, in, 0);
    emit(&p, BK_PASS_OBJECT, in->scene_root, 0);
    emit(&p, BK_PASS_FLUSH, 0, 1);
    if (in->shadow_mode == 2)
      emit(&p, BK_PASS_PROJECTED_SHADOW, 0, 0);
    break;
  case 1:
    lights(&p, in, 1);
    objects(&p, in, 0, 4);
    lights(&p, in, 2);
    objects(&p, in, 4, 16);
    break;
  case 2:
    lights(&p, in, 2);
    objects(&p, in, 20, 32);
    break;
  case 10:
    lights(&p, in, 2);
    objects(&p, in, 4, 16);
    lights(&p, in, 1);
    objects(&p, in, 0, 4);
    break;
  default:
    break;
  }
  *out = p;
  return 1;
}
