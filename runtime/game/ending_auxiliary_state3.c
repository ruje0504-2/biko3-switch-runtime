#include "game/ending_auxiliary_state3.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending auxiliary state3: %s", why);
  return 0;
}

static int child_media_busy(const BkEndingAuxiliaryChildOps *o,
                            unsigned owner, int *busy, char e[256]) {
  int present = 0;
  if (!o || !o->present || !o->present(o->context, owner, &present, e))
    return 0;
  *busy = 0;
  if (!present)
    return 1;
  if (!o->status || !o->status(o->context, owner, busy, e))
    return 0;
  return 1;
}

static int child_reached(float value, float bound) {
  /* fcomp + test C0 skips the effect when value is below the bound. */
  return value >= bound;
}

static int child_effect(const BkEndingAuxiliaryChildOps *o, unsigned effect,
                        unsigned flags, int32_t volume, char e[256]) {
  return o->effect && o->effect(o->context, effect, flags, volume, e);
}

static float child_distance(float x, float y) {
  float squared = (float)((double)x * x + (double)y * y);
  return (float)sqrt((double)squared);
}

static int child_clip4_interpolate(
    const BkEndingAuxiliaryChildBindings *b,
    const BkEndingFrameInput *input, int32_t active,
    const BkEndingAuxiliaryChildOps *o, int *interpolated,
    float *active_source,
    char e[256]) {
  if (!interpolated || !b->targets_721f90 || !b->offset_mode_54e2f8 ||
      !active_source || !o->source || !o->end || !o->rewind ||
      !o->request || !o->timing)
    return fail(e, "481EA5 clip3 interpolation services are unavailable");
  *interpolated = 0;
  *active_source = 0;
  int32_t index = b->frame->camera_cached;
  if (index < 0 || index >= 39)
    return fail(e, "481EA5 clip3 interpolation target is unavailable");

  BkEndingClipTiming clip2, clip3;
  if (!o->request(o->context, 3, e) ||
      !o->end(o->context, (unsigned)active, e) ||
      !o->timing(o->context, 2, &clip2, e) ||
      !o->timing(o->context, 3, &clip3, e))
    return 0;
  float span2 = clip2.end - clip2.start;
  float span3 = clip3.end - clip3.start;
  if (!isfinite(span2) || !isfinite(span3) || span3 == 0)
    return fail(e, "481EA5 clip3 interpolation range is invalid");

  float half = (float)((double)*b->menu_width_7389e8 / 2.0);
  float offset_x = 0, offset_y = 0;
  switch (*b->offset_mode_54e2f8) {
  case 0: offset_y = half; break;
  case 1: offset_x = half; offset_y = half; break;
  case 2: offset_x = half; offset_y = -half; break;
  case 3: offset_x = -half; offset_y = half; break;
  case 4: offset_x = -half; offset_y = -half; break;
  case 5: offset_x = -half; break;
  case 6: offset_x = half; break;
  case 7: offset_y = half; break;
  case 8: offset_y = -half; break;
  default: break;
  }

  int32_t raw_x, raw_y;
  memcpy(&raw_x, &input->words[9], sizeof(raw_x));
  memcpy(&raw_y, &input->words[10], sizeof(raw_y));
  float anchor_x = (float)b->point_7220c8[0] + offset_x;
  float anchor_y = (float)b->point_7220c8[1] + offset_y;
  float target_x = (float)b->targets_721f90[index][0] - anchor_x;
  float target_y = (float)b->targets_721f90[index][1] - anchor_y;
  float pointer_x = (float)raw_x - anchor_x;
  float pointer_y = (float)raw_y - anchor_y;
  float target_distance = child_distance(target_x, target_y);
  float pointer_distance = child_distance(pointer_x, pointer_y);
  if (!isfinite(target_distance) || !isfinite(pointer_distance) ||
      !(target_distance > pointer_distance))
    return 1; /* Native falls through to the clip4 failure reset. */

  float ratio = span2 / span3;
  float source = (float)(((double)target_distance - pointer_distance) *
                         ((1.0 + (double)ratio) * span2) /
                         target_distance) + 30.0f;
  if (!isfinite(source))
    return fail(e, "481EA5 clip3 interpolation source is invalid");
  if (!o->source(o->context, 4, source, e))
    return 0;
  *interpolated = 1;
  *active_source = source;
  return 1;
}

