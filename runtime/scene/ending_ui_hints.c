#include "scene/ending_ui_hints.h"
#include <math.h>
#include <stdio.h>
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending UI hints: %s", why);
  return 0;
}
int bk_ending_ui_hints_start(BkEndingUiHintsState *s, float scale,
                             char e[256]) {
  if (!s || !isfinite(scale) || scale <= 0 || scale > 16)
    return fail(e, "invalid initial scale/state");
  if (!(s->once_flags & 1)) {
    s->once_flags |= 1;
    s->length = (float)(130.0 * scale);
  }
  return 1;
}
static int target(const BkEndingUiHintsBindings *b, const int32_t **out,
                  char e[256]) {
  int32_t index = b->frame->camera_cached;
  if (!b->targets || index < 0 || (size_t)index >= b->target_count)
    return fail(e, "target index outside live table");
  *out = b->targets[index];
  return 1;
}
static int position(BkEndingUi *ui, BkEndingStageUi *stage, unsigned slot,
                    const int32_t point[2], char e[256]) {
  if (!point)
    return fail(e, "missing projected point");
  BkEndingUiSprite *p =
      slot < 63 ? &ui->sprites[slot] : &stage->sprites[slot - 63];
  p->rect[0] = (float)point[0];
  p->rect[1] = (float)point[1];
  return 1;
}
int bk_ending_ui_hints(BkEndingUi *ui, BkEndingStageUi *stage,
                       BkEndingUiHintsState *s,
                       const BkEndingUiHintsBindings *b, const float pointer[2],
                       const float motion[2], float scale, float dt,
                       BkEndingUiFrame *out, char e[256]) {
  if (!ui || !stage || !s || !b || !b->frame || !b->control || !b->auxiliary ||
      !pointer || !out || out->count > BK_ENDING_UI_DRAWS || !isfinite(scale) ||
      scale <= 0 || scale > 16 || !isfinite(dt) || dt < 0 ||
      !isfinite(pointer[0]) || !isfinite(pointer[1]))
    return fail(e, "invalid state/input");
  const BkEndingFrameState *f = b->frame;
  const BkEndingControlState *c = b->control;
  int branch = 0, ready = 0;
  if (c->variant == 1) {
    if (f->state_721ee4 == 3)
      branch = 1;
  } else if (f->phase == 5 || f->phase == 6) {
    if (b->auxiliary->gate == 3)
      branch = 2;
  } else if (f->phase == 3) {
    if (!b->stage3_state)
      return fail(e, "missing stage3 state");
    if (*b->stage3_state == 3) {
      if (!b->active_clip)
        return fail(e, "missing primary clip");
      if (*b->active_clip == 7 || *b->active_clip == 9) {
        if (!motion || !isfinite(motion[0]) || !isfinite(motion[1]) ||
            !isfinite(s->length))
          return fail(e, "invalid movement input");
        float next =
            (float)((fabs((double)motion[0]) + fabs((double)motion[1])) *
                        (double).05f +
                    s->length);
        if (!isfinite(next))
          return fail(e, "movement length overflow");
        float maximum = (float)(400.0 * scale);
        if (next >= maximum) {
          next = maximum;
          ready = 1;
        }
        s->length = next;
      }
      s->movement_ready = ready; /* actual47a026 setter, also when no points */
      branch = 3;
    } else
      s->length = (float)(130.0 * scale);
  } else if (f->phase == 4 && c->state_721eec == 3)
    branch = 4;
#define DRAW(slot)                                                             \
  do {                                                                         \
    if (!bk_ending_ui_dispatch_sprite(ui, stage, (slot), dt, out, e))          \
      return 0;                                                                \
  } while (0)
#define AT(slot, point)                                                        \
  do {                                                                         \
    if (!position(ui, stage, (slot), (point), e))                              \
      return 0;                                                                \
    DRAW(slot);                                                                \
  } while (0)
  if (branch) {
    if (!b->choices || !b->points)
      return fail(e, "missing projected choices");
    int stopped = 0;
    for (unsigned i = 0; i < 3; ++i) {
      int32_t choice = b->choices[i];
      if (choice == -1)
        continue;
      const int32_t *end = NULL;
      if (branch == 1 || branch == 2) {
        if (f->camera_event == 1) {
          if (!target(b, &end, e))
            return 0;
        } else if (f->camera_event == 2) {
          if (branch == 2) {
            if (!b->active_clip)
              return fail(e, "missing primary clip");
            if (*b->active_clip != 2) {
              stopped = 1;
              break;
            }
          }
          if (!b->alternate)
            return fail(e, "missing alternate point");
          end = b->alternate;
        } else if (branch == 2) {
          stopped = 1;
          break;
        }
      } else if (!target(b, &end, e))
        return 0;
      if (end) {
        int variable =
            branch == 3 && (*b->active_clip == 7 || *b->active_clip == 9);
        if (!bk_ending_ui_line(
                stage, b->points[i], end,
                variable ? s->length : (float)(400.0 * scale), dt,
                variable ? &s->variable_scroll : &s->fixed_scroll, e))
          return 0;
      }
      DRAW(63);
      AT(51, b->points[i]);
      float center[2] = {(float)b->points[i][0], (float)b->points[i][1]};
      float radius = (float)((double)ui->sprites[51].rect[2] / 2);
      int hit;
      float distance = 0; /* output only used by the original circle function */
      if (!bk_ending_ui_circle_hit(center, radius, pointer, &hit, &distance))
        return fail(e, "invalid circle geometry");
      int ring_allowed = branch != 3 ||
                         (*b->active_clip != 7 && *b->active_clip != 9) ||
                         ready;
      if (hit && ring_allowed) {
        ui->sprites[50].transform.scale[0] = 1;
        ui->sprites[50].transform.scale[1] = 1;
        AT(50, b->points[i]);
      }
      unsigned label = 75;
      if (branch == 1) {
        static const unsigned slots[] = {67, 64, 65, 66, 68, 69};
        if ((unsigned)choice < 6)
          label = slots[choice];
      } else if (branch == 4) {
        if (choice == 0)
          label = 64;
      } else if ((unsigned)choice < 4)
        label = 64 + (unsigned)choice;
      if (label < 75)
        AT(label, b->points[i]);
    }
    if (!stopped) {
      const int32_t *end = NULL;
      if (branch == 1 || branch == 2) {
        if (f->camera_event == 1 && f->camera_cached == (branch == 1 ? 0 : 4)) {
          if (!target(b, &end, e))
            return 0;
        } else if (f->camera_event == 2) {
          if (!b->alternate)
            return fail(e, "missing alternate point");
          end = b->alternate;
        }
      } else if (branch == 3 || f->camera_cached == 4) {
        if (!target(b, &end, e))
          return 0;
      }
      if (end)
        AT(51, end);
    }
  }
  if (f->phase != 7 && f->phase != 8 && f->phase != 9) {
    if (!b->gauge_y || !isfinite(*b->gauge_y) ||
        !isfinite(b->auxiliary->progress))
      return fail(e, "invalid progress gauge");
    DRAW(b->auxiliary->progress > .99f ? 11 : 10);
    ui->sprites[9].transform.scale[0] = 1;
    ui->sprites[9].transform.scale[1] = b->auxiliary->progress;
    ui->sprites[9].rect[1] = *b->gauge_y;
    if (!bk_fade_sprite_request(&ui->sprites[9].transform.fade, 1))
      return fail(e, "invalid gauge visibility");
    DRAW(9);
  }
  if ((f->phase == 5 || f->phase == 6) && b->auxiliary->gate == 3) {
    if (!b->active_clip)
      return fail(e, "missing primary clip");
    int32_t clip = *b->active_clip;
    if (clip != 1 && clip != 2 && clip != 3 && clip < 20 &&
        f->camera_event != 1) {
      /*4971bd owns a separate once flag and persists across stage reload. */
      double low = 846.0 * scale, high = 1176.0 * scale;
      if (!(s->meter_once_flags & 1)) {
        s->meter_once_flags |= 1;
        s->meter_x = (float)low;
      }
      DRAW(70);
      if (clip == 6 || clip == 7 || clip == 10 || clip == 11 || clip == 17 ||
          clip == 18) {
        if (!b->active_timing || !isfinite(b->active_timing->source) ||
            !isfinite(b->active_timing->start))
          return fail(e, "missing/invalid active clip timing");
        float delta =
            (float)((double)b->active_timing->source - b->active_timing->start);
        double movement = (double)delta * 17.5 * scale;
        float x =
            (float)((clip == 6 || clip == 10 || clip == 17) ? movement + low
                                                            : high - movement);
        if (!isfinite(x))
          return fail(e, "motion meter overflow");
        if (low > x)
          x = (float)low;
        if (high < x)
          x = (float)high;
        s->meter_x = x;
        stage->sprites[71 - 63].rect[0] = x;
        DRAW(71);
      }
    }
  }
#undef DRAW
#undef AT
  return 1;
}
