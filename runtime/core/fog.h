#ifndef BK_CORE_FOG_H
#define BK_CORE_FOG_H
#include <stdint.h>
typedef struct {
  uint32_t enabled, mode, table, range_based, color;
  float start, end, density;
} BkFog;
/* D3D modes 0 none,1 exp,2 exp2,3 linear. Table selects pixel eye-depth
 * fog; otherwise evaluate at vertices. Range applies to vertex fog only. */
int bk_fog_validate(const BkFog *, char error[256]);
typedef struct {
  uint32_t enabled, vertex_mode, table_mode, range_based, color;
  float start, end, density;
} BkFogState;
/* Original42d295/FOG loader state: setting vertex mode does not clear the
 * table mode installed by enable. Pixel mode takes precedence. Range=false
 * leaves its prior device state intact. Caps are booleans, not native bits.
 * Loading disabled records does nothing; caller explicitly resets on entry. */
int bk_fog_enable(BkFogState *, int enabled, int range, int table_supported,
                  int range_supported);
int bk_fog_load(BkFogState *, const BkFog *, int table_supported,
                int range_supported, char error[256]);
int bk_fog_resolve(const BkFogState *, BkFog *, char error[256]);
#endif
