#ifndef BK_LIGHTING_H
#define BK_LIGHTING_H
#define BK_MAX_POINT_LIGHTS 8
typedef struct {
  float position[3], range;
  float diffuse[3], attenuation0;
  float ambient[3], attenuation1;
  float attenuation2, specular[3];
} BkPointLight;
typedef struct {
  BkPointLight point;
  float direction[3], falloff;
  float theta, phi; /* Full inner/outer cone angles in radians. */
} BkSpotLight;
typedef struct {
  float ambient[3];
  unsigned point_count;
  BkPointLight points[BK_MAX_POINT_LIGHTS];
  unsigned spot_count;
  BkSpotLight spots[BK_MAX_POINT_LIGHTS];
} BkLighting;
/* Up to eight combined point/spot lights, global/per-light ambient/specular.
 * The renderer and model adapter share values, never native GPU types. */
int bk_lighting_validate(const BkLighting *lighting, char error[256]);
#endif
