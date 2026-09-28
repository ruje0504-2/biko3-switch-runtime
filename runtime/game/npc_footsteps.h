#ifndef BK_GAME_NPC_FOOTSTEPS_H
#define BK_GAME_NPC_FOOTSTEPS_H
#include <stddef.h>
#include <stdint.h>
#define BK_NPC_FOOTSTEP_LATCH_COUNT 426
/* Actor+18/1c,20/24,68/6c,70/74. Supply live bindings, including aliases. */
typedef struct {
  int32_t walk[2], run[2], special[2], extra[2];
} BkNpcFootstepActions;
typedef struct {
  unsigned group, area;
  int32_t action;
  float source_tick;
  const char *surface_name;
  BkNpcFootstepActions actions;
} BkNpcFootstepInput;
typedef struct {
  uint32_t count;
  int32_t ticks[8];
  const char *sound_file;
} BkNpcFootsteps;
/* 4fd796: descending authored tick checks, shared tick latches (no automatic
 * clip-change clear), priority surface-name equality, bk3_02 sound filename.
 * Group3 exits after its first emitted event; the others can emit multiple.
 * Caller consumes reload->spatial volume/pan->play in this exact event order.
 * Spatial audio uses effect master volume and attenuation6. No audio backend
 * or hard-hidden gate is invented. Failure leaves latches/output intact. */
int bk_npc_footsteps(uint8_t *latches, size_t latch_count,
                     const BkNpcFootstepInput *input, BkNpcFootsteps *out);
#endif
