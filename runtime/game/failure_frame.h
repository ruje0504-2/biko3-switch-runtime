#ifndef BK_GAME_FAILURE_FRAME_H
#define BK_GAME_FAILURE_FRAME_H
#include "world/failure_camera.h"
typedef struct {
  uint8_t *visible, *player_hidden, *npc_hidden;
  int32_t *player_action, *npc_action;
  const int32_t *player_idle, *npc_idle, *npc_rear_action;
  /* Only outcome5 needs the selected prop's live action. */
  int32_t *prop_action;
} BkFailureFrameBindings;
typedef enum {
  BK_FAILURE_FRAME_PLAYER,
  BK_FAILURE_FRAME_NPC,
  BK_FAILURE_FRAME_BACKGROUND,
  BK_FAILURE_FRAME_PROPS
} BkFailureFrameStage;
typedef struct {
  void *context;
  int (*camera)(void *, BkFailureCameraKind, int *arrived, char error[256]);
  /*46435e restarts a loaded buffer at0: prop=0 NPC speech, prop=1 selected
   * prop. Must retain/consume the real resource, not report a fake play. */
  int (*restart)(void *, int prop, char error[256]);
  int (*present)(void *, BkFailureFrameStage, char error[256]);
} BkFailureFrameOps;
/*51b244: camera, first-arrival voice/action, actor visibility/action, then
 * player/NPC/background/prop presentation. No AI/movement/collision/items.
 * Unknown outcomes still execute the four presentation stages. Callback
 * failure stops at that original prefix; basic invalid input is atomic. */
int bk_failure_frame_step(uint8_t outcome, const BkFailureFrameBindings *,
                          const BkFailureFrameOps *, char error[256]);
#endif
