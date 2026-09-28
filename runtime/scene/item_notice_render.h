#ifndef BK_SCENE_ITEM_NOTICE_RENDER_H
#define BK_SCENE_ITEM_NOTICE_RENDER_H
#include "resource/message.h"
#include "scene/failure_hud.h"
#include "scene/item_notice_state.h"
#include "scene/opening_session.h"
#include "scene/text_render.h"
typedef struct BkItemNoticeRender BkItemNoticeRender;
/* Owns the shared opening/item panel, icons, prompt and font/GPU resources.
 * Does not own gameplay state, audio or pickup services. Create/destroy and
 * prepare outside renderer frames. Store must outlive this owner.
 * Common HUD is separate. */
BkItemNoticeRender *bk_item_notice_render_create(BkRenderer *,
                                                 BkResourceStore *,
                                                 unsigned group,
                                                 char error[256]);
void bk_item_notice_render_destroy(BkItemNoticeRender *);
/* Recreates panel, reloads icons unless retained (native BEF778), preserving
 * their old group when retained. CPU stage initialization remains explicit.
 * Failure preserves prior owned resources; invalid profiles always reject. */
int bk_item_notice_render_reload(BkItemNoticeRender *, BkResourceStore *,
                                 unsigned group, int retain_icons,
                                 char error[256]);
/* Phase1 notice timer/sprites, then conditional font prepare. message is
 * borrowed when bind_message occurs; it must remain live until replacement
 * or destruction. This retains pointer identity through mutable messages.
 * Text starts bound to an empty message. CPU state and previous GPU changes
 * remain on GPU/font failure: caller must terminate the frame. Caller sets
 * the corresponding viewport before draw. Font flow is shared caller state. */
int bk_item_notice_render_prepare(BkItemNoticeRender *, BkItemNoticeState *,
                                  BkItemPickupState *, const BkMessage *,
                                  BkTextFlow *, float seconds, uint32_t now,
                                  unsigned width, unsigned height,
                                  char error[256]);
BkNoticeTextOps bk_item_notice_render_text_ops(BkItemNoticeRender *);
int bk_item_notice_render_prepare_opening(BkItemNoticeRender *,
                                          BkItemNoticeState *, BkPulseSprite *,
                                          uint8_t old_phase, uint8_t visible,
                                          BkTextFlow *, float seconds,
                                          unsigned width, unsigned height,
                                          char error[256]);
/* Prepared51afcb snapshot: no second sprite advance/request. */
int bk_item_notice_render_prepare_failure(
    BkItemNoticeRender *, const BkItemNoticeState *, const BkPulseSprite *,
    const BkFailureHudFrame *, BkTextFlow *, float seconds, unsigned width,
    unsigned height, char error[256]);
int bk_item_notice_render_draw(BkItemNoticeRender *, char error[256]);
const BkTextRender *bk_item_notice_render_text(const BkItemNoticeRender *);
const BkItemNoticeFrame *
bk_item_notice_render_frame(const BkItemNoticeRender *);
#endif
