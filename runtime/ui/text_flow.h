#ifndef BK_UI_TEXT_FLOW_H
#define BK_UI_TEXT_FLOW_H
#include <stdint.h>
typedef struct {
  float delay, scroll, target;
  int32_t enabled, started;
} BkTextFlow;
typedef struct {
  int update;
  int32_t source_y;
} BkTextUpdate;
/*4758f7/474be3: caller compares current text against last DISPLAYED input,
 * even when no bitmap update occurred. enabled==0 holds all fields.
 * started!=0 accumulates seconds*5; bitmap refresh begins strictly above50,
 * and delay is not reset. Otherwise only a changed string starts a refresh.
 * Each refresh advances scroll by seconds*10, strictly overshooting target
 * clamps/disables; exact equality remains enabled. started becomes1.
 * The source row truncates toward0. No allocation, glyphs or GPU commands.
 * Reject invalid time/nonfinite state/crop overflow without changing output.
 */
int bk_text_flow_step(BkTextFlow *, float seconds, int text_changed,
                      BkTextUpdate *);
#endif
