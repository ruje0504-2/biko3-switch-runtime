#include "game/selection_actor.h"
#include "core/random.h"
#include <math.h>
#include <string.h>
int bk_selection_actor_variant(const uint8_t unlocked[8], uint32_t *random,
                               uint8_t *alternate) {
  if (!unlocked || !random || !alternate)
    return 0;
  unsigned count = 0;
  for (unsigned i = 0; i < 8; ++i)
    count += unlocked[i] != 0;
  *alternate = count == 8 && bk_random_next(random) % 3 != 0;
  return 1;
}
int bk_selection_actor_rules(BkSelectionActorRules *out, unsigned group,
                             uint8_t alternate, float source) {
  if (!out || group >= 5 || alternate > 1 || !isfinite(source))
    return 0;
  BkSelectionActorRules n = {.eye_max = 9,
                             .expression = group == 0 && !alternate ? 0 : 1,
                             .gaze = (int8_t)alternate,
                             .texture = (int8_t)alternate};
  uint32_t pitch = 0x3db2b55f, yaw = group == 1 ? 0x40400000 : 0x3e32b7fe;
  memcpy(&n.pitch_limit, &pitch, 4);
  memcpy(&n.yaw_limit, &yaw, 4);
  if ((group == 0 && source >= 750 && source <= 800) ||
      (group == 2 &&
       ((source >= 240 && source <= 275) || (source >= 290 && source <= 310) ||
        (source >= 717 && source <= 730))))
    n.eye_max = 0;
  *out = n;
  return 1;
}
