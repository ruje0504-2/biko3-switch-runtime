#include "scene/item_notice_state.h"
#include "core/timer.h"
#include <math.h>
int bk_item_notice_layout(BkItemNoticeSprite out[6], unsigned group,
                          unsigned width) {
  static const char *const icons[5][5] = {
      {"ma_16.tga", "ma_14.tga", "ma_15.tga", "ma_17.tga", "ma_18.tga"},
      {"ma_20.tga", "ma_14.tga", "ma_19.tga", "ma_21.tga", "ma_22.tga"},
      {"ma_25.tga", "ma_14.tga", "ma_24.tga", "ma_26.tga", "ma_27.tga"},
      {"ma_28.tga", "ma_14.tga", "ma_15.tga", "ma_29.tga", "ma_30.tga"},
      {"ma_32.tga", "ma_14.tga", "ma_31.tga", "ma_33.tga", "ma_34.tga"}};
  if (!out || group >= 5 || !width || width > 16384)
    return 0;
  float scale = (float)((double)width / 1280);
  out[0] = (BkItemNoticeSprite){"ma_04.tga", 138 * scale, 727 * scale,
                                1010 * scale, 208 * scale};
  for (unsigned i = 0; i < 5; ++i)
    out[i + 1] = (BkItemNoticeSprite){icons[group][i], 192 * scale, 768 * scale,
                                      128 * scale, 128 * scale};
  return 1;
}
int bk_item_notice_initialize(BkItemNoticeState *s, int retain_icons) {
  if (!s)
    return 0;
  bk_fade_sprite_initialize(&s->panel);
  s->duration = 5000;
  if (!retain_icons)
    for (unsigned i = 0; i < BK_ITEM_TYPES; ++i)
      bk_fade_sprite_initialize(&s->icons[i]);
  return 1;
}
int bk_item_notice_step(BkItemNoticeState *s, BkItemPickupState *pickup,
                        BkTextFlow *flow, float seconds, uint32_t now,
                        BkItemNoticeFrame *out) {
  if (!s || !pickup || !flow || !out)
    return 0;
  BkItemNoticeState next = *s;
  BkItemPickupState p = *pickup;
  BkTextFlow f = *flow;
  BkItemNoticeFrame frame = {0};
  if (p.notice_visible == 1) {
    BkTimer timer = {next.duration, next.deadline, p.notice_timer_armed};
    if (bk_timer_poll(&timer, now))
      p.notice_visible = 0;
    p.notice_timer_armed = timer.armed;
    next.deadline = timer.deadline;
    frame.bind_message = 1;
    f.started = 0;
    f.enabled = 1;
  }
  if (!bk_fade_sprite_step(&next.panel, p.notice_visible, seconds))
    return 0;
  for (unsigned i = 0; i < BK_ITEM_TYPES; ++i)
    if (!bk_fade_sprite_step(
            &next.icons[i],
            p.selected_item == (int32_t)i ? p.notice_visible : 0, seconds))
      return 0;
  frame.draw_text = next.panel.stage == 2 || next.panel.stage == 3;
  *s = next;
  *pickup = p;
  *flow = f;
  *out = frame;
  return 1;
}
int bk_opening_notice_step(BkItemNoticeState *s, BkPulseSprite *prompt,
                           uint8_t phase, uint8_t visible, float seconds,
                           BkItemNoticeFrame *out) {
  if (!s || !prompt || !out || !isfinite(seconds) || seconds < 0 ||
      (phase != 0 && phase != 2 && phase != 3))
    return 0;
  BkItemNoticeState n = *s;
  BkPulseSprite p = *prompt;
  BkItemNoticeFrame frame = {0};
  if (phase != 3 && !bk_fade_sprite_step(&n.panel, visible, seconds))
    return 0;
  if (phase == 0) {
    if (!bk_pulse_sprite_step(&p, visible, seconds))
      return 0;
    frame.draw_text = n.panel.stage == 2 || n.panel.stage == 3;
  }
  *s = n;
  *prompt = p;
  *out = frame;
  return 1;
}
