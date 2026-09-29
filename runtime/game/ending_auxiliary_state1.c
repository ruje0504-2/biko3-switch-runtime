#include "game/ending_auxiliary_state1.h"
#include <math.h>
#include <stdio.h>

static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending auxiliary state1: %s", why);
  return 0;
}

static int query(const BkEndingAuxiliaryState1Ops *o, unsigned code,
                 unsigned mode, int *pressed, char e[256]) {
  uint32_t value = 0;
  if (!pressed || !o || !o->key || !o->key(o->context, code, mode, &value, e))
    return 0;
  *pressed = value != 0;
  return 1;
}

static int media_busy(const BkEndingAuxiliaryState1Ops *o, unsigned owner,
                      int *busy, char e[256]) {
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

int bk_ending_auxiliary_state1_step(
    const BkEndingAuxiliaryState1Bindings *b, const float pointer[2],
    float seconds, int32_t voice_volume,
    const BkEndingAuxiliaryState1Ops *o, char e[256]) {
  if (!b || !b->frame || !b->control || !b->auxiliary || !b->timer_6c7f6c ||
      !b->delay_54f8e0 || !pointer || !o ||
      b->control->state_721eec != 1 || !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid state1 bindings/time");

  /* 47E0FF: timer and randomized speech restart precede the media wait. */
  if (b->auxiliary->pending == 1)
    *b->timer_6c7f6c = (float)((double)*b->timer_6c7f6c + (double)seconds);
  if (b->auxiliary->pending == 1 &&
      *b->timer_6c7f6c >= (float)*b->delay_54f8e0) {
    int32_t random_value = 0;
    int playing = 0;
    BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_RESTART, .slot = 0,
                              .flags = 0, .volume = voice_volume};
    if (!o->audio || !o->audio(o->context, &call, &playing, e) ||
        !o->random || !o->random(o->context, &random_value, e))
      return 0;
    /* The recovered MSVC RNG is non-negative; keep signed remainder order. */
    *b->timer_6c7f6c = 0;
    *b->delay_54f8e0 = (int32_t)((int64_t)random_value % 21) + 30;
  }

  int busy = 0;
  if (!media_busy(o, 1, &busy, e))
    return 0;
  if (busy)
    return 1;

  if (b->frame->camera_mode != 1) {
    int32_t picked = 0;
    if (!o->pick || !o->pick(o->context, pointer, 4, &picked, e))
      return 0;
    if (picked == 1 && b->auxiliary->pending == 0) {
      int pressed = 0;
      if (!query(o, 0, 1, &pressed, e))
        return 0;
      if (!pressed && !query(o, 0x5a, 1, &pressed, e))
        return 0;
      if (!pressed && !query(o, 0x33450, 1, &pressed, e))
        return 0;
      if (pressed) {
        int32_t zone = -1;
        if (!o->menu || !o->menu(o->context, b->frame->camera_cached,
                                 &zone, e))
          return 0;
        if (zone != -1) {
          b->control->state_721eec = 3;
          if (!o->voice || !o->voice(o->context, 3, 0, 0, voice_volume, e))
            return 0;
          b->auxiliary->pending = 2;
          if (!o->request || !o->request(o->context, 2, e))
            return 0;
          if (!o->expression || !o->expression(o->context, 5, 4, 0, e))
            return 0;
          b->frame->camera_mode = 4;
          b->frame->camera_event = 1;
          return 1;
        }
      }
    }
  }
  if (b->auxiliary->pending != 1) {
    int pressed = 0;
    if (!query(o, 0, 2, &pressed, e))
      return 0;
    if (!pressed && !query(o, 1, 2, &pressed, e))
      return 0;
    if (!pressed && !query(o, 0x5a, 1, &pressed, e))
      return 0;
    if (!pressed && !query(o, 0x33450, 1, &pressed, e))
      return 0;
    if (!pressed)
      b->control->state_721eec = 2;
  }
  return 1;
}

int bk_ending_auxiliary_state2_step(BkEndingControlState *control,
                                    const BkEndingAuxiliaryState1Ops *o,
                                    char e[256]) {
  if (!control || !o || control->state_721eec != 2)
    return fail(e, "invalid state2 bindings");
  int pressed = 0;
  if (!query(o, 0, 0, &pressed, e))
    return 0;
  if (!pressed)
    return 1;
  if (!query(o, 1, 0, &pressed, e))
    return 0;
  if (!pressed)
    return 1;
  control->state_721eec = 1;
  return 1;
}
