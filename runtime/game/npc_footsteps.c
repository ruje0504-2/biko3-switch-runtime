#include "game/npc_footsteps.h"
#include "world/timeline_event.h"
#include <math.h>
#include <string.h>
/* Per-action source ticks in original storage order. Duplicate54 is real. */
static const int16_t ticks[5][4][8] = {{{40, 54, 75, 54, 91, 98, 105, -1},
                                        {120, 131, 150, 170, 178, 184, -1, -1},
                                        {322, 425, -1, -1, -1, -1, -1, -1},
                                        {-1}},
                                       {{48, 62, 82, -1, -1, -1, -1, -1},
                                        {104, 116, 126, 139, 149, 163, 168, -1},
                                        {-1},
                                        {-1}},
                                       {{73, 87, 105, 118, -1, -1, -1, -1},
                                        {145, 152, 161, 172, -1, -1, -1, -1},
                                        {276, -1, -1, -1, -1, -1, -1, -1},
                                        {-1}},
                                       {{40, 51, 70, 98, 106, -1, -1, -1},
                                        {121, 132, 150, 172, 181, -1, -1, -1},
                                        {324, 425, -1, -1, -1, -1, -1, -1},
                                        {-1}},
                                       {{42, 51, 73, 91, 102, 107, -1, -1},
                                        {121, 131, 151, 171, 180, 185, -1, -1},
                                        {319, 422, -1, -1, -1, -1, -1, -1},
                                        {198, -1, -1, -1, -1, -1, -1, -1}}};
static const char *surface(unsigned kind, unsigned group, unsigned area) {
  if (kind == 0) {
    if (group == 0 && area == 5)
      return "Mesh_m2_sound_kareha@M01_01.X";
    if (group == 3 && area == 5)
      return "Mesh_otibaALL@M04_01.X";
    if (group == 4 && area == 5)
      return "Mesh_otibaALL@M05_01.X";
  } else if (kind == 1) {
    if (group == 0 && area == 6)
      return "Mesh_m3_mizu_tamari_0@M01_02.X";
    if (group == 4 && area == 5)
      return "Mesh_mizutamari@M05_01.X";
  } else if (kind == 2) {
    if (group == 0 && area == 4)
      return "Mesh_m1_sound_nuki@M01_00.X";
    if (group == 1 && area == 5)
      return "Mesh_jariALL@M02_01.X";
    if (group == 2 && area == 6)
      return "Mesh_jariALL@M03_02.X";
    if (group == 3 && area == 5)
      return "Mesh_jariALL@M04_01.X";
  } else
    return group == 0 && area == 7 ? "Mesh_KusattaALL@M01_03.X" : NULL;
  return "NULL";
}
static int pair(int32_t action, const int32_t slots[2]) {
  return action == slots[0] || action == slots[1];
}
int bk_npc_footsteps(uint8_t *latches, size_t count,
                     const BkNpcFootstepInput *in, BkNpcFootsteps *out) {
  if (!latches || count < BK_NPC_FOOTSTEP_LATCH_COUNT || !in || !out ||
      in->group >= 5 || in->area >= 9 || !in->surface_name ||
      !isfinite(in->source_tick))
    return 0;
  int category = -1;
  if (pair(in->action, in->actions.walk))
    category = 0;
  else if (pair(in->action, in->actions.run))
    category = 1;
  else if (in->group == 4 && pair(in->action, in->actions.extra))
    category = 3;
  else if (in->group != 1 && pair(in->action, in->actions.special))
    category = 2;
  BkNpcFootsteps next = {0};
  if (category >= 0) {
    for (int i = 7; i >= 0; i--) {
      int32_t tick = ticks[in->group][category][i];
      if (tick < 0)
        continue;
      int fire;
      if (!bk_timeline_event(latches + tick, in->source_tick, tick, 0, &fire))
        return 0;
      if (fire) {
        next.ticks[next.count++] = tick;
        if (in->group == 3)
          break;
      }
    }
    if (next.count) {
      static const char *files[] = {"se107.wav", "se106.wav", "se144.wav",
                                    "se148.wav"};
      next.sound_file = in->group == 2                   ? "se118.wav"
                        : in->area == 7 || in->area == 8 ? "se117.wav"
                                                         : "se116.wav";
      for (unsigned kind = 0; kind < 4; kind++) {
        const char *name = surface(kind, in->group, in->area);
        if (name && !strcmp(name, in->surface_name)) {
          next.sound_file = files[kind];
          break;
        }
      }
    }
  }
  *out = next;
  return 1;
}
