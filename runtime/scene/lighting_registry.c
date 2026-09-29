#include "scene/lighting_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkSceneLightRegistry {
  BkSceneLighting *sources[BK_PASS_LIGHTS];
  uint32_t source_count, light_count, ambient;
  BkPassLight lights[BK_PASS_LIGHTS];
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "light registry: %s", why);
  return 0;
}
void bk_scene_light_registry_destroy(BkSceneLightRegistry *r) {
  if (!r)
    return;
  for (uint32_t i = 0; i < r->source_count; ++i)
    bk_scene_lighting_destroy(r->sources[i]);
  free(r);
}
int bk_scene_light_registry_reset(BkSceneLightRegistry *r) {
  if (!r)
    return 0;
  memset(r->lights, 0, sizeof(r->lights));
  r->light_count = 0;
  return 1;
}
int bk_scene_light_registry_select_bk3_l(BkSceneLightRegistry *r) {
  if (!bk_scene_light_registry_reset(r))
    return 0;
  for (uint32_t i = 0; i < r->source_count; ++i) {
    BkLightingPassInput source = {0};
    if (!bk_scene_lighting_input(r->sources[i], &source) ||
        source.light_count > BK_PASS_LIGHTS - r->light_count)
      return 0;
    for (uint32_t j = 0; j < source.light_count; ++j) {
      r->lights[r->light_count] = source.lights[j];
      r->lights[r->light_count++].ambient_rank = 0;
    }
  }
  return 1;
}
int bk_scene_light_registry_ambient(BkSceneLightRegistry *r) {
  if (!r)
    return 0;
  uint8_t names[BK_PASS_LIGHTS] = {0};
  uint32_t index = 0;
  for (uint32_t i = 0; i < r->source_count && index < r->light_count; ++i) {
    const BkModelEnvironment *env = bk_scene_lighting_environment(r->sources[i]);
    if (!env || env->light_count > r->light_count - index)
      return 0;
    for (uint32_t j = 0; j < env->light_count; ++j)
      names[index++] = strlen(env->lights[j].name) > 6 && env->lights[j].name[6] == 'A';
  }
  return index == r->light_count &&
         bk_light_ambient_initialize(r->lights, names, r->light_count, &r->ambient);
}
int bk_scene_light_registry_inherit(BkSceneLightRegistry *to,
                                     const BkSceneLightRegistry *from,
                                     char e[256]) {
  if (!to || !from || to == from)
    return fail(e, "invalid retained light state owners");
  for (uint32_t i = 0; i < to->source_count; ++i) {
    const BkModel *model = bk_scene_lighting_model(to->sources[i]);
    for (uint32_t j = 0; j < from->source_count; ++j)
      if (model == bk_scene_lighting_model(from->sources[j]) &&
          !bk_scene_lighting_inherit(to->sources[i], from->sources[j]))
        return fail(e, "retained light model changed its layout");
  }
  to->ambient = from->ambient;
  return 1;
}
int bk_scene_light_registry_save(const BkSceneLightRegistry *r,
                                 const BkModel *model,
                                 BkRetainedLightState *out, char e[256]) {
  if (!r || !out)
    return fail(e, "missing retained light snapshot input/output");
  BkRetainedLightState next;
  memset(&next, 0, sizeof(next));
  next.ambient = r->ambient;
  next.model = model;
  for (uint32_t i = 0; i < r->source_count; ++i)
    if (model && model == bk_scene_lighting_model(r->sources[i])) {
      if (next.has_model || !bk_scene_lighting_save(r->sources[i], &next.device))
        return fail(e, "ambiguous retained light model");
      next.has_model = 1;
    }
  *out = next;
  return 1;
}
int bk_scene_light_registry_restore(BkSceneLightRegistry *r,
                                    const BkRetainedLightState *saved, char e[256]) {
  if (!r || !saved || saved->has_model > 1 || (saved->has_model && !saved->model))
    return fail(e, "invalid retained light snapshot");
  BkSceneLighting *match = NULL;
  if (saved->has_model)
    for (uint32_t i = 0; i < r->source_count; ++i)
      if (saved->model == bk_scene_lighting_model(r->sources[i])) {
        if (match)
          return fail(e, "ambiguous destination light model");
        match = r->sources[i];
      }
  if (match && !bk_scene_lighting_restore(match, &saved->device))
    return fail(e, "retained light model changed its layout");
  r->ambient = saved->ambient;
  return 1;
}
BkSceneLightRegistry *bk_scene_light_registry_create(const BkLightSource *s,
                                                     uint32_t count,
                                                     char e[256]) {
  if (!s || !count || count > BK_PASS_LIGHTS) {
    fail(e, "invalid source list");
    return NULL;
  }
  BkSceneLightRegistry *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(e, "allocation failed");
    return NULL;
  }
  r->source_count = count;
  uint8_t ambient[BK_PASS_LIGHTS] = {0};
  for (uint32_t i = 0; i < count; ++i) {
    r->sources[i] = bk_scene_lighting_create(s[i].model, s[i].world.matrices,
                                             s[i].world.floats, e);
    if (!r->sources[i])
      goto bad;
    BkLightingPassInput input = {0};
    bk_scene_lighting_input(r->sources[i], &input);
    if (input.light_count > BK_PASS_LIGHTS - r->light_count) {
      fail(e, "native registry capacity exceeded");
      goto bad;
    }
    const BkModelEnvironment *env =
        bk_scene_lighting_environment(r->sources[i]);
    for (uint32_t j = 0; j < input.light_count; ++j) {
      r->lights[r->light_count] = input.lights[j];
      r->lights[r->light_count].ambient_rank = 0;
      ambient[r->light_count] =
          strlen(env->lights[j].name) > 6 && env->lights[j].name[6] == 'A';
      ++r->light_count;
    }
  }
  if (!bk_light_ambient_initialize(r->lights, ambient, r->light_count,
                                   &r->ambient)) {
    fail(e, "invalid combined ambient registry");
    goto bad;
  }
  return r;
