#ifndef BK_GAME_AREA_BOUNDARY_H
#define BK_GAME_AREA_BOUNDARY_H
#include "game/npc_spatial.h"
#include "game/prop_motion.h"
#include "world/spatial_audio.h"
typedef struct {
  int32_t left, top, right, bottom;
} BkAreaBounds;
/* 4f64ae writes prop+328 for the following presentation frame. Specialized
 * profiles override one rectangle edge; otherwise train kind1 checks front,
 * center and rear and writes the shared background ambient gate bf3be4.
 * Specialized profiles leave that shared byte untouched. Inclusive edges.
 * Invalid geometry/profile/pointers leaves both state and gate intact. */
int bk_area_prop_boundary(BkPropState *, uint8_t *ambient_gate, unsigned group,
                          unsigned area, const BkAreaBounds *);
/* 4f3d34's ordinary NPC rectangle (no special profile/train override).
 * Uses double comparison for original signed32 integer bounds vs float. */
int bk_area_npc_outside(uint8_t *out, const float position[3],
                        const BkAreaBounds *);
/* 4f5e24 exact route-point trigger. played==1 inhibits; other raw bytes
 * retain native behavior. Return boolean, including0 for unknown profiles. */
int bk_area_sound_trigger(int32_t group, int32_t area, int32_t cursor,
                          uint8_t played);
/* 4f607a: NULL for native99, otherwise canonical bk3_02 filename. */
const char *bk_area_sound_file(int32_t group, int32_t area);
typedef struct {
  uint8_t ambient_gate, npc_sound_played, player_sound_played;
} BkAreaBoundaryState;
typedef struct {
  uint32_t group, area, props_present;
  int npc_present;
  int32_t npc_group, effect_volume;
  uint8_t player_sound_suppressed; /* Player+330, exact0 enables. */
  float player_position[3], player_yaw;
  const char *player_wall, *boundary_wall;
  BkAreaBounds bounds;
} BkAreaBoundaryInput;
typedef enum { BK_AREA_SOUND_NPC, BK_AREA_SOUND_PLAYER } BkAreaSoundRecipient;
typedef struct {
  BkAreaSoundRecipient recipient;
  const char *file; /* bk3_02; replace current effect voice, one-shot at0. */
  BkSpatialAudio gain;
} BkAreaSoundCommand;
typedef struct {
  unsigned count;
  BkAreaSoundCommand commands[2]; /* NPC before player, when both trigger. */
} BkAreaBoundaryCommands;
/* Complete4f3d34 rules with explicit sound commands. Props retain slot order;
 * hidden writes become visible on the next presentation stage. Absent NPC
 * retains its hidden byte, which still gates the player wall response. NPC
 * route cue uses npc_group, file selection uses global group. Existing AI,
 * hidden and contact response objects are shared, not reinitialized here.
 * CPU planning is atomic; caller must execute all sounds before committing
 * a successful game frame. This does not load/play resources itself. */
int bk_area_boundary_step(BkAreaBoundaryState *, BkPropState props[16],
                          BkNpcSpatialState *, BkNpcInteractionState *,
                          const BkAreaBoundaryInput *,
                          BkAreaBoundaryCommands *);
#endif
