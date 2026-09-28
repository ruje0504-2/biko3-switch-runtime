#include "game/player_events.h"
#include "game/player_event_tables.inc"
#include "world/timeline_event.h"
#include <math.h>
#include <string.h>
static int pair(const BkPlayerEventInput *in, unsigned a, unsigned b) {
  return in->action == in->actions[a] || in->action == in->actions[b];
}
static int special(const BkPlayerEventInput *in) {
  return pair(in, 11, 13) || pair(in, 12, 14) || pair(in, 16, 17);
}
static void command(BkPlayerEvents *out, const char *file, int loop) {
  out->commands[out->count++] = (BkPlayerSoundCommand){file, loop};
}
static const char *surface_sound(const BkPlayerEventInput *in, int8_t *noise) {
  static const char *const sounds[3] = {"se107.wav", "se106.wav", "se144.wav"};
  *noise = 1;
  for (unsigned i = 0; i < 3; ++i)
    if (!strcmp(in->surface, surfaces[i][in->group * 9 + in->area]))
      return sounds[i];
  if (in->group == 0 && in->area == 7 &&
      !strcmp(in->surface, "Mesh_KusattaALL@M01_03.X"))
    return "se148.wav";
  *noise = 0;
  return in->area >= 7 ? "se105.wav" : "se104.wav";
}
int bk_player_events(BkPlayerEventState *state, uint8_t *steps,
                     size_t step_count, uint8_t *loops, size_t loop_count,
                     const BkPlayerEventInput *in, BkPlayerEvents *out) {
  if (!state || !steps || step_count < BK_PLAYER_EVENT_LATCH_COUNT || !loops ||
      loop_count < BK_PLAYER_LOOP_LATCH_COUNT || !in || !out || in->group < 0 ||
      in->group >= 5 || in->area < 0 || in->area >= 9 ||
      !isfinite(in->source) || !in->surface)
    return 0;
  BkPlayerEventState next = *state;
  BkPlayerEvents result = {0};
  uint8_t step_next[BK_PLAYER_EVENT_LATCH_COUNT],
      loop_next[BK_PLAYER_LOOP_LATCH_COUNT];
  memcpy(step_next, steps, sizeof(step_next));
  memcpy(loop_next, loops, sizeof(loop_next));
  int32_t first = 0, second = 0, loop_tick = 0;
  int8_t direction = 0;
  if (pair(in, 1, 2)) {
    first = 107;
    second = 123;
  } else if (pair(in, 3, 4)) {
    first = 246;
    second = 258;
  } else if (pair(in, 5, 6)) {
    first = 200;
    second = 185;
    direction = 1;
  } else if (pair(in, 11, 13)) {
    first = 369;
    second = 403;
    loop_tick = 408;
  } else if (pair(in, 12, 14)) {
    first = 403;
    second = 369;
    direction = 1;
  } else if (in->action == in->actions[16]) {
    first = 443;
    second = 474;
  } else if (in->action == in->actions[17]) {
    first = 543;
    second = 515;
    direction = 1;
  } else
    result.default_direction = 1;
  if (in->action == in->actions[8]) {
    result.default_direction = 0; /* No native direction read on this branch. */
    if (next.loop_latched == 0) {
      next.loop_latched = 1;
      command(&result, "se119.wav", 1);
    }
    goto done;
  }
  if (pair(in, 11, 13) || in->action == in->actions[16]) {
    if (next.loop_latched == 0 && !(in->voice_present && in->voice_playing)) {
      int fire;
      if (!bk_timeline_event(loop_next + loop_tick, in->source, loop_tick,
                             direction, &fire))
        return 0;
      if (fire) {
        next.loop_latched = 1;
        command(&result, "se119.wav", 1);
      }
    }
  } else if (next.loop_latched == 1) {
    next.loop_latched = 0;
    if (in->voice_present)
      command(&result, NULL, 0);
  }
  int fire;
  if (!bk_timeline_event(step_next + second, in->source, second, direction,
                         &fire))
    return 0;
  if (fire) {
    command(&result, special(in) ? "se110.wav" : surface_sound(in, &next.noise),
            0);
  } else {
    if (!bk_timeline_event(step_next + first, in->source, first, direction,
                           &fire))
      return 0;
    if (fire)
      command(&result,
              special(in) ? "se109.wav" : surface_sound(in, &next.noise), 0);
  }
done:
  memcpy(steps, step_next, sizeof(step_next));
  memcpy(loops, loop_next, sizeof(loop_next));
  *state = next;
  *out = result;
  return 1;
}
