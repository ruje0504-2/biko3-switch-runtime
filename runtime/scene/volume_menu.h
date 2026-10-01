#ifndef BK_SCENE_VOLUME_MENU_H
#define BK_SCENE_VOLUME_MENU_H
#include <stdint.h>
/* Original4ab0c0/4ac224/4ac6a7 volume page. Values are voice/BGM/effect,
 * in hundredths of dB; viewport coordinates retain integer sprite sizes. */
#define BK_VOLUME_SPRITES 23
#define BK_VOLUME_DRAWS 24
#define BK_VOLUME_SOUND_MASK ((1u<<0)|(1u<<3)|(1u<<6)|(1u<<9)|(1u<<10)|(1u<<11))
typedef struct {
  float rect[BK_VOLUME_SPRITES][4];
  float slider[3], minimum, maximum;
  int32_t values[3], selected, row, mouse_result, dragging[3], previewing;
  int32_t samples[3]; /* loaded sample slot, or -1 for original NULL */
  int32_t bar_rect[3][4], disabled[6];
  int32_t cursor[2];
  uint32_t sound_mask;
} BkVolumeMenu;
typedef struct {
  int32_t x, y;
  uint8_t mouse; /* original0 idle,3 press,1 held,2 release */
  uint8_t left, right, up, down, confirm, back, fast;
} BkVolumeMenuInput;
typedef struct {
  unsigned slot;
  float corners[4];
} BkVolumeMenuDraw;
typedef struct {
  unsigned count;
  BkVolumeMenuDraw draws[BK_VOLUME_DRAWS];
} BkVolumeMenuFrame;
typedef struct {
  void *context;
  int (*play)(void *, unsigned slot, int32_t volume, char error[256]);
  int (*stop)(void *, unsigned slot, char error[256]);
  int (*gain)(void *, unsigned slot, int32_t volume, char error[256]);
  int (*playing)(void *, unsigned slot, int *playing, char error[256]);
  int (*random)(void *, uint32_t *value, char error[256]);
  int (*warp)(void *, int32_t x, int32_t y, char error[256]);
} BkVolumeMenuOps;
const char *bk_volume_menu_image(unsigned slot);
const char *bk_volume_menu_sound(unsigned slot);
int bk_volume_menu_initialize(BkVolumeMenu *, unsigned width,
                               const int32_t values[3], char error[256]);
/*501e12 draws before handling this tick's input; repeated rendering only
 *reuses this snapshot. The cursor is intentionally submitted twice.*/
int bk_volume_menu_prepare(const BkVolumeMenu *, BkVolumeMenuFrame *, char error[256]);
/*No file IO or process master-volume writes. Action4 asks the caller to save
 *then publish values; action3 restores original defaults in this page only.
 *The seventh keyboard byte was uninitialized by501e12. Native core accepts
 *it explicitly here; production must define fast=0 rather than read garbage.*/
int bk_volume_menu_step(BkVolumeMenu *, const BkVolumeMenuInput *,
                         const BkVolumeMenuOps *, int *action, char error[256]);
#endif
