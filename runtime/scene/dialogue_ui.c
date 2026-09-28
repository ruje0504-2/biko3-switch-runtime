#include "scene/dialogue_ui.h"
#include <math.h>
#include <stdio.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "dialogue UI: %s", why);
  return 0;
}
static int valid_grid(unsigned columns, unsigned rows, unsigned step) {
  return columns && columns <= 640 && rows && rows <= 480 && step &&
         step <= 480;
}
int bk_dialogue_ui_layout(BkDialogueUiSprite out[2], unsigned width) {
  if (!out || !width || width > 16384)
    return 0;
  float scale = (float)((double)width / 1280);
  out[0] = (BkDialogueUiSprite){"ma_04.tga", (float)(138. * scale),
                                (float)(727. * scale), (float)(1010. * scale),
                                (float)(208. * scale)};
  out[1] = (BkDialogueUiSprite){"ma_05.tga", (float)(1097. * scale),
                                (float)(890. * scale), (float)(40. * scale),
                                (float)(40. * scale)};
  return 1;
}
int bk_dialogue_ui_text_style(BkTextStyle *out) {
  if (!out)
    return 0;
  *out = (BkTextStyle){104, 380, 432, 64, 16, 16, 1, 1, {1, 1, 1}, 1};
  return 1;
}
void bk_dialogue_ui_initialize(BkDialogueUi *s) {
  if (!s)
    return;
  bk_fade_sprite_initialize(&s->panel);
  bk_fade_sprite_request(&s->panel, 1);
  bk_pulse_sprite_initialize(&s->prompt);
  s->columns = 27;
  s->rows = 4;
  s->step_y = 16;
}
int bk_dialogue_text_target(const BkMessage *m, unsigned columns, unsigned rows,
                            unsigned step, int initial, float *out,
                            char e[256]) {
  if (!m || !out || m->length > BK_MESSAGE_CAPACITY ||
      !valid_grid(columns, rows, step) || (initial != 0 && initial != 1))
    return fail(e, "invalid message/grid");
  int32_t count = 0, line = 0;
  for (size_t i = 0; i < m->length && m->bytes[i]; ++i) {
    if (!initial && i % (columns * 2) == 0)
      ++line;
    if (!initial && m->bytes[i] == '\r')
      count += (line + 1) * (int32_t)(columns * 2) - (int32_t)i;
    else
      ++count;
  }
  int32_t chars = (count - 1) / 2;
  float target = 0;
  if (chars > (int32_t)(columns * rows)) {
    int32_t lines = chars / (int32_t)columns;
    if (chars % (int32_t)columns)
      ++lines;
    target = (float)((lines - (int32_t)rows) * (int32_t)(step * 2));
  }
  *out = target;
  return 1;
}
int bk_dialogue_ui_step(BkDialogueUi *s, const BkDialogueUiBindings *b,
                        const BkDialogueUiInput *in, const BkDialogueUiOps *ops,
                        BkDialogueUiFrame *out, char e[256]) {
  if (!s || !b || !in || !ops || !out || !b->dialogue || !b->text ||
      !b->backdrop || !b->phase || !b->curtain || !b->curtain_wanted ||
      !b->result || !ops->pause_voice || !ops->next || !ops->bind ||
      !ops->text || !ops->speech || !ops->music || !ops->unlock ||
      !ops->schedule || (in->advance != 0 && in->advance != 1) ||
      (in->voice_present != 0 && in->voice_present != 1) || in->group >= 5 ||
      b->dialogue->text.length > BK_MESSAGE_CAPACITY ||
      !valid_grid(s->columns, s->rows, s->step_y) ||
      !isfinite(b->text->delay) || !isfinite(b->text->scroll) ||
      !isfinite(b->text->target))
    return fail(e, "invalid state/services/input");
  BkFadeSprite panel = s->panel, curtain = *b->curtain;
  BkPulseSprite prompt = s->prompt;
  if (!bk_fade_sprite_advance(&panel, in->seconds) ||
      !bk_pulse_sprite_advance(&prompt, in->seconds) ||
      !bk_fade_sprite_advance(&curtain, in->seconds))
    return fail(e, "invalid fade/time/text");
  s->text_parameter = 1000;
  s->panel = panel;
  s->prompt = prompt;
  out->panel_alpha = panel.alpha;
  out->prompt_alpha = prompt.fade.alpha;
  if (in->advance && *b->phase == 0) {
    if (in->voice_present && !ops->pause_voice(ops->context, e))
      return 0;
    if (b->text->scroll != b->text->target) {
      b->text->scroll = b->text->target;
      b->text->started = 0;
      b->text->enabled = 1;
    } else {
      int done;
      if (!ops->next(ops->context, &done, e))
        return 0;
      if (done)
        *b->curtain_wanted = 1;
      if (!ops->bind(ops->context, e) ||
          !bk_dialogue_text_target(&b->dialogue->text, s->columns, s->rows,
                                   s->step_y, 0, &b->text->target, e))
        return 0;
      b->text->enabled = 1;
      b->text->delay = 2;
      b->text->scroll = 0;
      b->text->started = 0;
    }
    if (b->dialogue->image_kind == 1 || b->dialogue->image_kind == 2) {
      *b->phase = 1;
      b->backdrop->image_kind = b->dialogue->image_kind - 1;
      b->dialogue->image_kind = 0;
      b->backdrop->saved_expression = b->dialogue->code_f;
    }
  }
  if (!ops->text(ops->context, in->seconds, e) ||
      !ops->speech(ops->context, e) || !ops->music(ops->context, e))
    return 0;
  bk_fade_sprite_advance(b->curtain, in->seconds);
  out->curtain_alpha = b->curtain->alpha;
  bk_fade_sprite_request(b->curtain, *b->curtain_wanted);
  if (*b->curtain_wanted == 1 && b->curtain->stage == 3) {
    *b->curtain_wanted = 0;
    uint8_t target = 1;
    if (in->previous == 0x38)
      target = 2;
    else if (in->previous == 2) {
      b->result->group = (int32_t)in->group;
      if (in->area < 8)
        b->result->kind = 1;
      else if (in->response == 4)
        b->result->kind = 0;
      else if (in->response == 3)
        b->result->kind = 1;
      b->result->choice = 0;
      target = 0x10;
    } else if (in->previous == 0x10 && !ops->unlock(ops->context, in->group, e))
      return 0;
    if (!ops->schedule(ops->context, target, 1, e))
      return 0;
  }
  return 1;
}
