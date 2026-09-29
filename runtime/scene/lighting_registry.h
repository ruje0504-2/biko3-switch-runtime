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
/*Original4a4140 clears the registration arrays, not the retained device
 * ambient or enabled light state.4a435a rebuilds group classification in
 * the same source order for BK3_L, with all ambient ranks initially0.
 * 4a4438 then selects ambient and marks its ranks. No world publication,
 * asset reload, or mutation of already captured GPU light descriptors. */
int bk_scene_light_registry_reset(BkSceneLightRegistry *);
int bk_scene_light_registry_select_bk3_l(BkSceneLightRegistry *);
int bk_scene_light_registry_ambient(BkSceneLightRegistry *);
/*Keep device ambient and enable bits for identical retained model owners.
 * Newly loaded models retain their own initial state. Registration order,
 * classification/ranks and captured GPU descriptors are not copied.*/
int bk_scene_light_registry_inherit(BkSceneLightRegistry *,
                                     const BkSceneLightRegistry *, char error[256]);
typedef struct {
  uint32_t ambient, has_model;
  const BkModel *model;
  BkSceneLightDevice device;
} BkRetainedLightState;
/*A pure-UI interval releases the old 3D registry. Preserve global ambient
 * and only the explicitly retained background model's device state. The
 * caller keeps that model alive until restore/discard. A model without LIGH
 * still preserves global ambient; no obsolete actor pointer is captured.
 * Restore matches model identity, never a resource name, and leaves newly
 * loaded models at their own initial state. Registration/ranks are separate.*/
int bk_scene_light_registry_save(const BkSceneLightRegistry *,
                                 const BkModel *retained_model,
                                 BkRetainedLightState *, char error[256]);
int bk_scene_light_registry_restore(BkSceneLightRegistry *,
                                    const BkRetainedLightState *, char error[256]);
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
