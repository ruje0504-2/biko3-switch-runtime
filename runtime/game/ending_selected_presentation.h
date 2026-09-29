#ifndef BK_GAME_ENDING_SELECTED_PRESENTATION_H
#define BK_GAME_ENDING_SELECTED_PRESENTATION_H
#include "game/ending_presentation.h"
#include "world/actor_pose.h"
#include "world/face_controller.h"

typedef struct {
  BkEndingFrameState *frame;
  BkEndingAuxiliaryState *auxiliary;
  BkFaceState *face;
  const int32_t *mode; /*live UI6dde94, not a second controller mode*/
  const int32_t *plain_scheduled; /*raw b53c38, nonzero selects402e18*/
  const int32_t *face_mode; /*721dfc*/
  int32_t *expression_override; /*6dde98*/
  int32_t *eye_lower; /*721df8*/
  int32_t *expression_latch; /*6ea354*/
  const float *scale; /*live55469c, shared with both auxiliary tickers*/
  const uint8_t *toggles; /*7220f8..ff*/
  const uint32_t *primary_root, *background_root;
  const uint32_t *hidden_nodes; /*three709ef8 nodes; zero means absent*/
  const uint32_t *secondary_node; /*719b44+4, not the first word*/
} BkEndingSelectedPresentationBindings;
typedef struct {
  void *context;
  int (*clock)(void *, uint32_t *, char[256]);
  int (*advance)(void *, BkEndingPresentationActor, float, char[256]);
  int (*find)(void *, uint32_t, const char *, uint32_t *, char[256]);
  int (*hide)(void *, uint32_t, uint32_t, char[256]);
  int (*auxiliary_tick)(void *, char[256]); /*actual4968cb, not4965b9*/
  int (*active)(void *, int32_t *, char[256]);
  int (*duration)(void *, unsigned slot, int32_t *, char[256]);
  /*The three original helpers receive the actual primary and zero seconds.
   *The scene must submit their ANIM/MATA/MORP, not just change a clip clock.*/
  int (*controlled)(void *, BkClipPlainMode, char[256]);
  int (*material)(void *, const char *, uint32_t, float, char[256]);
  int (*publish)(void *, char[256]);
  int (*expression)(void *, int32_t, char[256]);
  int (*eye_range)(void *, float minimum, float maximum, char[256]);
  int (*gaze)(void *, float minimum, float maximum, char[256]);
  int (*blink)(void *, uint32_t, char[256]);
  int (*level)(void *, float *, char[256]);
  int (*mouth)(void *, float, uint32_t, char[256]);
} BkEndingSelectedPresentationOps;
/*Entire494015 dispatch. Background advances before hiding; per-clip ordinary or
 *controlled primary sampling, actual material/subtree toggles, then one
 *publication and the original face sequence. This owner borrows every
 *process alias. It does not load a scene, advance video/camera or replace
 *a missing child. Services can change live aliases; later branches reread
 *them. A service failure terminates the frame and retains its executed prefix.*/
int bk_ending_selected_presentation_step(
    const BkEndingSelectedPresentationBindings *, float seconds,
    const BkEndingSelectedPresentationOps *, char error[256]);
#endif
