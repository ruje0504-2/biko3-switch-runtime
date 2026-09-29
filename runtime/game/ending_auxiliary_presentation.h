#ifndef BK_GAME_ENDING_AUXILIARY_PRESENTATION_H
#define BK_GAME_ENDING_AUXILIARY_PRESENTATION_H
#include "game/ending_presentation.h"
#include "game/ending_control.h"
#include "model/clip.h"
#include "world/face_controller.h"

/* Portable boundary for native 48181F. It owns no resource, device, audio or
 * node handle; the scene supplies those services and keeps their identities. */
typedef enum {
  BK_ENDING_AUX_PRESENT_PRIMARY,
  BK_ENDING_AUX_PRESENT_BACKGROUND
} BkEndingAuxiliaryPresentationActor;
typedef struct {
  BkEndingFrameState *frame;
  const BkEndingControlState *control; /*721EEC*/
  BkEndingAuxiliaryState *auxiliary;
  BkFaceState *face;
  const int32_t *face_mode;       /*721DFC*/
  int32_t *expression_override;   /*6BBE48*/
  int32_t *eye_lower;             /*721DF8*/
  int32_t *expression_latch;      /*6C7F48*/
  const uint8_t *toggles;         /*7220F8..FF*/
  const uint32_t *primary_root;
  const uint32_t *background_root;
  const uint32_t *hidden_nodes;   /*three optional pre-resolved nodes*/
  const uint32_t *secondary_node; /*optional 719B44+4*/
} BkEndingAuxiliaryPresentationBindings;
typedef struct {
  void *context;
  int (*clock)(void *, uint32_t *, char[256]);
  int (*advance)(void *, BkEndingAuxiliaryPresentationActor, float,
                 char[256]);              /*4026FE*/
  int (*plain)(void *, BkEndingAuxiliaryPresentationActor, float,
               BkClipPlainMode, char[256]); /*402E18*/
  int (*active)(void *, BkEndingAuxiliaryPresentationActor, int32_t *,
                char[256]);
  int (*hide)(void *, uint32_t, uint32_t, char[256]); /*423A99*/
  int (*material)(void *, const char *, uint32_t, float, char[256]);
  int (*publish)(void *, char[256]);                 /*423BE2*/
  int (*expression)(void *, int32_t, char[256]);
  int (*eye_range)(void *, float, float, char[256]);
  int (*gaze)(void *, float, float, char[256]);
  int (*blink)(void *, uint32_t, char[256]);
  int (*level)(void *, float *, char[256]);
  int (*mouth)(void *, float, uint32_t, char[256]);
} BkEndingAuxiliaryPresentationOps;

/* Complete the recovered 48181F presentation ordering. It deliberately does
 * not implement the preceding 47DC79 controller or its media/UI services.
 * A missing service fails at the native boundary and preserves the executed
 * prefix; no empty callback represents a game action. */
int bk_ending_auxiliary_presentation_step(
    const BkEndingAuxiliaryPresentationBindings *, float seconds,
    const BkEndingAuxiliaryPresentationOps *, char error[256]);
#endif
