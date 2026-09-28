#ifndef BK_GAME_FRAME_DISPATCH_H
#define BK_GAME_FRAME_DISPATCH_H
#include <stdint.h>
typedef enum {
  BK_FRAME_COLLISION_BEGIN,
  BK_FRAME_BACKGROUND,
  BK_FRAME_PLAYER_IDLE,
  BK_FRAME_PLAYER_CONTROL,
  BK_FRAME_PLAYER_VIEW,
  BK_FRAME_PLAYER_PRESENTATION,
  BK_FRAME_NPC_SPATIAL,
  BK_FRAME_NPC_PRESENTATION,
  BK_FRAME_PROP_SPATIAL,
  BK_FRAME_PROP_PRESENTATION,
  BK_FRAME_ITEMS,
  BK_FRAME_PROP_INTERACTION,
  BK_FRAME_DETECTION,
  BK_FRAME_AREA,
  BK_FRAME_PICKUP,
  BK_FRAME_CAMERA,
  BK_FRAME_COLLISION_END
} BkGameFrameEvent;
typedef int (*BkGameFrameConsumer)(void *, BkGameFrameEvent, char error[256]);
/*51a682 +4ec78d. Append old-world collision, copy phase to NPC mode and latch
 * dispatch, then run exact stage order.4ef141 only counts an empty loop.
 * The enclosing HUD owner must apply4cc320 reserve=1 after latched phase0/2;
 * that UI field is outside this dispatcher's represented state.
 * Callback must perform each actual service, not silently ignore it. Phase
 * can change inside a service but never reroutes the current frame. This
 * does not perform51a190 UI/phase transitions or draw/publication.
 * Failed stage aborts further updates, always removes an appended collision
 * suffix, preserves the first error. Earlier effects are not rolled back. */
int bk_game_frame_dispatch(uint8_t *phase, int8_t *npc_mode,
                           BkGameFrameConsumer, void *, char error[256]);
#endif
