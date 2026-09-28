#ifndef BK_SCENE_ITEM_NOTICE_STATE_H
#define BK_SCENE_ITEM_NOTICE_STATE_H
#include "game/item.h"
#include "ui/fade_sprite.h"
#include "ui/text_flow.h"
typedef struct {
  uint32_t duration, deadline;
  BkFadeSprite panel, icons[BK_ITEM_TYPES];
} BkItemNoticeState;
typedef struct {
  int bind_message, draw_text;
} BkItemNoticeFrame;
typedef struct {
  const char *name;
  float x, y, width, height;
} BkItemNoticeSprite;
/* Panel then five icons in native submission order. Authored1280x960
 * coordinates use game viewport width/1280, not whole widescreen target. */
int bk_item_notice_layout(BkItemNoticeSprite out[6], unsigned group,
                          unsigned viewport_width);
/* 4e82b8 notice specialization; reload panel, optionally retain old icons.
 * Does not reset deadline or pickup's shared timer armed/visibility bytes. */
int bk_item_notice_initialize(BkItemNoticeState *, int retain_icons);
/* 51a190 phase1 notice segment only, after the other HUD calls. Timer uses
 * pickup.notice_timer_armed as its sole persistent armed byte. Expiration
 * still binds the message and resets text flow in that frame. Font raster/
 * scrolling runs later, only when draw_text. All invalid inputs atomic. */
int bk_item_notice_step(BkItemNoticeState *, BkItemPickupState *, BkTextFlow *,
                        float seconds, uint32_t now, BkItemNoticeFrame *);
/*51a190 branch0/2/3 after opening phase policy. old_phase is latched BEFORE
 * handover may set phase1. Phase0 steps shared panel and idle11 prompt;
 * phase2 panel only; phase3 neither. No timer or message binding here. */
int bk_opening_notice_step(BkItemNoticeState *, BkPulseSprite *,
                           uint8_t old_phase, uint8_t notice_visible,
                           float seconds, BkItemNoticeFrame *);
#endif
