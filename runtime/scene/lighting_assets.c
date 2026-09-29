#include "scene/lighting_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkSceneLighting {
  const BkModel *model;
  BkModelEnvironment *environment;
  BkPassLight registry[BK_PASS_LIGHTS];
  uint32_t ambient, enabled;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "scene lights: %s", message);
  return 0;
}
void bk_scene_lighting_destroy(BkSceneLighting *s) {
  if (!s)
    return;
  bk_model_environment_destroy(s->environment);
  free(s);
}
BkSceneLighting *bk_scene_lighting_create(const BkModel *m, const float *world,
                                          size_t floats, char error[256]) {
  if (!m || !world || floats < (size_t)m->frame_count * 16) {
    fail(error, "missing model/world");
    return NULL;
  }
  BkSceneLighting *s = calloc(1, sizeof(*s));
  if (!s) {
    fail(error, "allocation failed");
    return NULL;
  }
  s->model = m;
  s->environment = bk_model_environment_create(m, world, error);
  if (!s->environment)
    goto bad;
  uint32_t count = s->environment->light_count;
  if (count > BK_PASS_LIGHTS) {
    fail(error, "native registry capacity exceeded");
    goto bad;
  }
  uint8_t ambient[BK_PASS_LIGHTS] = {0};
  for (uint32_t i = 0; i < count; ++i) {
    const BkModelLight *light = &s->environment->lights[i];
    uint32_t parent = m->frames[light->frame_index].parent_index;
    if (parent == BK_MODEL_NONE ||
        !bk_light_group(m->frames[parent].name, "BK3_L",
                        &s->registry[i].group)) {
      fail(error, "light has no classified parent");
      goto bad;
    }
    memcpy(s->registry[i].diffuse, light->diffuse, 12);
    ambient[i] = strlen(light->name) > 6 && light->name[6] == 'A';
  }
  if (!bk_light_ambient_initialize(s->registry, ambient, count, &s->ambient)) {
    fail(error, "invalid initial ambient registry");
    goto bad;
  }
  return s;
bad:
  bk_scene_lighting_destroy(s);
  return NULL;
}
const BkModelEnvironment *
bk_scene_lighting_environment(const BkSceneLighting *s) {
  return s ? s->environment : NULL;
}
const BkModel *bk_scene_lighting_model(const BkSceneLighting *s) {
  return s ? s->model : NULL;
}
int bk_scene_lighting_inherit(BkSceneLighting *to, const BkSceneLighting *from) {
  if (!to || !from || to->model != from->model ||
      to->environment->light_count != from->environment->light_count)
    return 0;
  to->ambient = from->ambient;
  to->enabled = from->enabled;
  return 1;
}
int bk_scene_lighting_save(const BkSceneLighting *s, BkSceneLightDevice *out) {
  if (!s || !out)
    return 0;
  *out = (BkSceneLightDevice){s->environment->light_count, s->ambient, s->enabled};
  return 1;
}
int bk_scene_lighting_restore(BkSceneLighting *s, const BkSceneLightDevice *saved) {
  if (!s || !saved || saved->light_count != s->environment->light_count ||
      saved->light_count > BK_PASS_LIGHTS || saved->light_count >= 32 ||
      (saved->enabled & ~((UINT32_C(1) << saved->light_count) - 1u)))
    return 0;
  s->ambient = saved->ambient;
  s->enabled = saved->enabled;
  return 1;
}
int bk_scene_lighting_input(const BkSceneLighting *s, BkLightingPassInput *in) {
  if (!s || !in)
    return 0;
  in->light_count = s->environment->light_count;
  memcpy(in->lights, s->registry, sizeof(s->registry));
  return 1;
}
int bk_scene_lighting_command(BkSceneLighting *s, const BkLightingCommand *c) {
  if (!s || !c)
    return 0;
  if (c->kind == BK_PASS_AMBIENT) {
    s->ambient = c->value;
    return 1;
  }
  if (c->kind != BK_PASS_LIGHT_ENABLE ||
      c->target >= s->environment->light_count || c->value > 1)
    return 0;
  uint32_t bit = 1u << c->target;
  s->enabled = c->value ? s->enabled | bit : s->enabled & ~bit;
  return 1;
}
int bk_scene_lighting_values(const BkSceneLighting *s, const float *world,
                             size_t floats, BkLighting *out, char error[256]) {
  if (!s || !world || !out || floats < (size_t)s->model->frame_count * 16)
    return fail(error, "invalid published light world");
  BkLighting result = {0};
  for (unsigned j = 0; j < 3; ++j)
    result.ambient[j] = ((s->ambient >> (16 - j * 8)) & 255) / 255.f;
  for (uint32_t i = 0; i < s->environment->light_count; ++i) {
    const BkModelLight *light = &s->environment->lights[i];
    if (!(s->enabled & (1u << i)) || light->type == UINT32_MAX)
      continue;
    BkModelLight published = *light;
    if (!bk_model_light_frame(&published,
                              world + (size_t)light->frame_index * 16, error) ||
        !bk_model_lighting_append(&result, &published, error))
      return 0;
  }
  if (!bk_lighting_validate(&result, error))
    return 0;
  *out = result;
  return 1;
}
