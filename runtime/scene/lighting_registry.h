#ifndef BK_SCENE_LIGHTING_REGISTRY_H
#define BK_SCENE_LIGHTING_REGISTRY_H
#include "scene/lighting_assets.h"
typedef struct BkSceneLightRegistry BkSceneLightRegistry;
typedef struct {
  const float *matrices;
  size_t floats;
} BkLightWorld;
typedef struct {
  const BkModel *model;
  BkLightWorld world;
} BkLightSource;
/* Ordered registry across separately owned models. Each source must contain
 * LIGH; caller lists only light-bearing models in original load order.
 * Own decoded environments, borrow models. World pointers are used only by
 * this call; each snapshot must supply current published caches again.
 * Global ambient selection runs once across all sources. Fog remains the
 * caller's retained device state, never merged from the environments. */
BkSceneLightRegistry *bk_scene_light_registry_create(const BkLightSource *,
                                                     uint32_t count,
                                                     char error[256]);
void bk_scene_light_registry_destroy(BkSceneLightRegistry *);
/* Replace only the light array/count, retaining the caller's root tokens. */
int bk_scene_light_registry_input(const BkSceneLightRegistry *,
                                  BkLightingPassInput *);
int bk_scene_light_registry_command(BkSceneLightRegistry *,
                                    const BkLightingCommand *);
/* Sources remain in construction order. Up to eight combined point/spot
 * lights per snapshot, selected from at most16 registered lights. Invalid
 * caches/counts preserve output; no forest publication or GPU work here. */
int bk_scene_light_registry_values(const BkSceneLightRegistry *,
                                   const BkLightWorld *, uint32_t count,
                                   BkLighting *, char error[256]);
#endif
