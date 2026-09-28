#ifndef BK_GAME_PLAYER_EVENTS_H
#define BK_GAME_PLAYER_EVENTS_H
#include <stddef.h>
#include <stdint.h>
#define BK_PLAYER_EVENT_LATCH_COUNT 544
#define BK_PLAYER_LOOP_LATCH_COUNT 409
/* Separate shared banks:4afd50 uses708884;4afe00 uses709084 (also NPC).
 * Preserve them across action/actor changes. State is player+450 and NPC+319.
 */
typedef struct {
  int8_t loop_latched, noise;
} BkPlayerEventState;
typedef struct {
  int32_t group, area, action, actions[21];
  float source;
  const char *surface;
  int voice_present, voice_playing;
} BkPlayerEventInput;
typedef struct {
  /* NULL file means stop current voice. Otherwise release/load/play at the
   * effect master volume, centered, with the supplied loop flag. */
  const char *file;
  int loop;
} BkPlayerSoundCommand;
typedef struct {
  unsigned count;
  BkPlayerSoundCommand commands[2];
  int default_direction;
} BkPlayerEvents;
/* Complete4c155d rules, before animation/request. A higher-priority tick
 * short-circuits the second check, including its latch reset. Unknown actions
 * leave a native stack direction byte uninitialized: use0 explicitly and
 * expose default_direction; no equivalence to arbitrary stack data claimed.
 * Both banks, state and output commit atomically on success. Resource mapping
 * resolves canonical names to bk3_02; original loose/packed path is not logic.
 */
int bk_player_events(BkPlayerEventState *state, uint8_t *steps,
                     size_t step_count, uint8_t *loops, size_t loop_count,
                     const BkPlayerEventInput *input, BkPlayerEvents *out);
#endif
