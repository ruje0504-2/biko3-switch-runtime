#ifndef BK_SCENE_TITLE_MENU_H
#define BK_SCENE_TITLE_MENU_H
#include "scene/pause.h"
#include "ui/zoom_sprite.h"
#define BK_TITLE_SPRITES 13
#define BK_TITLE_CURSOR 13
#define BK_TITLE_CURTAIN 14
#define BK_TITLE_DRAWS 9
typedef struct {
  BkZoomSprite zoom;
  float rect[4];   /* native position (center for buttons), authored size */
  int32_t half[2]; /* constructor truncation; not animated scale */
} BkTitleSprite;
typedef struct {
  BkTitleSprite sprites[BK_TITLE_SPRITES];
  int32_t row;          /*BF9B90 retained until transition completes*/
  int32_t music_volume; /*728DD4*/
  uint8_t music_mode;   /*728DD8*/
  uint16_t loaded_mask;
} BkTitleMenuState;
typedef struct {
  BkCommonHudState *common;
  BkFlowTransition *flow;
  BkMenuCursor *cursor;
  uint8_t *hover_latched;
} BkTitleMenuBindings;
typedef struct {
  void *context;
  int (*sound)(void *, unsigned system_slot, char error[256]);
  int (*music_gain)(void *, int32_t volume, char error[256]);
  int (*warp)(void *, float x, float y, char error[256]);
  int (*position)(void *, float position[2], char error[256]);
  /* Read separately after keyboard warps and cursor submission. */
  int (*motion)(void *, float motion[2], char error[256]);
  int (*release)(void *, uint8_t flow, char error[256]);
} BkTitleMenuOps;
typedef struct {
  uint32_t buttons, now_ms; /*BK_PAUSE_CONFIRM/UP/DOWN edge masks*/
  float seconds, scale;
  int32_t music_master;
  uint8_t special;
} BkTitleMenuInput;
typedef struct {
  unsigned slot;
  float corners[4], alpha;
} BkTitleMenuDraw;
typedef struct {
  unsigned count;
  BkTitleMenuDraw draws[BK_TITLE_DRAWS];
} BkTitleMenuFrame;
/*4e7917; successful resources only. Special mode retains skipped slots.
 * Music owner loads/loops bk3_02/bg001.wav at music_master beforehand.
 * Constructor warps320,240 independently of scale; row is retained. */
const char *bk_title_menu_image(unsigned slot, uint8_t special);
int bk_title_menu_initialize(BkTitleMenuState *, unsigned width,
                             uint8_t special, int32_t music_master,
                             const BkTitleMenuOps *, char error[256]);
/*Complete519476, including50db23 and51c47e. Only selected button image
 * advances. Frames capture pre-release draw state, so draw is idempotent.
 * All services required; failure terminates the frame, retaining preceding
 * ordered side effects. Targets must be implemented by the owner. */
int bk_title_menu_step(BkTitleMenuState *, const BkTitleMenuBindings *,
                       const BkTitleMenuInput *, const BkTitleMenuOps *,
                       BkTitleMenuFrame *, char error[256]);
#endif
