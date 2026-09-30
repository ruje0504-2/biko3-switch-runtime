#ifndef BK_SCENE_LIGHTING_ASSETS_H
#define BK_SCENE_LIGHTING_ASSETS_H
#include "game/lighting_pass.h"
#include "model/environment.h"
typedef struct BkSceneLighting BkSceneLighting;
/*Original4a435a key, including NULL (all lights group2). The default
 * constructor below uses BK3_L. Parent-name comparison is case-sensitive. */
BkSceneLighting *bk_scene_lighting_create_key(const BkModel *, const float *,
                                              size_t, const char *, char[256]);
/* Owns environment, borrows model. Registry order follows its LIGH records.
 * Initial ambient follows4a4438; all drawable pass modes explicitly set the
 * non-ambient lights before drawing. No GPU object or implicit publication. */
BkSceneLighting *bk_scene_lighting_create(const BkModel *, const float *world,
                                          size_t floats, char error[256]);
void bk_scene_lighting_destroy(BkSceneLighting *);
/*A retained model keeps its device enable bits when only the surrounding
 * registration table is rebuilt. Never match a newly loaded model by name.*/
const BkModel *bk_scene_lighting_model(const BkSceneLighting *);
int bk_scene_lighting_inherit(BkSceneLighting *, const BkSceneLighting *);
typedef struct {
  uint32_t light_count, ambient, enabled;
} BkSceneLightDevice;
/* Value-only device state. The registry checks model identity before restore;
 * this layer also checks the immutable light layout and enabled-bit range.
 * No environment, model, world cache or GPU resource is retained here. */
int bk_scene_lighting_save(const BkSceneLighting *, BkSceneLightDevice *);
int bk_scene_lighting_restore(BkSceneLighting *, const BkSceneLightDevice *);
const BkModelEnvironment *
bk_scene_lighting_environment(const BkSceneLighting *);
/* Replaces only the light array/count; caller owns root tokens/pass settings.
 */
int bk_scene_lighting_input(const BkSceneLighting *, BkLightingPassInput *);
/* Accept only light/ambient commands. Object/flush/shadow remain the caller's
 * ordered draw work. Each GPU pass needs its own light descriptor snapshot. */
int bk_scene_lighting_command(BkSceneLighting *, const BkLightingCommand *);
/* Native light attachment reads each light frame's current cached world
 * translation and spotlight direction. Caller supplies that model's published
 * world array. Failure preserves the output. Fog device state is separately
 * retained. */
int bk_scene_lighting_values(const BkSceneLighting *, const float *world,
                             size_t floats, BkLighting *, char error[256]);
#endif
