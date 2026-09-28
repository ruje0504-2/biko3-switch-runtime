#include "game/background.h"
#include "core/random.h"
#include "world/proximity.h"
#include <limits.h>
#include <math.h>
static void ensure_playing(const BkAmbientInput *in, BkAmbientCommand *out) {
  if (!in->playing) {
    out->action = 1;
    out->loop = in->loop;
  }
}
static void music_step(BkBackgroundState *s, const BkBackgroundInput *in,
                       BkBackgroundCommands *c) {
  if (s->music_mode == 0 || s->music_mode == 1) {
    int32_t delta =
        (int32_t)(float)((double)in->seconds * (s->music_mode ? 600.f : 100.f));
    if (delta <= 1)
      delta = 1;
    if (s->music_mode == 1) {
      s->music_volume += delta;
      if (s->music_volume >= in->music_master)
        s->music_volume = in->music_master;
    } else {
      s->music_volume -= delta;
      if (s->music_volume <= -6000)
        s->music_volume = -6000;
    }
    c->music_update = 1;
  }
  c->music_volume = s->music_volume;
}
int bk_background_pause_step(BkBackgroundState *state,
                             const BkBackgroundInput *in,
                             BkBackgroundCommands *out) {
  if (!state || !in || !out || !isfinite(in->seconds) || in->seconds < 0 ||
      (double)in->seconds * 600 > INT32_MAX - 10000 ||
      in->music_master < -10000 || in->music_master > 0 ||
      in->effect_master < -10000 || in->effect_master > 0 ||
      state->music_volume < -10000 || state->music_volume > 0 ||
      !isfinite(in->player_yaw))
    return 0;
  for (unsigned k = 0; k < 3; ++k)
    if (!isfinite(in->player[k]))
      return 0;
  BkBackgroundState s = *state;
  BkBackgroundCommands c = {.door_request = -1, .seconds = in->seconds};
  music_step(&s, in, &c);
  for (unsigned i = 0; i < 8; ++i) {
    const BkAmbientInput *a = &in->ambient[i];
    if (!a->present || a->trigger != 0)
      continue;
    c.ambient[i].spatial = 1;
    if (!bk_spatial_audio(&c.ambient[i].gain, a->position, in->player,
                          in->player_yaw, in->effect_master, 18.f))
      return 0;
  }
  *state = s;
  *out = c;
  return 1;
}
int bk_background_step(BkBackgroundState *state, uint32_t *random,
                       const BkBackgroundInput *in, BkBackgroundCommands *out) {
  if (!state || !random || !in || !out || !isfinite(in->seconds) ||
      in->seconds < 0 || (double)in->seconds * 600 > INT32_MAX - 10000 ||
      in->music_master < -10000 || in->music_master > 0 ||
      in->effect_master < -10000 || in->effect_master > 0 ||
      state->music_volume < -10000 || state->music_volume > 0 ||
      !isfinite(in->player_yaw))
    return 0;
  for (unsigned k = 0; k < 3; ++k)
    if (!isfinite(in->player[k]) || !isfinite(in->npc[k]))
      return 0;
  BkBackgroundState s = *state;
  uint32_t rng = *random;
  BkBackgroundCommands c = {.door_request = -1, .seconds = in->seconds};
  s.ambient_timer.duration = 5000;
  if (bk_timer_poll(&s.ambient_timer, in->now))
    c.random_choice = bk_random_next(&rng) % 100;
  music_step(&s, in, &c);
  for (unsigned i = 0; i < 8; ++i) {
    const BkAmbientInput *a = &in->ambient[i];
    BkAmbientCommand *command = &c.ambient[i];
    if (!a->present)
      continue;
    if (!isfinite(a->trigger) || (double)a->trigger >= INT32_MAX ||
        (a->loop != 0 && a->loop != 1))
      return 0;
    if (a->trigger == 0)
      command->spatial = 1;
    else if (c.random_choice == a->trigger)
      ensure_playing(a, command);
    else if (a->trigger >= 100) {
      switch ((int32_t)a->trigger) {
      case 100:
        if (in->background_clip == 3)
          ensure_playing(a, command);
        else
          command->action = 2;
        command->spatial = 1;
        break;
      case 101: {
        int open = in->group == 2 && in->area == 8 ? 2 : 1;
        int closed = in->group == 2 && in->area == 8 ? 4 : 3;
        int slot = in->group == 2 && in->area == 8 ? in->door_clip
                                                   : in->background_clip;
        if (slot == open && s.ambient_latch[i] == 0) {
          command->action = 1;
          s.ambient_latch[i] = 1;
        } else if (slot == closed && s.ambient_latch[i] == 1) {
          command->action = 1;
          s.ambient_latch[i] = 0;
        }
        command->spatial = 1;
        break;
      }
      case 102:
        if (in->ambient_gate == 1)
          ensure_playing(a, command);
        else
          command->action = 2;
        command->spatial = 1;
        break;
      default:
        break;
      }
    }
    if (command->spatial &&
        !bk_spatial_audio(&command->gain, a->position, in->player,
                          in->player_yaw, in->effect_master, 18.f))
      return 0;
  }
  if (in->background_present) {
    c.door_advance = c.background_advance = 1;
    if (in->group == 2 && in->area == 8) {
      const float trigger[3] = {84, 0, 0};
      int near_player, near_npc;
      if (!bk_proximity_xz(&near_player, trigger, in->player, 40) ||
          !bk_proximity_xz(&near_npc, trigger, in->npc, 40))
        return 0;
      c.door_request = near_player || near_npc ? 2 : 4;
    }
  }
  if (in->weather_enabled && in->weather_present) {
    c.weather_advance = 1;
    c.weather_seconds = (float)((double)in->seconds * .1);
  }
  *state = s;
  *random = rng;
  *out = c;
  return 1;
}
