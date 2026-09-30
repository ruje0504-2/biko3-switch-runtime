#ifndef BK_SCENE_GALLERY_MENU_H
#define BK_SCENE_GALLERY_MENU_H
#include "scene/pause.h"
#define BK_GALLERY_SPRITES 49
#define BK_GALLERY_CURSOR 49
#define BK_GALLERY_CURTAIN 50
#define BK_GALLERY_DRAWS 36
enum { BK_GALLERY_BACK = 32 };
typedef struct {
  BkPauseSprite sprites[BK_GALLERY_SPRITES];
  float pointer[2]; /*709B20/24: previous sensor sample, used by hit tests*/
  int16_t group, requested_action, transition, picture;
  int32_t view_gate, image_view, music_volume;
  int32_t actions[5][9], pictures[5][5], hover[20];
  uint64_t loaded;
} BkGalleryMenu;
typedef struct {
  BkCommonHudState *common;
  BkMenuCursor *cursor;
} BkGalleryMenuBindings;
typedef struct {
  void *context;
  int (*sound)(void *, unsigned system_slot, char error[256]);
  /* NULL name releases this slot. Keep images referenced by the current
   * prepared frame alive until the next real tick (including picture close). */
  int (*image)(void *, unsigned slot, const char *name, char error[256]);
  int (*position)(void *, float out[2], char error[256]);
} BkGalleryMenuOps;
typedef struct {
  uint32_t buttons; /*CONFIRM, BACK edges; no native arrow navigation*/
  float seconds, scale;
  unsigned height; /*actual content height for the square final portrait*/
} BkGalleryMenuInput;
typedef struct {
  unsigned slot;
  float corners[4], alpha;
} BkGalleryMenuDraw;
typedef struct {
  unsigned count;
  BkGalleryMenuDraw draws[BK_GALLERY_DRAWS];
  int32_t action; /*4C8768:99 none;0..8 selection;10 return to title*/
} BkGalleryMenuFrame;
typedef struct {
  int32_t group, variant, selection; /*728DE0,729788,729784*/
} BkGalleryMenuSelection;
typedef struct {
  void *context;
  int (*release)(void *, uint8_t flow, char error[256]);
  /*4F75DD with area0, before scheduling flow48.*/
  int (*story)(void *, unsigned group, char error[256]);
} BkGalleryDispatchOps;
/*4C77B1. mode0 eight thumbnails; mode1 five lookup entries (last duplicated);
 * mode2/3 character portrait. Other modes and out-of-range indices reject.*/
int bk_gallery_menu_name(char out[32], unsigned group, unsigned index,
                          unsigned mode);
int bk_gallery_menu_layout(unsigned slot, unsigned group, float scale,
                            unsigned height, unsigned picture,
                            char name[32], float rect[4]);
/*4C59C0. Only fields written by the original loader are initialized; caller
 * owns process-zero initialization. It loads the actual48 images in native
 * order. The audio owner must then load/loop bk3_02/bg002.wav at master.*/
int bk_gallery_menu_initialize(BkGalleryMenu *, unsigned width,
                               uint8_t previous, unsigned ending_group,
                               unsigned game_group, const uint8_t unlocked[5][8],
                               int32_t master, const BkGalleryMenuOps *,
                               char error[256]);
/*4C8768 including4C832B and character image replacement4C7916. Captures
 * draw state before releases; drawing the frame never advances the menu.*/
int bk_gallery_menu_step(BkGalleryMenu *, const BkGalleryMenuBindings *,
                         const BkGalleryMenuInput *, const BkGalleryMenuOps *,
                         BkGalleryMenuFrame *, char error[256]);
/*51B9B3 after the menu step. Selection fields are written before release;
 * action8 uses story service,10 goes to title, all other values do nothing.*/
int bk_gallery_menu_dispatch(const BkGalleryMenu *, int32_t action,
                             BkGalleryMenuSelection *, BkFlowTransition *,
                             const BkGalleryDispatchOps *, char error[256]);
#endif