static int child_success_transition(
    const BkEndingAuxiliaryChildBindings *b, float active_source,
    const BkEndingAuxiliaryChildOps *o, char e[256]) {
  /* 482326..482401. 0x320 is the slot-2 authored end field
   * (actor+0x2c8+0x58), not a model bound. */
  int busy = 0;
  if (!child_media_busy(o, 0, &busy, e))
    return 0;
  if (busy)
    return 1;
  BkEndingClipTiming clip2;
  if (!o->timing || !o->timing(o->context, 2, &clip2, e))
    return fail(e, "481EA5 successful transition timing is unavailable");
  if (clip2.end < active_source) {
    int32_t current = -1;
    if (!o->active || !o->active(o->context, &current, e))
      return 0;
    if (current == 3 && b->auxiliary->pending != 3) {
      if (!o->voice(o->context, 4, 0, 0, o->voice_volume, e))
        return 0;
      b->auxiliary->pending = 3;
      if (b->frame->group != 0 && b->frame->group != 2 &&
          !o->expression(o->context, 0, 4, 0, e))
        return 0;
    }
  } else if (b->auxiliary->pending != 2) {
    if (!o->voice(o->context, 3, 0, 0, o->voice_volume, e))
      return 0;
    b->auxiliary->pending = 2;
  }
  return 1;
}

static int child_failure_reset(
    const BkEndingAuxiliaryChildBindings *b, int32_t active,
    const BkEndingAuxiliaryChildOps *o, char e[256]) {
  /* 482406..4824ca. These are the native 54e2bc[group*3+1/+2] entries;
   * duplicate zero entries are intentional and are stopped twice. */
  static const unsigned stop_effects[5][2] = {
      {0, 0}, {0, 0}, {33, 0}, {19, 20}, {0, 0}};
  if (!o->request || !o->expression || !o->rewind || !o->effect_stop)
    return fail(e, "481EA5 clip4 failure transition services are unavailable");
  if (!o->request(o->context, 2, e) ||
      !o->expression(o->context, 5, 4, 0, e) ||
      !o->rewind(o->context, (unsigned)active, e))
    return 0;
  memset(b->effect_latches_6c7f60, 0, 10);
  unsigned group = b->frame->group;
  if (!o->effect_stop(o->context, stop_effects[group][0], e) ||
      !o->effect_stop(o->context, stop_effects[group][1], e))
    return 0;
  return 1;
}

