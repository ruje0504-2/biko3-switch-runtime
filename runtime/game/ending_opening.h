#ifndef BK_GAME_ENDING_OPENING_H
#define BK_GAME_ENDING_OPENING_H
#include "game/ending_auxiliary.h"
#include "game/ending_control.h"
/*575644 is independent of the camera's current FOV. Initialize once to1;
 * state0 of the opening also resets it. Resource reload is not a reset. */
typedef struct {
  float fov;
} BkEndingOpeningRetained;
typedef struct {
  BkEndingFrameState *frame;
  BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  BkMenuCamera *camera;
  int32_t *substate, *voice_wait; /*719b50/719b24, existing live owners*/
  const int32_t *next_mode;
  const int8_t *previous_flow;
  const int32_t *voice_volume;
  char *speech_name; /*at least32 bytes;722224, written only for speech0*/
  BkEndingOpeningRetained *retained;
} BkEndingOpeningBindings;
typedef enum {
  BK_ENDING_OPENING_TRACK,
  BK_ENDING_OPENING_PRESET
} BkEndingOpeningCamera;
typedef struct {
  void *context;
  /* The preset arguments are captured before the call; its sixth word is
   * passed intact although the original4e0ecb does not consume it. */
  int (*camera)(void *, BkEndingOpeningCamera, int32_t choice,
                 const uint32_t offset[3], uint32_t extra,
                 uint32_t *result, char error[256]);
  int (*target)(void *, float position[3], char error[256]); /*old721f08*/
  int (*present)(void *, unsigned slot, int *, char error[256]);
  int (*status)(void *, unsigned slot, int *, char error[256]);
  int (*load)(void *, unsigned slot, const char *, char error[256]);
  int (*play)(void *, unsigned slot, int32_t volume, char error[256]);
  int (*voice_name)(void *, char name[32], char error[256]); /*4e0818(0,0,0)*/
} BkEndingOpeningOps;
/* Entire state4 branch4db68f..4dbb11 of4db608. One captured substate per
 * call; no fallthrough into the next substate. Keep submit-before-decrement
 * FOV order, low-AL camera completion, old matrix/target capture, independent
 * speech presence/status and callback-visible state writes. Does not advance
 * actors or publish the forest; those belong to4df6c0/the drawing stage.
 * Non-state4 calls reject rather than pretending other parent states exist.
 * An unknown substate is the original no-op. Later failures keep the prefix.
 */
int bk_ending_opening_step(const BkEndingOpeningBindings *, float seconds,
                            const BkEndingOpeningOps *, char error[256]);
#endif
