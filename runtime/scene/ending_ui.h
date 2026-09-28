#ifndef BK_SCENE_ENDING_UI_H
#define BK_SCENE_ENDING_UI_H
#include "core/timer.h"
#include "game/ending_control.h"
#include "ui/effect_sprite.h"
#define BK_ENDING_UI_SPRITES 63
typedef struct {
  BkEffectSprite transform;
  float rect[4], uv[4];
  uint32_t rgb;
  BkTimer timer;
} BkEndingUiSprite;
typedef struct {
  BkEndingUiSprite sprites[BK_ENDING_UI_SPRITES];
  uint64_t loaded;
} BkEndingUi;
#define BK_ENDING_UI_DRAWS 128
typedef struct {
  unsigned slot;
  float xy[8], uv[4], alpha;
  uint32_t rgb;
} BkEndingUiDraw;
typedef struct {
  unsigned count;
  BkEndingUiDraw draws[BK_ENDING_UI_DRAWS];
} BkEndingUiFrame;
/* One original50e6ba primary call: advance once and capture immutable draw
 * values. Does not choose visibility/order, or stand in for full4d499b. */
int bk_ending_ui_sprite_step(BkEndingUi *, unsigned slot, float seconds,
                             BkEndingUiDraw *, char error[256]);
/* Shared single-element implementation. Caller has checked resource ownership;
 * slot is copied unchanged to the immutable draw for its texture owner. */
int bk_ending_ui_element_step(BkEndingUiSprite *, unsigned slot, float seconds,
                              BkEndingUiDraw *, char error[256]);
/* bk3_00 image names. Slot8 is not constructed; it is retained, not an
 * implicit missing-resource fallback. This module is layout/animation only:
 * full4d499b draw/control scheduling belongs to a subsequent adapter. */
const char *bk_ending_ui_image(unsigned slot);
/*4ccabd..4ce7a9. Constructor-retained timers/directions/motion survive;
 * clears the six shared control pause_flags and assigns721e24 gauge_y.
 * Does not reset gameplay/camera/menu-open state or load textures. */
int bk_ending_ui_initialize(BkEndingUi *, unsigned width,
                            uint8_t pause_flags[6], float *gauge_y,
                            char error[256]);
/* Exports the live moving rectangles read by4d7ac4, in original hit order. */
int bk_ending_ui_control_rects(const BkEndingUi *,
                               BkEndingControlRect[BK_ENDING_CONTROL_RECTS]);
typedef struct {
  void *context;
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *result,
             char error[256]);
} BkEndingUiHoverOps;
/* Complete4d901c only, called at its original caller's gates. Inclusive
 * pointer tests use unrounded scaled bounds. Key0/1 are queried in order,
 * using low AL. open is the live72210c, request the live722110. Invalid
 * input/key failure rejects without moving sprites or changing open/output. */
int bk_ending_ui_hover(BkEndingUi *, int32_t *open, const int32_t *request,
                       const float pointer[2], float scale, float seconds,
                       const BkEndingUiHoverOps *, uint8_t *visible,
                       char error[256]);
#endif
