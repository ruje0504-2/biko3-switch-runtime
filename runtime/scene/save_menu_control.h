#ifndef BK_SCENE_SAVE_MENU_CONTROL_H
#define BK_SCENE_SAVE_MENU_CONTROL_H
#include "scene/pause.h"
typedef struct {
  float cursor[2]; /*BF4B20/24,last pointer sample before keyboard warp*/
  int32_t tab, skip_draw, last_hover, hover;
  int32_t confirm_column, confirm_hover, page, column, row;
  /* Actual loaded back,yes,no sprite rectangles. Retain resize semantics. */
  float back[4], yes[4], no[4];
} BkSaveMenuControl;
typedef struct {
  BkPauseInput ui;
  uint32_t current_group;
  uint8_t occupied[5][10]; /*native presence: stamp[0]!=0*/
} BkSaveMenuInput;
typedef struct {
  BkPauseOps menu;
  /* Real adapters required. load applies chosen area/eight inventory bytes
   * and selected group; store commits a checkpoint before returning success.
   * Resource and text operations preserve native callback ordering. */
  int (*load)(void *, unsigned group, unsigned slot, char error[256]);
  int (*store)(void *, unsigned group, unsigned slot, char error[256]);
  int (*refresh_bank)(void *, unsigned group, char error[256]);
  int (*clear_details)(void *, char error[256]);
  int (*load_details)(void *, unsigned group, char error[256]);
  int (*reset_text)(void *, char error[256]);
  int (*resume_pause)(void *, char error[256]);
} BkSaveMenuOps;
/*507540 (507578 list +50902f confirmation) ONLY, prior to5095d5 draw/fade.
 * Save mode is previous_flow20; title1/pause4 choose load mode. Keeps shared
 * cursor/curtain/hover state. Does not fake a menu renderer or file service.
 * All reached native callbacks run in order. Failure terminates the frame;
 * earlier effects are retained. Invalid native array indices reject safely.
 */
int bk_save_menu_control(BkSaveMenuControl *, const BkPauseBindings *,
                         const BkSaveMenuInput *, const BkSaveMenuOps *,
                         char error[256]);
#endif
