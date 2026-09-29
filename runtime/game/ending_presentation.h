#ifndef BK_GAME_ENDING_PRESENTATION_H
#define BK_GAME_ENDING_PRESENTATION_H
#include "game/ending_auxiliary.h"
/* Live aliases read by4df6c0. Node values are portable scene identifiers;
 * zero denotes the native absent reference. No resource ownership here. */
typedef struct {
  BkEndingFrameState *frame;
  BkEndingAuxiliaryState *auxiliary;
  const int32_t *normal_ready;
  const uint8_t *toggles; /*7220f8..7220ff, eight entries*/
  const uint32_t *primary_root, *background_root, *secondary;
  const uint32_t *direct_reference, *direct_node; /*719b28/719b2c*/
  const int32_t *flip; /*714fa8*/
} BkEndingPresentationBindings;
typedef enum {
  BK_ENDING_PRESENT_PRIMARY,
  BK_ENDING_PRESENT_SECONDARY,
  BK_ENDING_PRESENT_BACKGROUND
} BkEndingPresentationActor;
typedef enum {
  BK_ENDING_PRESENT_EYE_RANGE,
  BK_ENDING_PRESENT_EXPRESSION,
  BK_ENDING_PRESENT_BLINK,
  BK_ENDING_PRESENT_MOUTH
} BkEndingPresentationFace;
typedef struct {
  void *context;
  int (*clock)(void *, uint32_t *, char error[256]);
  int (*advance)(void *, BkEndingPresentationActor, float, char error[256]);
  int (*find)(void *, uint32_t root, const char *, uint32_t *, char error[256]);
  int (*hide)(void *, uint32_t node, uint32_t hidden, char error[256]);
  int (*material)(void *, const char *, uint32_t hidden, float alpha,
                  char error[256]);
  int (*disable_bom)(void *, unsigned binding, int32_t disabled,
                     char error[256]);
  int (*publish)(void *, char error[256]);
  /*kind0/1:49aa50/49ad16, binding0/1; kind2:49afde explicit node pair.
   * The original11-word input is passed unchanged. radius=.09,degrees=7.5.
   * Missing references retain the native no-op; no guessed mouse deltas. */
  int (*manual)(void *, unsigned kind, const BkEndingFrameInput *,
                uint32_t reference, uint32_t node, int32_t flip,
                char error[256]);
  int (*follow)(void *, char error[256]); /*ordered422c49/4230bd per binding*/
  int (*face)(void *, BkEndingPresentationFace, float value,
              int32_t expression, uint32_t timestamp, char error[256]);
  int (*gaze)(void *, char error[256]); /*4a0823, limits .001/.2*/
  int (*level)(void *, float *level, char error[256]); /*speech0 consumed PCM*/
} BkEndingPresentationOps;
/* Complete4df6c0 orchestration. Background full speed; primary .08 when
 * ready6, otherwise group1 .6 and others .5; auxiliary only in state3.
 * Two publications enclose manual/follow edits. Face order is range,gaze,
 * expression,blink,live speech envelope,mouth. Neither camera track nor
 * movie is advanced here. Services must implement their actual operation.
 * Later failures preserve the native prefix; callbacks can mutate aliases
 * which are read again at their original decision points. */
int bk_ending_presentation_step(const BkEndingPresentationBindings *,
                                 const BkEndingFrameInput *, float seconds,
                                 const BkEndingPresentationOps *,
                                 char error[256]);
const char *bk_ending_presentation_material(unsigned group, unsigned index);
#endif
