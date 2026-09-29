#ifndef BK_GAME_ENDING_NORMAL_ACTION_H
#define BK_GAME_ENDING_NORMAL_ACTION_H
#include "game/ending_normal_control.h"
/* Independent retained words709db0/709dac. Neither is the parent's
 * selected_clip57563c or the separate manual-key clip709db4. */
typedef struct {
  int32_t clip, increment;
} BkEndingNormalActionState;
typedef struct {
  BkEndingNormalControlBindings normal;
  int32_t *contact_index; /*721ed4*/
  int8_t *side;          /*719b4c*/
  int32_t (*targets)[2]; /*721f90,39 points written by actual projection*/
  uint32_t *direct_reference, *direct_node; /*portable IDs,719b28/2c*/
  const int32_t *flip, *effect_volume; /*714fa8/be9a10*/
} BkEndingNormalActionBindings;
typedef struct {
  float source, end;
} BkEndingNormalActionTiming;
typedef struct {
  /* Uses the same actor0/1 and buffer0/1 services as the parent. The
   * action additionally uses buffer5 (7228d4, existing effect se203). */
  BkEndingNormalControlOps normal;
  int (*project)(void *, unsigned target, int32_t point[2], char[256]);
  int (*timing)(void *, int32_t clip, BkEndingNormalActionTiming *, char[256]);
  int (*source)(void *, int32_t clip, float source, char[256]);
  int (*instant)(void *, unsigned actor, int32_t clip, char[256]); /*401d24*/
  int (*find)(void *, const char *, uint32_t *node, char[256]); /*primary425904*/
  int (*warp)(void *, float x, float y, char[256]);
  /*kind0=49b28f(binding0),1=49b900(last index1); .09, reset0. The
   * original result is intentionally ignored by the caller's final write. */
  int (*recoil)(void *, unsigned kind, float degrees, int32_t flip,
                uint32_t milliseconds, int32_t reset, int *done, char[256]);
} BkEndingNormalActionOps;
/* Actual4df32c; advances the same shared CRT RNG even when its result is
 * independent of the sample. Invalid/nonfinite inputs retain RNG/output. */
int bk_ending_normal_increment(uint32_t *random, float progress,
                                int32_t *increment, char error[256]);
/* Complete4dd280. Two unconditional shared RNG draws precede old-world
 * projection, including ready2/unknown states. Distinct narrow/wide pointer
 * bounds, source==end vs source>=end vs source>=end-1, configured/instant
 * requests, voice-status gates, direct node binding and recoil are retained.
 * The11-word input is copied by value; pointer clamping never edits caller
 * input. Reads/writes are against real services and shared owners, with no
 * automatic pose advance, world publication, fabricated target or audio.
 * Required service failure preserves its prefix. result is original EAX,
 * separate from portable success. Zero/long elapsed time is supported while
 * native recoil's signed32 conversion must remain representable. */
int bk_ending_normal_action_step(BkEndingNormalActionState *,
                                 const BkEndingNormalActionBindings *,
                                 int32_t requested_clip,
                                 const BkEndingFrameInput *, float seconds,
                                 const BkEndingNormalActionOps *,
                                 uint32_t *result, char error[256]);
typedef struct {
  BkEndingNormalControlOps normal;
  int (*write)(void *, int32_t clip, BkEndingClipWrite field, int32_t value,
                char error[256]);
} BkEndingNormalManualOps;
/*4e252b keyboard/manual action selection.709db4 is a separate process word,
 * initially0. Required auxiliary root assignment precedes even unknown modes.
 * Mutable primary chain/next writes and configured requests never advance
 * time. Missing auxiliary/services fail; no substitute clips are generated. */
int bk_ending_normal_manual_step(int32_t *clip,
                                  BkEndingNormalControlState *,
                                  const BkEndingNormalControlBindings *,
                                  const BkEndingNormalManualOps *,
                                  char error[256]);
#endif
