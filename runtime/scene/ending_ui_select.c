#include "scene/ending_ui_select.h"
#include <math.h>
#include <stdio.h>
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending UI select: %s", why);
  return 0;
}
static int valid(BkEndingUi *ui, BkEndingStageUi *stage,
                 const BkEndingUiSelectBindings *b, const float *p, float dt,
                 int32_t *selected, BkEndingUiFrame *out, char e[256]) {
  return (ui && stage && b && b->frame && b->auxiliary && b->open && p &&
          selected && out && out->count <= BK_ENDING_UI_DRAWS &&
          isfinite(p[0]) && isfinite(p[1]) && isfinite(dt) && dt >= 0) ||
         fail(e, "invalid state/input");
}
static int position(BkEndingUi *ui, const BkEndingUiSelectBindings *b,
                    char e[256]) {
  int32_t target = b->frame->camera_cached;
  if (!b->targets || target < 0 || (size_t)target >= b->target_count)
    return fail(e, "target outside live projected table");
  ui->sprites[50].rect[0] = (float)b->targets[target][0];
  ui->sprites[50].rect[1] = (float)b->targets[target][1];
  return 1;
}
static int ring(BkEndingUi *ui, BkEndingStageUi *stage,
                const BkEndingUiSelectBindings *b, float dt,
                BkEndingUiFrame *out, char e[256]) {
  return *b->open != 0 ||
         bk_ending_ui_dispatch_sprite(ui, stage, 50, dt, out, e);
}
static int fallback(BkEndingUi *ui, const BkEndingUiSelectBindings *b,
                    const float p[2], int32_t *selected, char e[256]) {
  if (!b->pick)
    return fail(e, "missing actual target geometry");
  BkEndingUiPickBindings pick = *b->pick;
  pick.ring_width = ui->sprites[50].rect[2];
  float distance = 10000;
  int32_t hit;
  if (!bk_ending_ui_pick_available_targets(&pick, p, &distance, &hit, e))
    return 0;
  b->frame->camera_request = 1;
  *selected = hit == -1 ? 0 : 3;
  return 1;
}
int bk_ending_ui_select_normal(BkEndingUi *ui, BkEndingStageUi *stage,
                               const BkEndingUiSelectBindings *b,
                               const float p[2], float dt, int32_t *selected,
                               BkEndingUiFrame *out, char e[256]) {
  if (!valid(ui, stage, b, p, dt, selected, out, e))
    return 0;
  BkEndingFrameState *f = b->frame;
  if (f->camera_cached == -1)
    return fallback(ui, b, p, selected, e);
  if (!b->actions)
    return fail(e, "missing live action table");
  for (unsigned row = 0; row < 16; row++) {
    if (f->camera_cached != b->actions[row * 5])
      continue;
    int outside, category;
    int special = f->group == 3 || f->group == 4;
    if (!bk_ending_ui_camera_sector(b->camera_local, f->camera_cached,
                                    special ? 120 : 200, special ? 240 : 320,
                                    &outside, &category))
      return fail(e, "invalid actual camera local");
    if (category != 2 && category != 3)
      break;
    if (row == 14)
      return fail(e, "undefined native row14 selection");
    int32_t result;
    int finish = 0;
    if (row < 5)
      result = 4;
    else if (row < 10)
      result = 5;
    else if (row < 14)
      result = 6;
    else {
      if (!isfinite(b->auxiliary->progress))
        return fail(e, "invalid progress");
      finish = b->auxiliary->progress >= .39f && f->camera_cached == 6;
      result = finish ? 7 : 3;
    }
    if (!position(ui, b, e))
      return 0;
    if (f->state_721ee0 == 1) {
      if ((row != 15 || finish) && !ring(ui, stage, b, dt, out, e))
        return 0;
      f->camera_request = 0;
    }
    *selected = result;
    return 1;
  }
  f->camera_request = 1;
  *selected = 3;
  return 1;
}
int bk_ending_ui_select_third(BkEndingUi *ui, BkEndingStageUi *stage,
                              const BkEndingUiSelectBindings *b,
                              const float p[2], float dt, int32_t *selected,
                              BkEndingUiFrame *out, char e[256]) {
  if (!valid(ui, stage, b, p, dt, selected, out, e))
    return 0;
  if (!b->active_clip || !b->stage3_state)
    return fail(e, "missing stage3/clip");
  BkEndingFrameState *f = b->frame;
  int32_t clip = *b->active_clip;
  int low = 140, high = 220;
  if (f->group == 1) {
    if (clip > 3) {
      low = 359;
      high = 0;
    }
  } else if (f->group == 2 || f->group == 3 || f->group == 4) {
    low = 135;
    high = 265;
  }
  if (clip != 6 && clip != 8 && clip != 10) {
    int outside, category;
    if (!bk_ending_ui_camera_sector(b->camera_local, f->camera_cached, low,
                                    high, &outside, &category))
      return fail(e, "invalid actual camera local");
    if (outside) {
      if (clip == 1 || clip == 2) {
        static const int32_t target[5] = {17, 32, 17, 17, 32};
        if (f->group >= 5)
          return fail(e, "group outside native target table");
        if (f->camera_cached == target[f->group]) {
          if (*b->stage3_state == 1) {
            if (!position(ui, b, e) || !ring(ui, stage, b, dt, out, e))
              return 0;
            f->camera_request = 0;
          }
          *selected = 4;
          return 1;
        }
      } else {
        if (!b->actions)
          return fail(e, "missing live action table");
        if (f->camera_cached == b->actions[5] ||
            f->camera_cached == b->actions[10] ||
            f->camera_cached == b->actions[15]) {
          if (*b->stage3_state == 1) {
            if (!position(ui, b, e))
              return 0;
            int allowed = f->camera_cached == b->actions[5];
            if (!allowed) {
              if (!b->unavailable)
                return fail(e, "missing availability");
              allowed = (f->camera_cached == b->actions[10] &&
                         b->unavailable[0] == 0) ||
                        (f->camera_cached == b->actions[15] &&
                         b->unavailable[1] == 0);
            }
            if (!allowed) {
              f->camera_request = 1;
              *selected = 3;
              return 1;
            }
            if (!ring(ui, stage, b, dt, out, e))
              return 0;
            f->camera_request = 0;
          }
          *selected = 4;
          return 1;
        }
        if (f->camera_cached == 6) {
          if (!isfinite(b->auxiliary->progress))
            return fail(e, "invalid progress");
          if (b->auxiliary->progress >= .39f) {
            if (!position(ui, b, e))
              return 0;
            if (*b->stage3_state == 1) {
              if (!ring(ui, stage, b, dt, out, e))
                return 0;
              f->camera_request = 0;
            }
            if (!b->item)
              return fail(e, "missing inventory byte");
            *selected = *b->item == 1 ? 8 : 7;
            return 1;
          }
        }
      }
    }
    if (f->camera_cached != -1) {
      f->camera_request = 1;
      *selected = 3;
      return 1;
    }
  }
  if (*b->stage3_state == 1)
    return fallback(ui, b, p, selected, e);
  f->camera_request = 1;
  *selected = 0;
  return 1;
}
static int keys(const BkEndingUiSelectOps *ops, int32_t *selected,
                char e[256]) {
  if (!ops || !ops->key)
    return fail(e, "missing held-key service");
  uint32_t value;
  if (!ops->key(ops->context, 0, 2, &value, e))
    return 0;
  if (value & 255) {
    *selected = 1;
    return 1;
  }
  if (!ops->key(ops->context, 1, 2, &value, e))
    return 0;
  if (value & 255)
    *selected = 2;
  return 1;
}
int bk_ending_ui_select(BkEndingUi *ui, BkEndingStageUi *stage,
                        const BkEndingUiSelectBindings *b, const float p[2],
                        float dt, uint8_t visible,
                        const BkEndingUiSelectOps *ops, int32_t *selected,
                        BkEndingUiFrame *out, char e[256]) {
  if (!valid(ui, stage, b, p, dt, selected, out, e) || !b->control)
    return fail(e, "invalid selector binding");
  BkEndingFrameState *f = b->frame;
  int32_t result = 0;
  int phase = f->phase;
  if (phase == 1 || phase == 3) {
    if (phase == 3) {
      if (!bk_ending_ui_select_third(ui, stage, b, p, dt, &result, out, e))
        return 0;
    } else if (f->state_721ee0 != 4) {
      if (!bk_ending_ui_select_normal(ui, stage, b, p, dt, &result, out, e))
        return 0;
    }
    if ((result == 0 || result == 3) && !keys(ops, &result, e))
      return 0;
  } else if (phase == 2 || phase == 4 || phase == 5 || phase == 6) {
    int gate = phase == 2   ? f->state_721ee4
               : phase == 4 ? b->control->state_721eec
                            : b->auxiliary->gate;
    int blocked = gate == 3 || ((phase == 5 || phase == 6) && gate == 5);
    if (!blocked) {
      if (!ops || !ops->voice_playing)
        return fail(e, "missing speech1 status service");
      if (!ops->voice_playing(ops->context, &blocked, e))
        return 0;
    }
    int enabled =
        !blocked &&
        ((f->camera_event == 1 && f->camera_cached == (phase == 2 ? 0 : 4)) ||
         (phase != 4 && f->camera_event == 2));
    if (enabled && !ring(ui, stage, b, dt, out, e))
      return 0;
    f->camera_request = enabled ? 0 : 1;
    if (!keys(ops, &result, e))
      return 0;
    /* Gates are re-read after callbacks, as in the original caller. */
    if (phase == 2) {
      if (f->state_721ee4 != 2)
        result = 0;
    } else if (phase == 4) {
      if (b->control->state_721eec != 2 && b->control->state_721eec != 4)
        result = 0;
    } else if (b->auxiliary->gate != 2)
      result = 0;
  } else if (phase == 8) {
    if (!keys(ops, &result, e))
      return 0;
  } else if (visible != 1 && f->phase != 9)
    return fail(e, "undefined native phase selection");
  if (visible == 1 || f->phase == 9)
    result = 0;
  *selected = result;
  return 1;
}
