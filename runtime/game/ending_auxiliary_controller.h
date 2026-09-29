#ifndef BK_GAME_ENDING_AUXILIARY_CONTROLLER_H
#define BK_GAME_ENDING_AUXILIARY_CONTROLLER_H
#include "game/ending_auxiliary.h"
#include "game/ending_control.h"

/* The first recoverable prefix of native 47DC79 state8. It owns no media,
 * actor or table storage. The scene supplies those services and keeps the
 * process fields in the shared ending state. */
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control; /*721EEC*/
  BkEndingAuxiliaryState *auxiliary;
  int32_t *delay_54f8e0;
  uint8_t *saved_toggle_6bbe4c;
} BkEndingAuxiliaryControllerBindings;

typedef struct {
  void *context;
  int (*status)(void *, unsigned owner, int *playing, char[256]);
  int (*request)(void *, unsigned slot, char[256]); /*4018C8*/
  int (*expression)(void *, int32_t, int32_t, unsigned, char[256]); /*4DFB96*/
  int (*voice)(void *, int32_t cue, unsigned slot, int32_t flags,
               int32_t volume, char[256]); /*481E0A*/
  int (*group_sound)(void *, unsigned group, char[256]); /*group 0/1 only*/
  int (*camera_setup)(void *, unsigned group, const float values[4],
                      int preset, char[256]);
  int (*target)(void *, float position[3], char[256]); /*old721F08*/
  int32_t voice_volume;
} BkEndingAuxiliaryControllerOps;

/* Recover the 47DC79 state-8 transition prefix. If either native media owner is
 * still active, the native function is a no-op for this frame. The returned
 * state is ready for the paired 48181F presentation only after all services
 * complete. State4 is implemented by ending_auxiliary_state4. */
int bk_ending_auxiliary_controller_begin(
    const BkEndingAuxiliaryControllerBindings *,
    const BkEndingAuxiliaryControllerOps *, char error[256]);
#endif
