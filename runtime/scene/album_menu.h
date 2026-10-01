#ifndef BK_SCENE_ALBUM_MENU_H
#define BK_SCENE_ALBUM_MENU_H
#include <stdint.h>
enum { BK_ALBUM_SPRITES=42, BK_ALBUM_DRAWS=64, BK_ALBUM_GROUPS=5,
       BK_ALBUM_PAGE=29, BK_ALBUM_PHOTO=40, BK_ALBUM_SLIDE=41 };
typedef struct {
  float rect[4], uv[4], scale[2], alpha;
  uint32_t hidden;
} BkAlbumSprite;
typedef struct {
  BkAlbumSprite sprites[BK_ALBUM_SPRITES];
  uint64_t loaded;
  float scale, curtain, spare_alpha, spare_duration, progress;
  int32_t state, page, hover, group, exit;
  int32_t counts[5], count, live_count;
  int32_t selected[5], tab_hover[5], item_hover[20], button_hover[4];
  int32_t spare_hover[3], deleted[5][100];
  int32_t catalog_index, catalog_subpage, slide_index, erase;
  uint32_t slide_ms;
  uint32_t image_generation[BK_ALBUM_SPRITES]; /* Portable immutable draw identity. */
} BkAlbumMenu;
typedef struct { int32_t x,y; uint32_t delta_ms; uint8_t confirm,back; } BkAlbumInput;
typedef struct { unsigned slot, generation; float corners[4],uv[4],alpha; } BkAlbumDraw;
typedef struct { unsigned count; BkAlbumDraw draws[BK_ALBUM_DRAWS]; } BkAlbumFrame;
typedef enum { BK_ALBUM_RELEASE, BK_ALBUM_ASSET, BK_ALBUM_CATALOG,
               BK_ALBUM_STILL, BK_ALBUM_SLIDESHOW } BkAlbumImageKind;
typedef struct {
  void *context;
  int (*scan)(void *, int32_t counts[5], char[256]);
  int (*catalog)(void *, unsigned page_in_group, unsigned page, char[256]);
  /* Frozen grid names survive deletions. This refreshes only the live
   * slideshow list for the group, as47440d does. */
  int (*select)(void *, unsigned group, int32_t *live_count, char[256]);
  int (*image)(void *, unsigned slot, BkAlbumImageKind, unsigned index,
               unsigned group, uint32_t generation, int *present, char[256]);
  int (*probe)(void *, unsigned group, unsigned live_index, int *present, char[256]);
  int (*remove)(void *, unsigned group, unsigned frozen_index, char[256]);
  int (*sound)(void *, unsigned system_slot, char[256]);
  int (*playing)(void *, unsigned system_slot, int *playing, char[256]);
} BkAlbumOps;
/* Process initialization is separate from entry: the original constructor
 * resets only state/exit, and successful exit resets retained menu globals. */
void bk_album_menu_initialize(BkAlbumMenu *);
const char *bk_album_menu_image(unsigned slot, unsigned variant);
int bk_album_menu_load(BkAlbumMenu *, unsigned width, const BkAlbumOps *, char[256]);
int bk_album_menu_step(BkAlbumMenu *, const BkAlbumInput *, const BkAlbumOps *,
                        BkAlbumFrame *, char[256]);
int bk_album_menu_release(BkAlbumMenu *, const BkAlbumOps *, char[256]);
#endif
