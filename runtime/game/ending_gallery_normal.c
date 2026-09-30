#include "game/ending_gallery_normal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "gallery normal: %s", why);
  return 0;
}
BkEndingGalleryNormalState bk_ending_gallery_normal_initial(void) {
  return (BkEndingGalleryNormalState){1.f, 0, 0};
}
#define CALL(name, ...) \
  (o->name ? o->name(o->context, __VA_ARGS__) : fail(e, "missing " #name " service"))
static int signed_byte(uint8_t value) { return value < 128 ? value : (int)value - 256; }
static int recorded(const BkEndingGalleryNormalBindings *b, int32_t *out, char e[256]) {
  if ((uint32_t)*b->cursor >= b->workspace_capacity)
    return fail(e, "workspace cursor outside capacity");
  *out = b->workspace[*b->cursor];
  return 1;
}
static int playing(const BkEndingGalleryNormalOps *o, unsigned slot, int *busy, char e[256]) {
  int exists;
  if (!CALL(present, slot, &exists, e)) return 0;
  if (!exists) { *busy = 0; return 1; }
  return CALL(status, slot, busy, e);
}
static int finished(const BkEndingGalleryNormalState *s, const BkEndingGalleryNormalOps *o,
                    int *done, char e[256]) {
  int32_t clip = signed_byte(s->clip);
  if (clip < 0) return fail(e, "negative timing clip");
  BkEndingClipTiming t;
  if (!CALL(timing, (unsigned)clip, &t, e)) return 0;
  *done = !(t.source < t.end || t.source > t.end); /*x87 C3, includes unordered*/
  return 1;
}
static int request(const BkEndingGalleryNormalState *s, const BkEndingGalleryNormalOps *o,
                   char e[256]) {
  return CALL(request, 0, signed_byte(s->clip), e) &&
         CALL(request, 1, signed_byte(s->clip), e);
}
static int voice(const BkEndingGalleryNormalBindings *b, int32_t mode, int32_t slot,
                 int32_t flags, const BkEndingGalleryNormalOps *o, char e[256]) {
  int32_t action;
  if (!recorded(b, &action, e)) return 0;
  BkEndingGalleryVoiceBindings bindings = {&b->frame->group, b->speech_names, b->voice_volume};
  BkEndingGalleryVoiceOps ops = {o->context, o->random, o->load, o->play};
  return bk_ending_gallery_voice(&bindings, action, mode, slot, flags, &ops, e);
}
static int opening(BkEndingGalleryNormalState *s, const BkEndingGalleryNormalBindings *b,
                   float seconds, const BkEndingGalleryNormalOps *o, char e[256]) {
  if (*b->opening == 0) {
    *b->opening = 1;
    b->auxiliary->index = 0;
    if (!CALL(expression, 7, 6, 1, e)) return 0;
    s->fov = 1.f;
  } else if (*b->opening == 1) {
    uint32_t position[3], result;
    if (!CALL(target, position, e)) return 0;
    memcpy(b->frame->camera_values, position, sizeof(position));
    if (!CALL(fov, s->fov, e)) return 0;
    s->fov = (float)((double)s->fov - (double)seconds * .2f);
    int done = !(s->fov > .2f);
    if (done) s->fov = .2f;
    if (b->frame->group >= 5) return fail(e, "camera group outside table");
    if (!CALL(camera, BK_ENDING_OPENING_PRESET, b->frame->camera_clip,
              b->frame->camera_values, b->frame->camera_table[b->frame->group][3],
              &result, e)) return 0;
    if (done && (result & 255u)) {
      *b->opening = 0;
      s->fov = 1.f;
      b->auxiliary->pending = 0;
      b->frame->camera_mode = 0;
      *b->substate = 1;
    }
  }
  return 1;
}
int bk_ending_gallery_normal_step(BkEndingGalleryNormalState *s,
    const BkEndingGalleryNormalBindings *b, float seconds,
    const BkEndingGalleryNormalOps *o, char e[256]) {
  if (!s || !b || !b->frame || !b->control || !b->auxiliary || !b->substate ||
      !b->opening || !b->cursor || !b->workspace || !b->elapsed_bits ||
      !b->speech_names || !b->voice_volume || !b->effect_volume || !o ||
      !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid bindings/time");
  int32_t random, action;
  int busy, done;
  if (!CALL(random, &random, e)) return 0;
  int32_t delta = random % 3 + 1;
  if (!CALL(hidden, 1, 0, e)) return 0;
  switch (*b->substate) {
  case 0: return opening(s, b, seconds, o, e);
  case 1:
    if (!recorded(b, &action, e)) return 0;
    if ((uint32_t)action <= 7) {
      static const uint8_t clips[] = {2, 7, 12, 17, 22, 27};
      if (action < 6) s->clip = clips[action];
      b->auxiliary->index = action + 1;
      if (!CALL(expression, action == 0 ? 8 : 7, action == 0 ? 1 : 3, 1, e)) return 0;
      if ((action == 1 || action == 2) && !CALL(hidden, 1, 0, e)) return 0;
      if (action == 6) s->clip = 30;
      if (action == 7) {
        if (!CALL(play, 5, 1, *b->effect_volume, e)) return 0;
        s->clip = b->frame->group == 1 ? 27 : 33;
      }
    }
    if (!voice(b, 0, 0, 0, o, e) || !request(s, o, e)) return 0;
    *b->substate = 2;
    break;
  case 2:
    if (!finished(s, o, &done, e)) return 0;
    if (!done) break;
    if (!playing(o, 0, &busy, e)) return 0;
    if (busy) break;
    if (!recorded(b, &action, e)) return 0;
    if (action == 5 || action == 6) s->clip++;
    else { s->delta = (uint8_t)delta; s->clip += s->delta; }
    if (!voice(b, 1, 0, 1, o, e)) return 0;
    if (b->control->toggles[7] && !voice(b, 1, 1, 0, o, e)) return 0;
    if (!request(s, o, e)) return 0;
    *b->elapsed_bits = 0;
    *b->substate = 3;
    if (!recorded(b, &action, e)) return 0;
    return CALL(expression, action == 0 ? 3 : 7, 3, 1, e);
  case 3:
    if (!playing(o, 1, &busy, e)) return 0;
    if (!busy) {
      float elapsed;
      memcpy(&elapsed, b->elapsed_bits, sizeof(elapsed));
      elapsed += seconds;
      memcpy(b->elapsed_bits, &elapsed, sizeof(elapsed));
      if (elapsed > 10.f) { *b->substate = 4; *b->elapsed_bits = 0; }
    }
    break;
  case 4:
    if (!recorded(b, &action, e)) return 0;
    if (action == 5 || action == 6) {
      s->clip++;
      if (!CALL(random, &random, e)) return 0;
      char name[32];
      snprintf(name, sizeof(name), "PH%d02%02d.wav", signed_byte(b->frame->group) + 1,
               random % 3 + 9);
      memcpy(b->speech_names[0], name, strlen(name) + 1);
      if (!CALL(load, 0, name, e) || !CALL(play, 0, 0, *b->voice_volume, e)) return 0;
    } else {
      if (s->delta >= 1 && s->delta <= 3) s->clip += 4 - s->delta;
      if (!CALL(pause, 0, e)) return 0;
    }
    if (!CALL(expression, 9, 6, 1, e)) return 0;
    b->auxiliary->index = 0;
    if (!playing(o, 5, &busy, e)) return 0;
    if (busy && !CALL(stop, 5, e)) return 0;
    if (!request(s, o, e)) return 0;
    *b->substate = 5;
    break;
  case 5:
    if (!finished(s, o, &done, e)) return 0;
    if (!done) break;
    if (!playing(o, 0, &busy, e)) return 0;
    if (!busy) {
      s->clip = 1;
      if (!request(s, o, e)) return 0;
      uint32_t next = (uint32_t)*b->cursor + 1u;
      memcpy(b->cursor, &next, sizeof(next));
      b->frame->state_721ee0 = 0;
    }
    break;
  }
  return 1;
}
#undef CALL
