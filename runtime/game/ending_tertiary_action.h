#ifndef BK_GAME_ENDING_TERTIARY_ACTION_H
#define BK_GAME_ENDING_TERTIARY_ACTION_H
#include "game/ending_tertiary_control.h"

/* Independent process words6afd24/54ccd8. Do not alias the parent's
 *6afd20/54ccd4 replay clock or reset this clock on a resource replacement. */
typedef struct {
  float replay_elapsed;
  int32_t replay_after;
} BkEndingTertiaryActionState;
typedef struct {
  BkEndingTertiaryControlBindings control;
  float *gauge_y;       /*721e24*/
  const float *scale;   /*721ad0*/
  int32_t *choices;     /*three7220e4 words, written by478eab*/
} BkEndingTertiaryActionBindings;
typedef enum {
  BK_ENDING_TERTIARY_ACTION_PAIR, /*54ae00, two names/group*/
  BK_ENDING_TERTIARY_ACTION_SIX,  /*548f88, six names/group*/
  BK_ENDING_TERTIARY_ACTION_FOUR  /*54b828, four names/group*/
} BkEndingTertiaryActionMaterial;
typedef struct {
  BkEndingTertiaryControlOps control;
  /*Original722574 + slot*120. Distinct from the speech slots in control.*/
  int (*effect_slot)(void *, unsigned slot, int32_t volume, char[256]);
  int (*material)(void *, BkEndingTertiaryActionMaterial, unsigned group,
                   unsigned slot, int hidden, float alpha, char[256]);
  int (*manual)(void *, const BkEndingFrameInput *, const int32_t point[2],
                 char[256]); /*479bc9, input copied by value*/
  int (*drag)(void *, unsigned actor, const uint32_t motion[2], unsigned mode,
               char[256]); /*479cc2, original return deliberately ignored*/
  int (*hit)(void *, unsigned menu, const int32_t pointer[2], int *,
              char[256]); /*actual4a777e with slot51 half-width*/
  int (*rewind)(void *, unsigned actor, unsigned clip, char[256]);
  int (*refresh)(void *, unsigned actor, char[256]); /*402e18(actor,0)*/
  int (*place_menu)(void *, int32_t angle, int32_t step,
                     const int32_t point[2], char[256]); /*495125*/
} BkEndingTertiaryActionOps;

/*478eab choice setup and edge-zone dispatch. No qualifying choice preserves
 * all three old words. Projected target is reread after the zone callback.
 * The geometry service must place the actual UI; missing services fail. */
int bk_ending_tertiary_action_begin(const BkEndingTertiaryActionBindings *,
                                    const BkEndingTertiaryActionOps *,
                                    char error[256]);
/*Complete47811c parent action rules. Keeps the entry record owner while
 * rereading live actor/group/state after services. Cancellation decrements
 * the signed32 record counter with native wrapping, including count0->-1;
 * no record-array access is performed here. Three hit circles are tested
 * regardless of their label values. Auxiliary source rewinds precede both
 *402e18 submissions and then both hides. Later failure retains its prefix.
 * This does not supply the manual/drag, model, geometry or audio children. */
int bk_ending_tertiary_action_step(BkEndingTertiaryActionState *,
                                   const BkEndingTertiaryActionBindings *,
                                   const BkEndingFrameInput *, float seconds,
                                   const BkEndingTertiaryActionOps *,
                                   char error[256]);
#endif
