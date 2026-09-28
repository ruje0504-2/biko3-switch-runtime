#include "model/environment.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static float f32(const uint8_t *p) {
  uint32_t v = u32(p);
  float f;
  memcpy(&f, &v, 4);
  return f;
}
static void floats(float *out, const uint8_t *p, unsigned n) {
  for (unsigned i = 0; i < n; i++)
    out[i] = f32(p + 4 * i);
}
void bk_model_environment_destroy(BkModelEnvironment *e) {
  if (e) {
    free(e->lights);
    free(e);
  }
}
int bk_model_light_frame(BkModelLight *light, const float world[16],
                         char error[256]) {
  if (!light || !world ||
      (light->type != 1 && light->type != 2 && light->type != UINT32_MAX))
    goto invalid;
  BkModelLight result = *light;
  if (result.type == 1 || result.type == 2) {
    for (unsigned i = 0; i < 3; ++i) {
      if (!isfinite(world[12 + i]))
        goto invalid;
      result.position[i] = world[12 + i];
    }
  }
  if (result.type == 2) {
    float v[3] = {-world[2], -world[6], world[10]};
    double norm =
        (double)v[1] * v[1] + (double)v[2] * v[2] + (double)v[0] * v[0];
    if (!isfinite(norm) || norm <= (double)1e-10f)
      goto invalid;
    if (norm - 1 < -(double)1e-5f || norm - 1 > (double)1e-5f) {
      double inverse = 1 / sqrt(norm);
      for (unsigned j = 0; j < 3; ++j)
        v[j] = (float)(v[j] * inverse);
    }
    memcpy(result.direction, v, sizeof(v));
  }
  *light = result;
  return 1;
invalid:
  snprintf(error, 256, "invalid light frame or degenerate spotlight direction");
  return 0;
}
int bk_model_lighting_append(BkLighting *out, const BkModelLight *light,
                             char error[256]) {
  if (!out || !light || out->point_count >= BK_MAX_POINT_LIGHTS ||
      out->spot_count >= BK_MAX_POINT_LIGHTS ||
      out->point_count + out->spot_count >= BK_MAX_POINT_LIGHTS ||
      (light->type != 1 && light->type != 2)) {
    snprintf(error, 256, "unsupported light type/count");
    return 0;
  }
  BkPointLight *p;
  if (light->type == 1)
    p = &out->points[out->point_count++];
  else {
    BkSpotLight *spot = &out->spots[out->spot_count++];
    memcpy(spot->direction, light->direction, sizeof(spot->direction));
    spot->falloff = light->falloff;
    spot->theta = light->theta;
    spot->phi = light->phi;
    p = &spot->point;
  }
  memcpy(p->position, light->position, sizeof(p->position));
  memcpy(p->diffuse, light->diffuse, sizeof(p->diffuse));
  memcpy(p->ambient, light->ambient, sizeof(p->ambient));
  memcpy(p->specular, light->specular, sizeof(p->specular));
  p->range = light->range;
  p->attenuation0 = light->attenuation[0];
  p->attenuation1 = light->attenuation[1];
  p->attenuation2 = light->attenuation[2];
  return 1;
}
BkModelEnvironment *bk_model_environment_create(const BkModel *m,
                                                const float *world,
                                                char error[256]) {
  BkModelEnvironment *e = NULL;
  if (!m || !world)
    goto invalid;
  const BkModelChunk *lc = bk_model_chunk(m, "LIGH"),
                     *fc = bk_model_chunk(m, "FRAM"),
                     *fog = bk_model_chunk(m, "FOG ");
  if (!lc || !fc || !lc->size || lc->size % 172 || lc->size / 172 > 256 ||
      fc->size != (uint64_t)m->frame_count * 396 || (fog && fog->size != 32))
    goto invalid;
  e = calloc(1, sizeof(*e));
  if (!e)
    goto oom;
  e->light_count = lc->size / 172;
  e->lights = calloc(e->light_count, sizeof(*e->lights));
  if (!e->lights)
    goto oom;
  if (fog) {
    const uint8_t *p = m->source + fog->offset;
    e->fog = (BkModelFog){u32(p),      u32(p + 4),  u32(p + 8),  u32(p + 12),
                          u32(p + 16), f32(p + 20), f32(p + 24), f32(p + 28)};
    if (!bk_fog_validate(&e->fog, error))
      goto fail;
  }
  for (uint32_t i = 0; i < e->light_count; i++) {
    BkModelLight *l = &e->lights[i];
    const uint8_t *p = m->source + lc->offset + i * 172;
    memcpy(l->name, p, 64);
    l->name[64] = 0;
    l->id = u32(p + 64);
    l->type = u32(p + 68);
    l->frame_index = BK_MODEL_NONE;
    if (!l->id)
      goto invalid;
    for (uint32_t j = 0; j < i; j++)
      if (l->id == e->lights[j].id)
        goto invalid;
    for (unsigned j = 72; j < 172; j += 4)
      if (!isfinite(f32(p + j)))
        goto invalid;
    floats(l->diffuse, p + 72, 4);
    floats(l->specular, p + 88, 4);
    floats(l->ambient, p + 104, 4);
    floats(l->position, p + 120, 3);
    floats(l->direction, p + 132, 3);
    l->range = f32(p + 144);
    l->falloff = f32(p + 148);
    floats(l->attenuation, p + 152, 3);
    l->theta = f32(p + 164);
    l->phi = f32(p + 168);
  }
  for (uint32_t f = 0; f < m->frame_count; f++) {
    uint32_t id = u32(m->source + fc->offset + f * 396 + 180);
    if (!id)
      continue;
    uint32_t i = 0;
    while (i < e->light_count && e->lights[i].id != id)
      i++;
    if (i == e->light_count || e->lights[i].frame_index != BK_MODEL_NONE)
      goto invalid;
    BkModelLight *l = &e->lights[i];
    l->frame_index = f;
    if (!bk_model_light_frame(l, world + f * 16, error))
      goto fail;
  }
  for (uint32_t i = 0; i < e->light_count; i++) {
    BkModelLight *l = &e->lights[i];
    if (l->frame_index == BK_MODEL_NONE) {
      snprintf(error, 256, "unattached static light is unsupported");
      goto fail;
    }
    if (l->type == UINT32_MAX) {
      if (++e->ambient_count > 1) {
        snprintf(error, 256,
                 "multiple ambient lights require original selection policy");
        goto fail;
      }
      for (unsigned j = 0; j < 3; j++) {
        if (l->diffuse[j] < 0 || l->diffuse[j] > 1)
          goto invalid;
        /* Original x87 float*255 then truncation, NOT rounding to nearest. */
        e->lighting.ambient[j] =
            (unsigned)((double)l->diffuse[j] * 255) / 255.0f;
      }
    } else if (!bk_model_lighting_append(&e->lighting, l, error))
      goto fail;
  }
  if (!bk_lighting_validate(&e->lighting, error))
    goto fail;
  return e;
invalid:
  snprintf(error, 256, "invalid LIGH/FOG records or frame light references");
  goto fail;
oom:
  snprintf(error, 256, "environment allocation failed");
fail:
  bk_model_environment_destroy(e);
  return NULL;
}