bad:
  bk_scene_light_registry_destroy(r);
  return NULL;
}
int bk_scene_light_registry_input(const BkSceneLightRegistry *r,
                                  BkLightingPassInput *out) {
  if (!r || !out)
    return 0;
  out->light_count = r->light_count;
  memcpy(out->lights, r->lights, sizeof(r->lights));
  return 1;
}
int bk_scene_light_registry_command(BkSceneLightRegistry *r,
                                    const BkLightingCommand *command) {
  if (!r || !command)
    return 0;
  if (command->kind == BK_PASS_AMBIENT) {
    r->ambient = command->value;
    return 1;
  }
  if (command->kind != BK_PASS_LIGHT_ENABLE ||
      command->target >= r->light_count || command->value > 1)
    return 0;
  BkLightingCommand local = *command;
  for (uint32_t i = 0; i < r->source_count; ++i) {
    uint32_t count = bk_scene_lighting_environment(r->sources[i])->light_count;
    if (local.target < count)
      return bk_scene_lighting_command(r->sources[i], &local);
    local.target -= count;
  }
  return 0;
}
int bk_scene_light_registry_values(const BkSceneLightRegistry *r,
                                   const BkLightWorld *world, uint32_t count,
                                   BkLighting *out, char e[256]) {
  if (!r || !world || !out || count != r->source_count)
    return fail(e, "invalid published sources");
  BkLighting result = {0};
  for (unsigned j = 0; j < 3; ++j)
    result.ambient[j] = ((r->ambient >> (16 - j * 8)) & 255) / 255.f;
  for (uint32_t i = 0; i < count; ++i) {
    BkLighting source;
    if (!bk_scene_lighting_values(r->sources[i], world[i].matrices,
                                  world[i].floats, &source, e))
      return 0;
    if (result.point_count + result.spot_count + source.point_count +
            source.spot_count >
        BK_MAX_POINT_LIGHTS)
      return fail(e, "enabled light capacity exceeded");
    memcpy(result.points + result.point_count, source.points,
           source.point_count * sizeof(*source.points));
    memcpy(result.spots + result.spot_count, source.spots,
           source.spot_count * sizeof(*source.spots));
    result.point_count += source.point_count;
    result.spot_count += source.spot_count;
  }
  if (!bk_lighting_validate(&result, e))
    return 0;
  *out = result;
  return 1;
}
