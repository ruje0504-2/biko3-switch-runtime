#ifndef BK_SCENE_DIALOGUE_UI_H
#define BK_SCENE_DIALOGUE_UI_H
#include "resource/dialogue.h"
#include "scene/dialogue_backdrop.h"
#include "ui/text_draw.h"
#include "ui/text_flow.h"
typedef struct {
  const char *image;
  float x, y, width, height;
} BkDialogueUiSprite;
int bk_dialogue_ui_layout(BkDialogueUiSprite out[2], unsigned viewport_width);
int bk_dialogue_ui_text_style(BkTextStyle *);
typedef struct {
  BkFadeSprite panel;                            /*734058*/
  BkPulseSprite prompt;                          /*7341c4 idle11*/
  int32_t columns, rows, step_y, text_parameter; /*6a13c0/c4/cc,bf00f8*/
} BkDialogueUi;
typedef struct {
  int32_t group, kind, choice; /*728de0,729788,729784*/
} BkDialogueResult;
typedef struct {
  BkDialogue *dialogue;
  BkTextFlow *text;
  BkDialogueBackdrop *backdrop;
  uint8_t *phase;
  BkFadeSprite *curtain; /*common beea18, not backdrop's729c10*/
  uint8_t *curtain_wanted;
  BkDialogueResult *result;
} BkDialogueUiBindings;
typedef struct {
  float seconds;
  int advance, voice_present;
  uint8_t previous, response; /*721ad4,71bcda*/
  uint32_t group;
  int32_t area;
} BkDialogueUiInput;
typedef struct {
  void *context;
  int (*pause_voice)(void *, char error[256]);
  int (*next)(void *, int *done, char error[256]);
  int (*bind)(void *, char error[256]);
  /*4aaa17 ignores its apparent two arguments and uses global game seconds.
   * This callback prepares the real font/canvas and advances shared text. */
  int (*text)(void *, float seconds, char error[256]);
  int (*speech)(void *, char error[256]);
  int (*music)(void *, char error[256]);
  /*50c6bc writes the actual persistent unlock table; not just a bit setter. */
  int (*unlock)(void *, unsigned group, char error[256]);
  int (*schedule)(void *, uint8_t target, uint8_t mode, char error[256]);
} BkDialogueUiOps;
typedef struct {
  float panel_alpha, prompt_alpha, curtain_alpha;
} BkDialogueUiFrame;
/*4ef6a0 widget/text-layout subset. Preserve prompt direction and unrelated
 * globals. Text binding/loading and first-page scroll target remain explicit.
 */
void bk_dialogue_ui_initialize(BkDialogueUi *);
/* Initial uses strlen; later pages use4f1ab4's raw-byte CR expansion. Returns
 * the native target for (effective_length-1)/2 characters. Bounded input;
 * does not reinterpret Shift-JIS or substitute generic line wrapping. */
int bk_dialogue_text_target(const BkMessage *, unsigned columns, unsigned rows,
                            unsigned step_y, int initial, float *target,
                            char error[256]);
/* Complete4f0e44 control/service order. Caller already ran actor/backdrop.
 * Captures widget/curtain draw alpha; callbacks prepare actual text/audio and
 * schedule native flow transitions. All callbacks mandatory. Invalid input
 * rejects before mutation; a service failure preserves the executed prefix
 * and terminates this frame. No fake unlock/transition fallback. */
int bk_dialogue_ui_step(BkDialogueUi *, const BkDialogueUiBindings *,
                        const BkDialogueUiInput *, const BkDialogueUiOps *,
                        BkDialogueUiFrame *, char error[256]);
#endif
