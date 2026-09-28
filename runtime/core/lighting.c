#include "core/lighting.h"
#include <math.h>
#include <stdio.h>
int bk_lighting_validate(const BkLighting *s, char error[256]) {
  if (!s || s->point_count > BK_MAX_POINT_LIGHTS ||
      s->spot_count > BK_MAX_POINT_LIGHTS ||
      s->point_count + s->spot_count > BK_MAX_POINT_LIGHTS)
    goto invalid;
  for (unsigned j = 0; j < 3; j++)
    if (!isfinite(s->ambient[j]) || s->ambient[j] < 0)
      goto invalid;
  for (unsigned i = 0; i < s->point_count + s->spot_count; i++) {
    const BkSpotLight *spot =
        i < s->point_count ? NULL : &s->spots[i - s->point_count];
    const BkPointLight *p = spot ? &spot->point : &s->points[i];
    for (unsigned j = 0; j < 3; j++)
      if (!isfinite(p->position[j]) || !isfinite(p->diffuse[j]) ||
          !isfinite(p->ambient[j]) || !isfinite(p->specular[j]) ||
          p->diffuse[j] < 0 || p->ambient[j] < 0 || p->specular[j] < 0)
        goto invalid;
    if (!isfinite(p->range) || p->range < 0 || !isfinite(p->attenuation0) ||
        !isfinite(p->attenuation1) || !isfinite(p->attenuation2) ||
        p->attenuation0 < 0 || p->attenuation1 < 0 || p->attenuation2 < 0 ||
        !(p->attenuation0 > 0 || p->attenuation1 > 0 || p->attenuation2 > 0))
      goto invalid;
    if (spot) {
      double norm = 0;
      for (unsigned j = 0; j < 3; ++j) {
        if (!isfinite(spot->direction[j]))
          goto invalid;
        norm += (double)spot->direction[j] * spot->direction[j];
      }
      if (norm <= 1e-10 || !isfinite(spot->falloff) || spot->falloff < 0 ||
          !isfinite(spot->theta) || !isfinite(spot->phi) || spot->theta < 0 ||
          spot->theta > spot->phi || spot->phi > 3.141592653589793f)
        goto invalid;
    }
  }
  return 1;
invalid:
  snprintf(error, 256, "invalid or unsupported point/spot lighting values");
  return 0;
}
