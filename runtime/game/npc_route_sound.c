#include "game/npc_route_sound.h"
int bk_npc_route_sound(BkNpcRouteSound *out, int32_t group, int32_t area,
                       uint32_t cursor, const float source[3],
                       const float listener[3], float listener_yaw,
                       int32_t effect_volume) {
  if (!out)
    return 0;
  BkNpcRouteSound next = {0};
  if ((group == 0 && area == 1 && cursor == 59) ||
      (group == 4 && area == 3 && cursor == 27))
    next.file = "se152.wav";
  else if (group == 1 && area == 1 && cursor == 83)
    next.file = "se151.wav";
  else if (group == 1 && area == 5 && cursor == 75)
    next.file = "se304.wav";
  else if (group == 2 &&
           ((area == 6 && cursor == 155) || (area == 8 && cursor == 171)))
    next.file = "se155.wav";
  if (next.file && (effect_volume < -10000 || effect_volume > 0 ||
                    !bk_spatial_audio(&next.gain, source, listener,
                                      listener_yaw, effect_volume, 6)))
    return 0;
  *out = next;
  return 1;
}
