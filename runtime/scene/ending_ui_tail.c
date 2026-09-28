#include "scene/ending_ui_tail.h"
#include "core/random.h"
#include "game/ending_sound.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending UI tail: %s", why);
  return 0;
}
static int32_t increment(int32_t x) {
  return x == INT32_MAX ? INT32_MIN : x + 1;
}
static int signed_byte(uint8_t v) { return v < 128 ? v : (int)v - 256; }
static void uv(BkEndingUi *ui, unsigned slot, float left, float right,
               float bottom) {
  float *v = ui->sprites[slot].uv;
  v[0] = left;
  v[1] = 0;
  v[2] = right;
  v[3] = bottom;
}
static void speed(BkEndingUi *ui, unsigned first, float value) {
  ui->sprites[first].transform.fade.speed = value;
  ui->sprites[first + 1].transform.fade.speed = value;
}
static int image(const BkEndingUi *ui, unsigned slot, char e[256]) {
  return ((ui->loaded >> slot) & 1) || fail(e, "missing required sprite image");
}
static int expression(const BkEndingUiTailBindings *b,
                      const BkEndingUiTailOps *ops, int a, int c, char e[256]) {
  b->auxiliary->expression_a = a;
  b->auxiliary->expression_b = c;
  return (ops && ops->actor.eyes &&
          ops->actor.eyes(ops->actor.context, 1, e)) ||
         fail(e, "eye update failed or missing");
}
static int audio(const BkEndingUiTailBindings *b, const BkEndingUiTailOps *ops,
                 BkEndingAudioOperation op, unsigned slot, int cue,
                 int *playing, char e[256]) {
  if (!ops || !ops->actor.audio ||
      (op != BK_ENDING_AUDIO_STATUS && !b->voice_volume))
    return fail(e, "missing audio service/volume");
  BkEndingAudioCall call = {
      .operation = op,
      .slot = slot,
      .cue = cue,
      .volume = op == BK_ENDING_AUDIO_STATUS ? 0 : *b->voice_volume};
  return ops->actor.audio(ops->actor.context, &call, playing, e);
}
static int speech(const BkEndingUiTailBindings *b, const BkEndingUiTailOps *ops,
                  unsigned slot, const char *name, char e[256]) {
  if (!b->speech_names)
    return fail(e, "missing speech name owner");
  strcpy(b->speech_names[slot], name);
  if (!ops || !ops->speech || !b->voice_volume)
    return fail(e, "missing speech service/volume");
  return ops->speech(ops->actor.context, slot, name, *b->voice_volume, e);
}
int bk_ending_ui_curtain(BkCommonHudState *s, float dt, BkCommonHudFrame *out,
                         char e[256]) {
  if (!s || !out || !bk_fade_sprite_advance(&s->curtain, dt))
    return fail(e, "invalid shared curtain");
  out->curtain_alpha = s->curtain.alpha;
  return bk_fade_sprite_request(&s->curtain, s->blocked);
}
static int flash(BkEndingUi *ui, BkEndingStageUi *stage,
                 const BkEndingUiTailBindings *b, int request_first, float dt,
                 BkEndingUiFrame *out, char e[256]) {
  if (!b->flash_wanted || !image(ui, 52, e))
    return fail(e, "missing flash owner/image");
  uv(ui, 52, .5f, .75f, 1);
  BkFadeSprite *f = &ui->sprites[52].transform.fade;
  if (request_first && !bk_fade_sprite_request(f, *b->flash_wanted))
    return fail(e, "invalid flash request");
  if (!bk_ending_ui_dispatch_sprite(ui, stage, 52, dt, out, e))
    return 0;
  if (!request_first && !bk_fade_sprite_request(f, *b->flash_wanted))
    return fail(e, "invalid flash request");
  f->speed = 5;
  if (*b->flash_wanted == 1 && f->stage == 3)
    *b->flash_wanted = 0;
  return 1;
}
static int aux_index(const BkEndingUiTailBindings *b, BkEndingUiAuxNotice *s,
                     int *enabled, char e[256]) {
  if (!b->aux_inputs || s->choice < 0 || s->choice > 1)
    return fail(e, "invalid auxiliary choice");
  *enabled = 0;
  if (!b->aux_inputs[s->choice] || s->processed[s->choice])
    return 1;
  if (!b->aux_config || b->frame->group >= 5 || b->auxiliary->selection < 0 ||
      b->auxiliary->selection > 2)
    return fail(e, "invalid auxiliary table selection");
  *enabled =
      b->aux_config[b->frame->group][s->choice + 2 * b->auxiliary->selection]
          ? 1
          : -1;
  return 1;
}
static int auxiliary(BkEndingUi *ui, BkEndingStageUi *stage,
                     BkEndingUiAuxNotice *s, const BkEndingUiTailBindings *b,
                     const BkEndingUiTailOps *ops, float dt,
                     BkEndingUiFrame *out, char e[256]) {
  if (!s)
    return fail(e, "missing auxiliary notice state");
  BkEndingUiNoticeState *n = b->notices;
  if (s->mode == 5 || s->mode == 7)
    return flash(ui, stage, b, 0, dt, out, e);
  n->popups[0] = s->mode == 2 && b->auxiliary->gate == 3 && s->choice != 0;
  n->popups[1] = s->mode == 2 && b->auxiliary->gate == 3 && s->choice == 0;
  if (s->mode == 4)
    return 1;
  if (s->mode == 6) {
    if (b->auxiliary->pending != 23) {
      int playing;
      if (!audio(b, ops, BK_ENDING_AUDIO_VOICE, 0, 23, &playing, e))
        return 0;
      b->auxiliary->pending = 23;
      if (!expression(b, ops, 6, 6, e))
        return 0;
      s->sequence_elapsed = 0;
      s->sequence = 0;
    }
    if (!s->once) {
      n->notices[0] = n->notices[1] = 1;
      speed(ui, 53, 1);
      s->once = 1;
    }
    if (n->notices[0] == 1 && ui->sprites[53].transform.fade.stage == 3) {
      if (!image(ui, 53, e) || !image(ui, 54, e))
        return 0;
      float v = (float)signed_byte(s->sequence);
      for (unsigned i = 53; i <= 54; i++) {
        uv(ui, i, 0, v, v);
        ui->sprites[i].transform.scale[0] = ui->sprites[i].transform.scale[1] =
            v;
      }
      s->sequence_elapsed = (float)((double)s->sequence_elapsed + dt);
      if (s->sequence_count != 4 && s->sequence_elapsed > 0) {
        s->sequence++;
        s->sequence_count++;
        s->sequence_elapsed = 0;
      }
      if (s->sequence_count == 4) {
        int32_t clip;
        if (!ops || !ops->actor.active || !ops->actor.write ||
            !ops->actor.active(ops->actor.context, &clip, e))
          return fail(e, "missing primary clip services");
        unsigned request = clip == 6 || clip == 7 || clip == 14     ? 15
                           : clip == 10 || clip == 11 || clip == 13 ? 16
                                                                    : 0;
        if (request && (!ops->actor.request ||
                        !ops->actor.request(ops->actor.context, request, e)))
          return fail(e, "clip selection failed or missing");
        /* +b24/+bc0 are the chain fields of clips15/16 (+200+slot*9c). */
        if (!ops->actor.write(ops->actor.context, 15, BK_ENDING_CLIP_CHAIN, 1,
                              e) ||
            !ops->actor.write(ops->actor.context, 16, BK_ENDING_CLIP_CHAIN, 1,
                              e))
          return 0;
        n->notices[0] = n->notices[1] = 0;
        speed(ui, 53, .5f);
        s->mode = 4;
        s->once = 0;
        s->reset_a = 0;
        s->sequence_count = 0;
        s->sequence_elapsed = 0;
        s->sequence = 0;
        s->reset_b = 0;
        s->reset_c = 0;
      }
    }
  }
  if (s->mode == 6)
    return 1;
  int enabled;
  if (!aux_index(b, s, &enabled, e))
    return 0;
  if (enabled == 1) {
    if (!image(ui, 53, e) || !image(ui, 54, e))
      return 0;
    uv(ui, 53, 0, 1, 1);
    uv(ui, 54, 0, 1, 1);
    n->notices[0] = 1;
    ui->sprites[53].transform.fade.stage = 0;
    if (!bk_ending_ui_dispatch_sprite(ui, stage, 53, dt, out, e))
      return 0;
    n->notices[1] = 1;
    s->processed[s->choice] = 1;
    speed(ui, 53, 1);
  }
  if (n->notices[0] == 1 && ui->sprites[53].transform.fade.stage == 3) {
    if (!image(ui, 52, e))
      return 0;
    uv(ui, 52, 0, .25f, 1);
    s->elapsed = (float)((double)s->elapsed + dt);
    if (s->elapsed > 0) {
      s->cycles = increment(s->cycles);
      s->elapsed = 0;
    }
    if (s->cycles == 4) {
      n->notices[0] = n->notices[1] = 0;
      speed(ui, 53, .5f);
      int cue = 14 + s->choice * 6;
      float p = b->auxiliary->progress;
      if (!isfinite(p) || b->frame->group >= 5)
        return fail(e, "invalid auxiliary progress/group");
      cue += p < .6f ? 0 : p < .8f ? 1 : 2;
      if (!s->group_seen[b->frame->group]) {
        s->random_latch = 1;
        s->group_seen[b->frame->group] = 1;
      } else {
        if (!b->random)
          return fail(e, "missing shared random state");
        s->random_latch = bk_random_next(b->random) % 1000 <= 25;
      }
      if (b->control->toggles[7] && s->random_latch) {
        int playing;
        if (!audio(b, ops, BK_ENDING_AUDIO_VOICE, 1, cue, &playing, e))
          return 0;
      }
      s->cycles = 0;
      s->elapsed = 0;
    }
    ui->sprites[52].transform.fade.alpha = .01f;
    if (!bk_ending_ui_dispatch_sprite(ui, stage, 52, dt, out, e))
      return 0;
  }
  if (!aux_index(b, s, &enabled, e))
    return 0;
  if (enabled == -1) {
    n->notices[2] = 1;
    ui->sprites[55].transform.fade.stage = 0;
    if (!bk_ending_ui_dispatch_sprite(ui, stage, 55, dt, out, e))
      return 0;
    n->notices[3] = 1;
    speed(ui, 55, .5f);
    s->processed[s->choice] = 1;
  }
  if (n->notices[3] == 1 && ui->sprites[56].transform.fade.stage == 3) {
    speed(ui, 55, .5f);
    n->notices[2] = n->notices[3] = 0;
  }
  return 1;
}
static int normal_index(const BkEndingUiTailBindings *b, int *index,
                        int *enabled, char e[256]) {
  static const uint8_t config[5][14] = {
      {0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0},
      {0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1},
      {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0},
      {0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1},
      {0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0}};
  if (!b->normal_side || !b->normal_target || !b->normal_inputs ||
      !b->normal_processed || b->frame->group >= 5)
    return fail(e, "missing normal table bindings/group");
  int64_t i = *b->normal_side + (int64_t)*b->normal_target * 2;
  if (i < 0 || i >= 14)
    return fail(e, "normal table index out of bounds");
  *index = (int)i;
  *enabled = config[b->frame->group][i];
  return 1;
}
static int progress(const BkEndingUiTailBindings *b, float amount, float pixels,
                    float scale, char e[256]) {
  if (!b->gauge_y || !isfinite(*b->gauge_y) ||
      !isfinite(b->auxiliary->progress))
    return fail(e, "invalid gauge/progress");
  b->auxiliary->progress = (float)((double)b->auxiliary->progress + amount);
  *b->gauge_y = (float)((double)*b->gauge_y - (double)pixels * scale);
  if (b->auxiliary->progress > 1)
    b->auxiliary->progress = 1;
  return 1;
}
int bk_ending_ui_tail(BkEndingUi *ui, BkEndingStageUi *stage,
                      BkEndingUiNormalNotice *s, BkEndingUiAuxNotice *as,
                      const BkEndingUiTailBindings *b,
                      const BkEndingUiTailOps *ops, float scale, float dt,
                      BkEndingUiFrame *out, char e[256]) {
  if (!ui || !stage || !b || !b->frame || !b->control || !b->auxiliary ||
      !b->notices || !out || out->count > BK_ENDING_UI_DRAWS ||
      !isfinite(scale) || scale <= 0 || !isfinite(dt) || dt < 0)
    return fail(e, "invalid input/bindings");
  if ((b->frame->phase == 5 || b->frame->phase == 6) && b->auxiliary->gate != 5)
    if (!auxiliary(ui, stage, as, b, ops, dt, out, e))
      return 0;
  if (b->frame->phase == 8) {
    if (!b->final_state)
      return fail(e, "missing final state");
    if (*b->final_state == 9 && ((ui->loaded >> 52) & 1))
      if (!flash(ui, stage, b, 1, dt, out, e))
        return 0;
  }
  if (b->frame->phase != 1)
    return 1;
  if (!s || !isfinite(s->elapsed))
    return fail(e, "invalid normal notice state");
  int i, enabled;
  BkEndingUiNoticeState *n = b->notices;
  if (!normal_index(b, &i, &enabled, e))
    return 0;
  if (enabled && !b->normal_processed[i] && b->normal_inputs[i]) {
    if (ui->sprites[55].transform.fade.stage != 0) {
      ui->sprites[55].transform.fade.stage =
          ui->sprites[56].transform.fade.stage = 0;
      n->notices[2] = n->notices[3] = 0;
    }
    n->notices[0] = n->notices[1] = 1;
    b->normal_processed[i] = 1;
    if (!progress(b, .1f, 20, scale, e))
      return 0;
    speed(ui, 53, 1);
  }
  if (n->notices[0] == 1 && ui->sprites[53].transform.fade.stage == 3) {
    if (!image(ui, 52, e))
      return 0;
    s->frame %= 4;
    s->uv_right = (float)((double)(s->frame + 1) * .25f);
    uv(ui, 52, (float)((double)s->uv_right - .25f), s->uv_right, 1);
    s->elapsed = (float)((double)s->elapsed + dt);
    if (s->elapsed > 0) {
      s->frame = increment(s->frame);
      s->elapsed = 0;
    }
    if (s->frame == 1) {
      s->frame = 0;
      s->cycles = increment(s->cycles);
    }
    if (s->cycles == 4) {
      n->notices[0] = n->notices[1] = 0;
      speed(ui, 53, .5f);
      if (b->auxiliary->pending != 6) {
        char name[32];
        if (!bk_ending_sound_normal_voice(
                b->frame->group, b->frame->camera_cached, 2, name, e) ||
            !speech(b, ops, 0, name, e))
          return 0;
        b->auxiliary->pending = 6;
      }
      int target = b->frame->camera_cached;
      if (target == 11 || target == 12 || target == 10) {
        if (!expression(b, ops, 9, 3, e))
          return 0;
      } else if (target == 9) {
        if (!expression(b, ops, 0, 4, e))
          return 0;
      }
      s->cycles = s->frame = 0;
    }
    ui->sprites[52].transform.fade.alpha = .01f;
    if (!bk_ending_ui_dispatch_sprite(ui, stage, 52, dt, out, e))
      return 0;
  }
  /* Re-read group/side/target after speech and eye callbacks. */
  if (!normal_index(b, &i, &enabled, e))
    return 0;
  if (!enabled && !b->normal_processed[i] && b->normal_inputs[i]) {
    if (ui->sprites[53].transform.fade.stage != 0) {
      n->notices[0] = n->notices[1] = 0;
      ui->sprites[53].transform.fade.stage =
          ui->sprites[54].transform.fade.stage = 0;
    }
    n->notices[2] = n->notices[3] = 1;
    speed(ui, 55, .5f);
    b->normal_processed[i] = 1;
    if (!progress(b, .04f, 8, scale, e))
      return 0;
  }
  if (n->notices[2] == 1 && ui->sprites[55].transform.fade.stage == 3) {
    speed(ui, 55, .5f);
    n->notices[2] = n->notices[3] = 0;
    int playing;
    if (!audio(b, ops, BK_ENDING_AUDIO_STATUS, 1, 0, &playing, e))
      return 0;
    if (!playing && b->control->toggles[7]) {
      char name[32];
      if (!b->contact_index)
        return fail(e, "missing contact voice index");
      if (!bk_ending_sound_contact_voice(b->frame->group, 1, *b->contact_index,
                                         0, name, e) ||
          !speech(b, ops, 1, name, e))
        return 0;
    }
  }
  return 1;
}
