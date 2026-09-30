#ifndef BK_GAME_LIGHTING_PASS_H
#define BK_GAME_LIGHTING_PASS_H
#include <stdint.h>
#define BK_PASS_LIGHTS 16
#define BK_PASS_COMMANDS 128
typedef struct {
  float diffuse[3];
  int32_t group, ambient_rank;
} BkPassLight;
typedef struct {
  BkPassLight lights[BK_PASS_LIGHTS];
  uint32_t light_count;
  /* Opaque caller tokens,0 means no object.0..3/4..19/20..51 preserve the
   * original four-,sixteen-,thirty-two-root arrays. No native pointers. */
  uint32_t objects[52], scene_root;
  int32_t mode, shadow_mode;
} BkLightingPassInput;
typedef enum {
  BK_PASS_AMBIENT,
  BK_PASS_LIGHT_ENABLE,
  BK_PASS_OBJECT,
  BK_PASS_FLUSH,
  BK_PASS_PROJECTED_SHADOW
} BkLightingCommandKind;
typedef struct {
  uint32_t kind, target, value;
} BkLightingCommand;
typedef struct {
  uint32_t count;
  BkLightingCommand commands[BK_PASS_COMMANDS];
} BkLightingPass;
/*4a4159: scan case-sensitively for the first B in a light node's parent name,
 * then compare the suffix case-sensitively (lstrcmpA) to the loader's key.
 * A NULL key selects group2 for every nonnull parent, including empty names.
 * Registry names have at most64 bytes; no host locale or filesystem names. */
int bk_light_group(const char *parent_name, const char *key, int32_t *out);
/*4a4438: all ambient-named lights receive rank1; first group1 ambient wins,
 * otherwise first ambient. Caller supplies ambient_name(name[6]=='A').
 * Outputs packed ARGB and ranks atomically. At most16 native registry slots. */
int bk_light_ambient_initialize(BkPassLight *, const uint8_t *ambient_name,
                                uint32_t count, uint32_t *argb);
/*4a4701 ordered commands, no renderer/world side effects. Ambient always
 * starts black then uses first rank1,otherwise rank2. Modes0/1/2/10 preserve
 * per-group toggles and object/flush order. Other modes emit ambient only.
 * Native mode0 projected-shadow callback is explicit, not implemented here. */
int bk_lighting_pass(const BkLightingPassInput *, BkLightingPass *);
#endif
