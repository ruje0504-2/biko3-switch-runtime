#ifndef BK_SCENE_SAVE_MENU_VIEW_H
#define BK_SCENE_SAVE_MENU_VIEW_H
#include "scene/save_menu_control.h"
#define BK_SAVE_MENU_SPRITES 56
#define BK_SAVE_MENU_CURSOR 56
#define BK_SAVE_MENU_CURTAIN 57
#define BK_SAVE_MENU_DRAWS 48
typedef struct {
  BkPauseSprite sprites[BK_SAVE_MENU_SPRITES];
  uint64_t loaded;
  int32_t detail_slot; /*BF4B40,retained when hover is outside slot rows*/
} BkSaveMenuView;
typedef struct {
  uint32_t area;
  uint8_t occupied, inventory[5];
} BkSaveMenuRecord;
typedef struct {
  unsigned count, text_after; /*UINT32_MAX when text is skipped*/
  BkPauseDraw draws[BK_SAVE_MENU_DRAWS];
} BkSaveMenuFrame;
typedef struct {
  void *context;
  /*4aaa01/4aa9f4/reset flags/4aaa17: actual Type_G list rendering service.
   * Called at text_after; GPU draw must preserve that snapshot position. */
  int (*text)(void *, unsigned group, float seconds, char error[256]);
  int (*motion)(void *, float out[2], char error[256]);
} BkSaveMenuViewOps;
/*4e97fd/50ace3 geometry/fade portion; caller owns bank and font operations.
 * Constructor retains detail_slot and unloaded CPU slots. No cursor warp.
 * Request mode is0(load) or1(save); width is the actual 4:3 viewport width. */
int bk_save_menu_view_initialize(BkSaveMenuView *, unsigned mode,
                                 unsigned group, unsigned width);
/*42..55 resources must really be replaced by owner before this CPU reset. */
int bk_save_menu_view_details(BkSaveMenuView *, unsigned group, unsigned width);
const char *bk_save_menu_image(unsigned slot, unsigned group, unsigned mode);
/*5095d5 with full50c371 detail draw policy. Resource release must clear loaded
 * bits before this step if applicable. Uses this frame's control.cursor even
 * if keyboard services have warped the external pointer since the sample.
 * No file, GPU or font backend is fabricated here. */
int bk_save_menu_view_step(BkSaveMenuView *, BkSaveMenuControl *,
                           const BkPauseBindings *, const BkPauseInput *,
                           unsigned mode, const BkSaveMenuRecord records[5][10],
                           const BkSaveMenuViewOps *, BkSaveMenuFrame *,
                           char error[256]);
#endif
