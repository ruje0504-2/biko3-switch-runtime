#ifndef BK_MODEL_ENVIRONMENT_H
#define BK_MODEL_ENVIRONMENT_H
#include "core/fog.h"
#include "core/lighting.h"
#include "model/model.h"
typedef struct {
  char name[65];
  uint32_t id, type, frame_index;
  float diffuse[4], specular[4], ambient[4], position[3], direction[3];
  float range, falloff, attenuation[3], theta, phi;
} BkModelLight;
typedef BkFog BkModelFog;
typedef struct {
  BkModelLight *lights;
  uint32_t light_count, ambient_count;
  BkLighting lighting;
  BkModelFog fog;
} BkModelEnvironment;
/* Borrows model/world only during construction; owns all returned data.
 * Current adapter supports one ambient record, up to eight point/spot lights,
 * material specular and decoded fog. Per-pass light selection is separate.
 * Unsupported light types/counts fail explicitly.
 */
BkModelEnvironment *bk_model_environment_create(const BkModel *model,
                                                const float *world,
                                                char error[256]);
void bk_model_environment_destroy(BkModelEnvironment *environment);
/*4260b6 frame binding: position is frame origin. Spot direction comes from
 * (-world[2],-world[6],world[10]) with original522922 normalization.
 * An unrepresentable/degenerate direction fails without changing light. */
int bk_model_light_frame(BkModelLight *, const float world[16],
                         char error[256]);
/* Append a supported nonambient light to a temporary CPU snapshot. */
int bk_model_lighting_append(BkLighting *, const BkModelLight *,
                             char error[256]);
#endif
