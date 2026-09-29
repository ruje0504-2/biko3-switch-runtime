#ifndef BK_GAME_ENDING_TERTIARY_MOTION_H
#define BK_GAME_ENDING_TERTIARY_MOTION_H
#include <stdint.h>
typedef struct {
  int32_t direction[2]; /*54ccdc/e0, process initialized1/1*/
} BkEndingTertiaryMotionState;
typedef struct {
  int32_t duration;
  float start, end, source, rate, elapsed;
} BkEndingTertiaryMotionClip;
BkEndingTertiaryMotionState bk_ending_tertiary_motion_initial(void);
/*479bc9: radial source control relative to the first menu point. Integer
 * differences wrap before float conversion. Coincident anchors keep start;
 * their unused native division result is not made into a visible failure.
 * Updates source only, without requesting/sampling/publishing the actor. */
int bk_ending_tertiary_motion_pointer(BkEndingTertiaryMotionClip *,
                                      const int32_t target[2],
                                      const int32_t menu[2],
                                      const int32_t pointer[2], char error[256]);
/*479cc2: signed INTEGER motion is clamped per group and frame duration,
 * then applied to source with independent primary/auxiliary direction words.
 * Resets elapsed, clamps at start, but deliberately leaves an end overshoot
 * for the later plain scheduler. It switches direction without consuming a
 * clip request. Failure leaves both state and clip unchanged. */
int bk_ending_tertiary_motion_drag(BkEndingTertiaryMotionState *, unsigned group,
                                   unsigned mode, float seconds,
                                   const int32_t motion[2],
                                   BkEndingTertiaryMotionClip *, char error[256]);
#endif
