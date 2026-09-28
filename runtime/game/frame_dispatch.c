#include "game/frame_dispatch.h"
#include <stdio.h>
int bk_game_frame_dispatch(uint8_t *phase, int8_t *npc_mode,
                           BkGameFrameConsumer consume, void *context,
                           char error[256]) {
  if (!phase || !npc_mode || !consume) {
    snprintf(error, 256, "game frame: missing state/service");
    return 0;
  }
  if (!consume(context, BK_FRAME_COLLISION_BEGIN, error))
    return 0;
  uint8_t selected = *phase;
  *npc_mode = (int8_t)selected;
  int ok = 1;
#define RUN(event)                                                             \
  do {                                                                         \
    if (!consume(context, event, error)) {                                     \
      ok = 0;                                                                  \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0)
  if (selected == 0 || selected == 1 || selected == 2) {
    RUN(BK_FRAME_BACKGROUND);
    if (selected == 1) {
      RUN(BK_FRAME_PLAYER_CONTROL);
      RUN(BK_FRAME_PLAYER_VIEW);
    } else
      RUN(BK_FRAME_PLAYER_IDLE);
    RUN(BK_FRAME_PLAYER_PRESENTATION);
    RUN(BK_FRAME_NPC_SPATIAL);
    RUN(BK_FRAME_NPC_PRESENTATION);
    RUN(BK_FRAME_PROP_SPATIAL);
    RUN(BK_FRAME_PROP_PRESENTATION);
    RUN(BK_FRAME_ITEMS);
    if (selected == 1) {
      RUN(BK_FRAME_PROP_INTERACTION);
      RUN(BK_FRAME_DETECTION);
      RUN(BK_FRAME_AREA);
      RUN(BK_FRAME_PICKUP);
    } else
      RUN(BK_FRAME_CAMERA);
  }
cleanup:
  if (!ok) {
    char cleanup_error[256];
    consume(context, BK_FRAME_COLLISION_END, cleanup_error);
  } else
    ok = consume(context, BK_FRAME_COLLISION_END, error);
  return ok;
#undef RUN
}
