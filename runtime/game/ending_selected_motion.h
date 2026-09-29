#ifndef BK_GAME_ENDING_SELECTED_MOTION_H
#define BK_GAME_ENDING_SELECTED_MOTION_H
#include <stdint.h>

/* A value view of the active descriptor. Only source is written; the scene
 * commits it to the same descriptor captured before calling this service. */
typedef struct {
  int32_t duration;
  float start, end, source, rate, elapsed;
} BkEndingSelectedMotionClip;

/*495469: signed32 screen-coordinate differences wrap before conversion.
 * Inside the anchor radius, source uses the authored offset30, NOT start.
 * Outside/on that radius (including coincident anchors), it uses start.
 * No animation request, sampling, publication or elapsed-time update. */
int bk_ending_selected_motion_pointer(BkEndingSelectedMotionClip *,
                                      const int32_t target[2],
                                      const int32_t menu[2],
                                      const int32_t pointer[2], char error[256]);

/*4952c8: motion components are FLOAT values, not integers or pixel words.
 * plain_scheduled is the live B53C38 setting and reverse is6EA314.
 * The ordinary branch consumes only X, subtracting/clamping at start when
 * reverse is nonzero. The scheduled branch adds abs(X)+abs(Y) and ignores
 * reverse. Duration selects the original coefficient; there is no extra dt
 * factor and no clamp at end. Input/overflow failure preserves the clip. */
int bk_ending_selected_motion_drag(BkEndingSelectedMotionClip *,
                                   int32_t plain_scheduled, int32_t reverse,
                                   const float motion[2], char error[256]);
#endif
