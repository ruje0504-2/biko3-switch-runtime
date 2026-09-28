#include "game/npc_footsteps.h"
#include "world/spatial_audio.h"
#include "world/timeline_event.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  uint8_t guarded[428] = {0}, saved[428];
  guarded[0] = 0x7a;
  guarded[427] = 0x3e;
  BkNpcFootstepInput in = {.group = 0,
                           .area = 8,
                           .action = 1,
                           .source_tick = 500,
                           .surface_name = "",
                           .actions = {{1, 2}, {4, 5}, {10, 12}, {7, 9}}};
  BkNpcFootsteps out;
  assert(bk_npc_footsteps(guarded + 1, 426, &in, &out));
  assert(out.count == 6 && out.ticks[0] == 105 && out.ticks[5] == 40);
  assert(!strcmp(out.sound_file, "se117.wav"));
  assert(guarded[0] == 0x7a && guarded[427] == 0x3e);
  assert(bk_npc_footsteps(guarded + 1, 426, &in, &out) && !out.count);
  BkNpcFootsteps held = out;
  memcpy(saved, guarded, sizeof(saved));
  in.source_tick = NAN;
  assert(!bk_npc_footsteps(guarded + 1, 426, &in, &out));
  assert(!memcmp(&held, &out, sizeof(out)) &&
         !memcmp(saved, guarded, sizeof(saved)));
  in.source_tick = 0;
  assert(!bk_npc_footsteps(guarded + 1, 425, &in, &out));
  assert(!memcmp(saved, guarded, sizeof(saved)));
  assert(bk_npc_footsteps(guarded + 1, 426, &in, &out) && !out.count);
  in.group = 3;
  in.source_tick = 500;
  assert(bk_npc_footsteps(guarded + 1, 426, &in, &out) && out.count == 1 &&
         out.ticks[0] == 106);
  assert(bk_npc_footsteps(guarded + 1, 426, &in, &out) && out.count == 1 &&
         out.ticks[0] == 98);
  uint8_t latch = 255;
  int fired = 9;
  assert(bk_timeline_event(&latch, 100, 50, 0, &fired) && !fired &&
         latch == 255);
  assert(bk_timeline_event(&latch, 49, 50, 0, &fired) && !fired && !latch);
  assert(bk_timeline_event(&latch, 50, 50, 1, &fired) && fired && latch == 1);
  assert(!bk_timeline_event(&latch, NAN, 50, 0, &fired) && fired && latch == 1);
  const float source[3] = {0, 1, 10}, listener[3] = {0};
  BkSpatialAudio audio = {123, 456};
  assert(bk_spatial_audio(&audio, source, listener, 0, -100, 6));
  assert(audio.volume == -160 && audio.pan == 0);
  assert(bk_spatial_audio(&audio, source, listener, 0, -10000, 6) &&
         audio.volume == -6000);
  BkSpatialAudio before = audio;
  assert(!bk_spatial_audio(&audio, source, listener, NAN, 0, 6));
  assert(!memcmp(&audio, &before, sizeof(audio)));
  puts("PASS shared event latches, descending/duplicate ticks, early exit, "
       "guarded rejection and audio units");
  return 0;
}
