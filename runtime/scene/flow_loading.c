#include "scene/flow_loading.h"
#include <stdio.h>
const char *bk_flow_loading_image(BkFlowLoadingAsset asset) {
  static const char *const names[] = {"ma_03.bmp", "te_01.bmp", "ma_01.tga",
                                      "za_00.bmp", "za_01.bmp"};
  return (unsigned)asset < 5 ? names[asset] : NULL;
}
int bk_flow_loading_layout(float out[4], BkFlowLoadingAsset asset,
                           unsigned width, unsigned height) {
  if (!out || (unsigned)asset >= 5 || !width || !height || width > 16384 ||
      height > 16384)
    return 0;
  if (asset < BK_LOADING_PROMPT) {
    float scale = (float)((double)width / 1280);
    out[0] = out[1] = 0;
    out[2] = (float)(1280.0 * scale);
    out[3] = (float)(960.0 * scale);
  } else {
    float x = (float)((double)width / 1024);
    float y = (float)((double)height / 768);
    out[0] = (float)(976.0 * x);
    out[1] = (float)(720.0 * y);
    out[2] = (float)(48.0 * x);
    out[3] = (float)(48.0 * y);
  }
  return 1;
}
void bk_flow_loading_initialize(BkFlowLoadingState *s, uint8_t special) {
  if (!s)
    return;
  bk_fade_sprite_initialize(&s->background);
  bk_fade_sprite_request(&s->background, 1);
  if (special == 1) {
    bk_fade_sprite_initialize(&s->special_background);
    bk_fade_sprite_request(&s->special_background, 1);
  }
  bk_fade_sprite_initialize(&s->prompt_base);
  bk_fade_sprite_request(&s->prompt_base, 1);
  bk_pulse_sprite_initialize(&s->prompt);
}
static int draw(BkFlowLoadingFrame *f, BkFadeSprite *s,
                BkFlowLoadingAsset asset, float seconds) {
  if (!bk_fade_sprite_advance(s, seconds))
    return 0;
  f->draws[f->count++] = (BkFlowLoadingDraw){asset, s->alpha};
  return 1;
}
int bk_flow_loading_step(BkFlowLoadingState *s, BkCommonHudState *common,
                         BkFlowTransition *flow, uint8_t special, int advance,
                         float seconds, const BkFlowLoadingOps *ops,
                         BkFlowLoadingFrame *frame, char error[256]) {
  if (!s || !common || !flow || flow->current != 0x50 || !ops || !ops->load ||
      !ops->confirm || !frame || (advance != 0 && advance != 1))
    goto invalid;
  *frame = (BkFlowLoadingFrame){0};
  if (flow->mode == 1) {
    int alternate = special == 1 && flow->previous == 0x38;
    if (!draw(frame, alternate ? &s->special_background : &s->background,
              alternate ? BK_LOADING_SPECIAL : BK_LOADING_BACKGROUND, seconds))
      goto invalid;
  }
  if (!draw(frame, &common->curtain, BK_LOADING_CURTAIN, seconds))
    goto invalid;
  if (flow->mode == 1) {
    if (!bk_fade_sprite_request(&common->curtain, common->blocked))
      goto invalid;
    if (special == 1 && flow->previous == 0x38) {
      if (s->awaiting == 1) {
        if (advance) {
          if (!ops->confirm(ops->context, error))
            return 0;
          common->blocked = 1;
          s->awaiting = 0;
        }
        if (!draw(frame, &s->prompt_base, BK_LOADING_PROMPT, seconds) ||
            !bk_pulse_sprite_step(&s->prompt, 2, seconds))
          goto invalid;
        /* wanted2 performs no request in50e633, so this is advance only. */
        frame->draws[frame->count++] =
            (BkFlowLoadingDraw){BK_LOADING_PULSE, s->prompt.fade.alpha};
      }
      if (common->blocked == 0 && common->curtain.stage == 0 &&
          s->awaiting == 0) {
        if (!ops->load(ops->context, flow->target, error))
          return 0;
        s->awaiting = 1;
      } else if (common->blocked == 1 && common->curtain.stage == 3) {
        common->blocked = 0;
        flow->current = flow->target;
      }
    } else if (common->blocked == 0 && common->curtain.stage == 0) {
      common->blocked = 1;
      if (!ops->load(ops->context, flow->target, error))
        return 0;
    } else if (common->blocked == 1 && common->curtain.stage == 3) {
      common->blocked = 0;
      flow->current = flow->target;
    }
  } else if (flow->mode == 0 || flow->mode == 2) {
    common->blocked = 0;
    if (!ops->load(ops->context, flow->target, error))
      return 0;
    flow->current = flow->target;
  } else if (flow->mode == 3) {
    common->blocked = 0;
    flow->current = flow->target;
  }
  return 1;
invalid:
  snprintf(error, 256, "flow loading: invalid state/services/time");
  return 0;
}