int bk_ending_auxiliary_state3_child_step(
    const BkEndingAuxiliaryChildBindings *b, const BkEndingFrameInput *input,
    const BkEndingAuxiliaryChildOps *o, char e[256]) {
  if (!b || !b->frame || !b->control || !b->auxiliary ||
      !b->point_7220c8 || !b->menu_width_7389e8 ||
      !b->voice_latch_6c7f44 || !b->effect_latches_6c7f60 || !input || !o ||
      !o->active || !o->present ||
      !o->status || !o->hit || !o->request || !o->expression || !o->voice ||
      !o->random || b->frame->group >= 5)
    return fail(e, "invalid 481EA5 bindings/services");

  int32_t raw_x, raw_y;
  memcpy(&raw_x, &input->words[9], sizeof(raw_x));
  memcpy(&raw_y, &input->words[10], sizeof(raw_y));
  float pointer[2] = {(float)raw_x, (float)raw_y};
  float center[2] = {(float)b->point_7220c8[0],
                     (float)b->point_7220c8[1]};
  float radius = (float)((double)*b->menu_width_7389e8 / 2.0);
  if (!isfinite(pointer[0]) || !isfinite(pointer[1]) ||
      !isfinite(center[0]) || !isfinite(center[1]) || isnan(radius) ||
      radius < 0)
    return fail(e, "481EA5 circle geometry is invalid");
  int hit = 0;
  if (!o->hit(o->context, center, radius, pointer, &hit, e))
    return 0;
  if (!hit) {
    int32_t active = -1;
    if (!o->active(o->context, &active, e))
      return 0;
    if (active == 4) {
      int interpolated = 0;
      float active_source = 0;
      if (!child_clip4_interpolate(b, input, active, o, &interpolated,
                                   &active_source, e))
        return 0;
      if (interpolated &&
          !child_success_transition(b, active_source, o, e))
        return 0;
      if (!interpolated && !child_failure_reset(b, active, o, e))
        return 0;
    }
    if (b->frame->group == 2) {
      BkEndingClipTiming clip3;
      if (!o->timing || !o->effect ||
          !o->timing(o->context, 3, &clip3, e))
        return fail(e, "481EA5 group2 timing/effect service is unavailable");
      if (child_reached(clip3.source, 60.0f) &&
          b->effect_latches_6c7f60[0] == 0) {
        if (!child_effect(o, 32, 0, o->effect_volume, e)) return 0;
        b->effect_latches_6c7f60[0] = 1;
      }
      if (child_reached(clip3.source, 65.0f) &&
          b->effect_latches_6c7f60[1] == 0) {
        if (!child_effect(o, 33, 0, o->effect_volume, e)) return 0;
        b->effect_latches_6c7f60[1] = 1;
      }
    } else if (b->frame->group == 3) {
      BkEndingClipTiming clip2, clip3;
      if (!o->timing || !o->effect ||
          !o->timing(o->context, 2, &clip2, e) ||
          !o->timing(o->context, 3, &clip3, e))
        return fail(e, "481EA5 group3 timing/effect service is unavailable");
      if (child_reached(clip2.source, 32.0f) &&
          b->effect_latches_6c7f60[0] == 0) {
        if (!child_effect(o, 18, 0, o->effect_volume, e)) return 0;
        b->effect_latches_6c7f60[0] = 1;
      }
      if (b->effect_latches_6c7f60[0] == 1 &&
          b->effect_latches_6c7f60[1] == 0 &&
          child_reached(clip2.source, 32.0f)) {
        int present = 0;
        if (!o->effect_present ||
            !o->effect_present(o->context, 19, &present, e))
          return fail(e, "481EA5 group3 secondary effect presence is unavailable");
        if (present) {
          int playing = 0;
          if (!o->effect_status ||
              !o->effect_status(o->context, 19, &playing, e))
            return fail(e, "481EA5 group3 secondary effect status is unavailable");
          if (!playing) {
            if (!child_effect(o, 19, 1, o->effect_volume, e)) return 0;
            b->effect_latches_6c7f60[1] = 1;
          }
        }
      }
      if (child_reached(clip3.source, 56.0f) &&
          b->effect_latches_6c7f60[2] == 0) {
        if (!child_effect(o, 18, 0, o->effect_volume, e)) return 0;
        b->effect_latches_6c7f60[2] = 1;
      }
      if (b->effect_latches_6c7f60[2] == 1 &&
          b->effect_latches_6c7f60[3] == 0 &&
          child_reached(clip3.source, 56.0f)) {
        int present = 0;
        if (!o->effect_present ||
            !o->effect_present(o->context, 19, &present, e))
          return fail(e, "481EA5 group3 trailing effect presence is unavailable");
        if (present) {
          int playing = 0;
          if (!o->effect_status ||
              !o->effect_status(o->context, 19, &playing, e))
            return fail(e, "481EA5 group3 trailing effect status is unavailable");
          if (!playing) {
            if (!child_effect(o, 19, 1, o->effect_volume, e)) return 0;
            b->effect_latches_6c7f60[3] = 1;
          }
        }
      }
    } else if (b->frame->group == 4) {
      BkEndingClipTiming clip2;
      if (!o->timing || !o->effect ||
          !o->timing(o->context, 2, &clip2, e))
        return fail(e, "481EA5 group4 timing/effect service is unavailable");
      if (child_reached(clip2.source, clip2.end) &&
          b->effect_latches_6c7f60[0] == 0) {
        if (!child_effect(o, 13, 0, o->effect_volume, e)) return 0;
        b->effect_latches_6c7f60[0] = 1;
      }
    }
    return 1;
  }

  if (!o->request(o->context, 4, e))
    return 0;
  if (b->frame->group == 0 || b->frame->group == 2) {
    if (!o->expression(o->context, 0, 4, 0, e))
      return 0;
  } else if (!o->expression(o->context, 5, 4, 0, e)) {
    return 0;
  }
  if (b->frame->group == 2)
    b->auxiliary->index = 0x49;

  int busy = 0;
  if (!child_media_busy(o, 0, &busy, e))
    return 0;
  if (!busy && b->auxiliary->pending != 4) {
    if (!o->voice(o->context, 5, 0, 0, o->voice_volume, e))
      return 0;
    b->auxiliary->pending = 4;
  }

  int32_t random_value = 0;
  if (!o->random(o->context, &random_value, e))
    return 0;
  int rare = random_value % 1000 <= 5;
  if (*b->voice_latch_6c7f44 == 0) {
    if (b->control->toggles[7]) {
      if (!o->voice(o->context, 6, 1, 0, o->voice_volume, e))
        return 0;
      *b->voice_latch_6c7f44 = 1;
    }
  } else if (rare && b->control->toggles[7]) {
    if (!o->voice(o->context, 6, 1, 0, o->voice_volume, e))
      return 0;
  }
  return 1;
}

