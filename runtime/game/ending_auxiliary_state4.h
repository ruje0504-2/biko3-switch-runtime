#ifndef BK_GAME_ENDING_AUXILIARY_STATE4_H
#define BK_GAME_ENDING_AUXILIARY_STATE4_H

#include "game/ending_auxiliary.h"
#include "game/ending_control.h"
#include "game/ending_opening.h"
#include "world/menu_camera.h"

/* Portable owners for 47DC79's actual state4 branch. The byte substate is
 * 6C7F50; it is retained by the scene because it survives draw/reload. */
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  BkMenuCamera *camera;
  uint8_t *substate;
  float *fov;
  const int8_t *previous_flow;
} BkEndingAuxiliaryState4Bindings;

typedef struct {
  void *context;
  int (*present)(void *, unsigned owner, int *present, char[256]);
  int (*status)(void *, unsigned owner, int *playing, char[256]);
  int (*prepare_actor)(void *, char[256]); /*721b28 XAN slot2/3 source reset*/
  int (*expression)(void *, int32_t, int32_t, unsigned, char[256]); /*4DFB96*/
  int (*target)(void *, float position[3], char[256]); /*721F08+F0*/
  int (*camera)(void *, BkEndingOpeningCamera, int32_t choice,
                const uint32_t offset[3], uint32_t extra, uint32_t *result,
                char[256]); /*4E0B7E/4E0ECB*/
  int (*effect)(void *, unsigned, unsigned, unsigned, char[256]); /*481E0A*/
} BkEndingAuxiliaryState4Ops;

/* Execute the recovered state4 jump-table logic of 47DC79. Each call captures
 * the substate once. The actor source reset is an explicit service owned by
 * the primary BkActorPose. Media waits are no-ops, while camera/effect
 * services execute in original order and retain their side effects on failure.
 * The routine does not advance or publish actors. */
int bk_ending_auxiliary_state4_step(
    const BkEndingAuxiliaryState4Bindings *, float seconds,
    const BkEndingAuxiliaryState4Ops *, char error[256]);

#endif
