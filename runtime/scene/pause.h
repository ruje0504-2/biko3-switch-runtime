#ifndef BK_SCENE_PAUSE_H
#define BK_SCENE_PAUSE_H
#include "game/flow_transition.h"
#include "scene/common_hud.h"
#define BK_PAUSE_SPRITES 20
#define BK_PAUSE_DRAWS 20
#define BK_PAUSE_CURSOR 20
#define BK_PAUSE_CURTAIN 21
enum {
  BK_PAUSE_CONFIRM = 1,
  BK_PAUSE_UP = 2,
  BK_PAUSE_DOWN = 4,
  BK_PAUSE_LEFT = 8,
  BK_PAUSE_RIGHT = 16
};
typedef struct {
  BkFadeSprite fade;
  float rect[4];
} BkPauseSprite;
typedef struct {
  BkPauseSprite sprites[BK_PAUSE_SPRITES];
  float cursor[2], motion[2]; /*BF9B64/68 and84/88; last UI sensor sample*/
  int32_t row, hover, column, confirm_hover, action, page; /*6c..80*/
  uint8_t background_show, loaded; /*7341BF; actual image residency*/
} BkPauseState;
typedef struct {
  BkPauseSprite sprite; /*B537E8 ma_00.tga, shared by all menus*/
  BkTimer idle;
  uint8_t wanted; /*B5394F*/
} BkMenuCursor;
typedef struct {
  BkCommonHudState *common;
  BkFlowTransition *flow;
  BkMenuCursor *cursor;
  uint8_t *pause_overlay, *hover_latched; /*725D38,BEF178(system slot3)*/
} BkPauseBindings;
typedef struct {
  void *context;
  int (*sound)(void *, unsigned system_slot, char error[256]);
  int (*warp)(void *, float x, float y, char error[256]);
  /* Read after any warp; must return current position and relative motion
   * without changing game/UI state. Position updates the NEXT hit test. */
  int (*pointer)(void *, float position[2], float motion[2], char error[256]);
  int (*release)(void *, uint8_t flow, char error[256]);
  int (*remove_capture)(void *, char error[256]);
} BkPauseOps;
typedef struct {
  uint32_t buttons, now_ms;
  float seconds, scale; /*721AD0=game width/1280, retained layout independent*/
  uint8_t special;
} BkPauseInput;
typedef struct {
  unsigned slot;
  float rect[4], alpha;
} BkPauseDraw;
typedef struct {
  unsigned count;
  BkPauseDraw draws[BK_PAUSE_DRAWS];
} BkPauseFrame;
const char *bk_pause_image(unsigned slot);
/*5158A0 state for successfully loaded images. Retains page/indices/action,
 * cached pointer and shared cursor/hover latch. Warp320,240 is unscaled.
 * Caller supplies the real window pointer service; failure discards menu. */
int bk_pause_initialize(BkPauseState *, unsigned width, const BkPauseOps *,
                        char error[256]);
/* Process cursor initialization from4e6dee; retain timer deadline/armed. */
int bk_menu_cursor_initialize(BkMenuCursor *, unsigned width, unsigned height);
/*51a78e screenshot pass, separate from later51a76d update+UI. */
int bk_pause_backdrop(BkPauseState *, BkPauseFrame *, char error[256]);
/* Full5162D9+5171B5. Logical edge masks represent confirm0/Z/33450,
 * arrows26/28/25/27 with30D40/41/42/43 respectively (all mode1,arg0).
 * Requires current flow4 on entry; later native-ordered release/schedule may
 * change it while this same frame continues stepping UI. Resources released
 * for flow4 skip actual draws but retain/advance their CPU sprite state.
 * Unknown page or reached out-of-range selection index rejects instead of
 * using original uninitialized stack data. All callback failures are fatal.
 */
int bk_pause_step(BkPauseState *, const BkPauseBindings *, const BkPauseInput *,
                  const BkPauseOps *, BkPauseFrame *, char error[256]);
#endif