int bk_ending_auxiliary_state3_step(
    const BkEndingAuxiliaryState3Bindings *b, const BkEndingFrameInput *input,
    const BkEndingAuxiliaryState3Ops *o, char e[256]) {
  if (!b || !b->frame || !b->control || !b->auxiliary ||
      !b->point_7220c8 || !b->menu_width_7389e8 || !input || !o ||
      !o->child || !o->key || !o->hit || !o->expression || !o->request ||
      !o->voice || b->control->state_721eec != 3 || b->frame->group >= 5)
    return fail(e, "invalid state3 bindings/services");

  /* 47E32E: 481EA5 runs before the parent reads the copied input words. */
  if (!o->child(o->context, input, e))
    return 0;

  uint32_t value = 0;
  if (!o->key(o->context, 0, 3, &value, e))
    return 0;
  if (!value)
    return 1;

  int32_t raw_x, raw_y;
  memcpy(&raw_x, &input->words[9], sizeof(raw_x));
  memcpy(&raw_y, &input->words[10], sizeof(raw_y));
  float pointer[2] = {(float)raw_x, (float)raw_y};
  float center[2] = {(float)b->point_7220c8[0],
                     (float)b->point_7220c8[1]};
  float radius = (float)((double)*b->menu_width_7389e8 / 2.0);
  int hit = 0;
  if (!isfinite(pointer[0]) || !isfinite(pointer[1]) ||
      !isfinite(center[0]) || !isfinite(center[1]) || isnan(radius) ||
      radius < 0)
    return fail(e, "state3 circle geometry is invalid");
  if (!o->hit(o->context, center, radius, pointer, &hit, e))
    return 0;
  if (hit) {
    b->control->state_721eec = 8;
    b->auxiliary->index = -1;
    b->frame->camera_mode = 0;
    return 1;
  }

  if (b->frame->group == 2)
    b->auxiliary->index = 0x48;
  b->frame->camera_mode = 0;
  if (!o->expression(o->context, 7, 3, 1, e) ||
      !o->request(o->context, 1, e) ||
      !o->voice(o->context, 1, 0, 0, o->voice_volume, e))
    return 0;
  b->auxiliary->pending = 1;
  b->control->state_721eec = 1;
  b->frame->camera_cached = -1;
  return 1;
}
