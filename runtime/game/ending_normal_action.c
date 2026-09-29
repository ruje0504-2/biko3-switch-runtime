#include "game/ending_normal_action.h"
#include "core/random.h"
#include "game/ending_normal.h"
#include "game/ending_sound.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "normal ending action: %s", why);
  return 0;
}
static int32_t add(int32_t value, int32_t amount) {
  uint32_t bits = (uint32_t)value + (uint32_t)amount;
  memcpy(&value, &bits, sizeof(value));
  return value;
}
static int32_t input_word(const BkEndingFrameInput *in, unsigned index) {
  int32_t value;
  memcpy(&value, &in->words[index], sizeof(value));
  return value;
}
int bk_ending_normal_increment(uint32_t *random, float p, int32_t *out,
                                char e[256]) {
  if (!random || !out || !isfinite(p))
    return fail(e, "invalid action increment input");
  unsigned sample = bk_random_next(random) % 4;
  *out = p < .2f ? (sample == 0 ? 2 : 1)
         : p >= .19f && p < .4f ? (sample < 2 ? 2 : 1)
         : p >= .39f && p < .6f ? (sample < 2 ? 2 : 3)
         : p >= .6f ? 2 : 0;
  return 1;
}
static int playing(const BkEndingNormalActionOps *o, unsigned slot, int *out,
                    char e[256]) {
  int present = 0;
  if (!o->normal.present)
    return fail(e, "missing buffer presence service");
  if (!o->normal.present(o->normal.context, slot, &present, e))
    return 0;
  *out = 0;
  if (!present)
    return 1;
  return o->normal.status
             ? o->normal.status(o->normal.context, slot, out, e)
             : fail(e, "missing buffer status service");
}
static int pause(const BkEndingNormalActionOps *o, unsigned slot, char e[256]) {
  int active;
  if (!playing(o, slot, &active, e))
    return 0;
  return !active || (o->normal.pause
      ? o->normal.pause(o->normal.context, slot, e)
      : fail(e, "missing buffer pause service"));
}
static int play(const BkEndingNormalActionBindings *b,
                 const BkEndingNormalActionOps *o, unsigned slot,
                 int32_t flags, char e[256]) {
  const int32_t *volume = slot == 5 ? b->effect_volume : b->normal.voice_volume;
  if (!volume || !o->normal.play)
    return fail(e, "missing buffer play service/volume");
  return o->normal.play(o->normal.context, slot, flags, *volume, e);
}
static int load(const BkEndingNormalActionOps *o, unsigned slot,
                 const char *name, char e[256]) {
  return o->normal.load ? o->normal.load(o->normal.context, slot, name, e)
                        : fail(e, "missing speech load service");
}
static int voice(const BkEndingNormalActionBindings *b,
                  const BkEndingNormalActionOps *o, int mode, int flags,
                  char e[256]) {
  char name[32];
  if (!bk_ending_sound_normal_voice(b->normal.frame->group,
                                     b->normal.frame->camera_cached,
                                     mode, name, e))
    return 0;
  memcpy(b->normal.speech_name, name, strlen(name) + 1);
  return load(o, 0, name, e) && play(b, o, 0, flags, e);
}
static int contact(const BkEndingNormalActionBindings *b,
                    const BkEndingNormalActionOps *o, int alternate,
                    int unconditional_play, char e[256]) {
  char name[32];
  if (!bk_ending_sound_contact_voice(b->normal.frame->group, 1,
                                      *b->contact_index, alternate, name, e) ||
      !load(o, 1, name, e))
    return 0;
  return !(unconditional_play || b->normal.control->toggles[7]) ||
         play(b, o, 1, 0, e);
}
static int expression(const BkEndingNormalActionBindings *b,
                       const BkEndingNormalActionOps *o, int x, int y,
                       char e[256]) {
  b->normal.auxiliary->expression_a = x;
  b->normal.auxiliary->expression_b = y;
  return o->normal.eyes ? o->normal.eyes(o->normal.context, 1, e)
                        : fail(e, "missing eye-texture service");
}
static int request(const BkEndingNormalActionOps *o, unsigned actor,
                    int32_t clip, int instant, char e[256]) {
  if (instant)
    return o->instant ? o->instant(o->normal.context, actor, clip, e)
                      : fail(e, "missing instant clip request");
  return o->normal.request
      ? o->normal.request(o->normal.context, actor, clip, e)
      : fail(e, "missing configured clip request");
}
static int actor_present(const BkEndingNormalActionOps *o, int *exists,
                          char e[256]) {
  return o->normal.actor_present
      ? o->normal.actor_present(o->normal.context, 1, exists, e)
      : fail(e, "missing auxiliary actor presence service");
}
static int auxiliary_request(BkEndingNormalActionState *s,
                              const BkEndingNormalActionOps *o, char e[256]) {
  int exists = 0;
  return actor_present(o, &exists, e) &&
         (!exists || request(o, 1, s->clip, 0, e));
}
static int timing(const BkEndingNormalActionState *s,
                   const BkEndingNormalActionOps *o,
                   BkEndingNormalActionTiming *t, char e[256]) {
  if (!o->timing)
    return fail(e, "missing actual clip timing");
  if (!o->timing(o->normal.context, s->clip, t, e))
    return 0;
  return (isfinite(t->source) && isfinite(t->end)) ||
         fail(e, "nonfinite native action timing");
}
static int source_end(const BkEndingNormalActionState *s,
                       const BkEndingNormalActionOps *o, float end,
                       char e[256]) {
  return o->source ? o->source(o->normal.context, s->clip, end, e)
                    : fail(e, "missing clip source write");
}
static int find_pair(const BkEndingNormalActionBindings *b,
                      const BkEndingNormalActionOps *o, const char *reference,
                      const char *node, char e[256]) {
  if (!o->find)
    return fail(e, "missing actual primary node lookup");
  return o->find(o->normal.context, reference, b->direct_reference, e) &&
         o->find(o->normal.context, node, b->direct_node, e);
}
static int target_index(const BkEndingNormalActionBindings *b,
                         unsigned *index, char e[256]) {
  int32_t value = b->normal.frame->camera_cached;
  if (value < 0 || value >= 39)
    return fail(e, "target index outside original39-node table");
  *index = (unsigned)value;
  return 1;
}
static int warp_target(const BkEndingNormalActionBindings *b,
                        const BkEndingNormalActionOps *o, char e[256]) {
  unsigned index;
  if (!target_index(b, &index, e))
    return 0;
  return o->warp ? o->warp(o->normal.context, (float)b->targets[index][0],
                           (float)b->targets[index][1], e)
                  : fail(e, "missing pointer warp service");
}
static int begin(BkEndingNormalActionState *s,
                  const BkEndingNormalActionBindings *b, int32_t requested,
                  const BkEndingNormalActionOps *o, char e[256]) {
  BkEndingFrameState *f = b->normal.frame;
  BkEndingAuxiliaryState *a = b->normal.auxiliary;
  s->clip = requested;
  int special = f->camera_cached == 1;
  BkEndingNormalActionTiming t;
  if (!timing(s, o, &t, e))
    return 0;
  if (special ? t.source != t.end : t.source < t.end)
    return 1;
  if (!source_end(s, o, t.end, e))
    return 0;
  *b->normal.normal_ready = special ? 5 : 1;
  if (!special && (f->camera_cached == 26 || f->camera_cached == 27)) {
    s->clip = add(s->clip, 1);
    *b->contact_index = 10;
    if (f->camera_cached == 27) {
      a->index = 7;
      if (!find_pair(b, o, "A_siri_R", "R_siri", e))
        return 0;
    } else {
      a->index = 6;
      if (!find_pair(b, o, "A_siri_L", "L_siri", e))
        return 0;
    }
    if (!expression(b, o, 0, 4, e) || !request(o, 0, s->clip, 0, e))
      return 0;
  } else {
    if (!bk_ending_normal_increment(b->normal.random, a->progress,
                                      &s->increment, e))
      return 0;
    s->clip = add(s->clip, s->increment);
  }
  if ((!special && !auxiliary_request(s, o, e)) ||
      !request(o, 0, s->clip, 0, e))
    return 0;
  int active;
  if (!playing(o, 0, &active, e))
    return 0;
  if (!active) {
    if (!voice(b, o, 0, 1, e))
      return 0;
    a->pending = 4;
    if (special) {
      *b->contact_index = 0;
      return warp_target(b, o, e) && expression(b, o, 0, 3, e);
    }
  }
  if (special)
    return 1;
  switch (f->camera_cached) {
  case 11:
    if (!expression(b, o, 4, 4, e)) return 0;
    *b->contact_index = 2; a->index = 2;
    break;
  case 12:
    if (!expression(b, o, 0, 4, e)) return 0;
    *b->contact_index = 4; a->index = 3;
    break;
  case 9:
    if (!expression(b, o, 4, 6, e)) return 0;
    *b->contact_index = 6; a->index = 4;
    if (!find_pair(b, o, "A_nip_L", "L_nip", e)) return 0;
    break;
  case 10:
    if (!expression(b, o, 4, 3, e)) return 0;
    *b->contact_index = 8; a->index = 5;
    if (!find_pair(b, o, "A_nip_R", "R_nip", e)) return 0;
    break;
  case 5:
    if (!expression(b, o, 6, 4, e)) return 0;
    *b->contact_index = 12; a->index = 8;
    if (!play(b, o, 5, 1, e)) return 0;
    break;
  default:
    break;
  }
  return warp_target(b, o, e);
}
static int pointer_clamp(const BkEndingNormalActionBindings *b,
                          BkEndingFrameInput *input, int minimum,
                          const BkEndingNormalActionOps *o, char e[256]) {
  unsigned index;
  if (!target_index(b, &index, e))
    return 0;
  float xy[2];
  for (unsigned axis = 0; axis < 2; ++axis) {
    int32_t v = input_word(input, 9 + axis);
    int32_t lo = add(b->targets[index][axis], minimum);
    int32_t hi = add(b->targets[index][axis], 20);
    if (v <= lo) v = lo;
    else if (v >= hi) v = hi;
    input->words[9 + axis] = (uint32_t)v;
    xy[axis] = (float)v;
  }
  return o->warp ? o->warp(o->normal.context, xy[0], xy[1], e)
                  : fail(e, "missing pointer warp service");
}
static int motion_expression(const BkEndingNormalActionBindings *b,
                              const BkEndingNormalActionOps *o, int moving,
                              char e[256]) {
  switch (b->normal.frame->camera_cached) {
  case 11: return expression(b, o, moving ? 6 : 4, moving ? 3 : 4, e);
  case 12: return expression(b, o, moving ? 6 : 0, moving ? 3 : 4, e);
  case 9: return expression(b, o, moving ? 6 : 4, moving ? 3 : 6, e);
  case 10: return expression(b, o, moving ? 6 : 4, 3, e);
  case 26:
  case 27: return expression(b, o, moving ? 6 : 0, moving ? 3 : 4, e);
  case 5: return expression(b, o, moving ? 0 : 4, moving ? 4 : 3, e);
  default: return 1;
  }
}
static int motion_voice(const BkEndingNormalActionBindings *b,
                         const BkEndingNormalActionOps *o, int moving,
                         int narrow, char e[256]) {
  BkEndingAuxiliaryState *a = b->normal.auxiliary;
  int desired = moving ? 5 : 4;
  if (a->pending == desired)
    return 1;
  int waiting_contact = narrow && a->pending == 7;
  if (a->pending == 3 || waiting_contact) {
    int active;
    if (!playing(o, 0, &active, e))
      return 0;
    if (active && (waiting_contact || a->pending == 3))
      return 1;
  }
  if (!voice(b, o, moving ? 1 : 0, 1, e))
    return 0;
  a->pending = desired;
  return 1;
}
static int motion_notice(const BkEndingNormalActionBindings *b,
                          const BkEndingNormalActionOps *o, unsigned side,
                          int narrow, char e[256]) {
  int64_t index = (int64_t)*b->normal.normal_target * 2 + side;
  if (index < 0 || index >= 14)
    return fail(e, "normal input index outside fourteen words");
  BkEndingAuxiliaryState *a = b->normal.auxiliary;
  if (!b->normal.inputs[index]) {
    b->normal.inputs[index] = 1;
  } else {
    int preference = narrow ? bk_ending_normal_preference(
        b->normal.frame->group, (unsigned)index) : 0;
    if (preference < 0)
      return fail(e, "preference group outside original five rows");
    if (preference) {
      if (a->pending != 7 && bk_random_next(b->normal.random) % 1000 <= 15) {
        if (!voice(b, o, 2, 0, e))
          return 0;
        a->pending = 6;
      }
    } else {
      int active;
      if (!playing(o, 1, &active, e))
        return 0;
      if (!active && bk_random_next(b->normal.random) % 1000 <= 15 &&
          b->normal.control->toggles[7] && !contact(b, o, 0, 1, e))
        return 0;
    }
  }
  *b->side = (int8_t)side;
  return 1;
}
static void final_clip(BkEndingNormalActionState *s, int special) {
  if (special)
    s->clip = add(s->clip, 1);
  else if (s->increment >= 1 && s->increment <= 3)
    s->clip = add(s->clip, 4 - s->increment);
}
static int manual(BkEndingNormalActionState *s,
                   const BkEndingNormalActionBindings *b,
                   BkEndingFrameInput input, float seconds, int narrow,
                   const BkEndingNormalActionOps *o, char e[256]) {
  BkEndingFrameState *f = b->normal.frame;
  BkEndingAuxiliaryState *a = b->normal.auxiliary;
  if (!pointer_clamp(b, &input, narrow ? -20 : -40, o, e))
    return 0;
  if (narrow && a->pending == 6 && b->normal.control->toggles[7]) {
    if (!contact(b, o, 1, 1, e))
      return 0;
    a->pending = 7;
  }
  int32_t dx = input_word(&input, 6), dy = input_word(&input, 7);
  int moving = dx != 0 || dy != 0;
  if ((narrow && !motion_expression(b, o, moving, e)) ||
      !motion_voice(b, o, moving, narrow, e))
    return 0;
  /*The first abs value is explicitly stored tofloat. Int->float and
   *47a5b0's absolute value precede the extended precision sum/comparison. */
  double distance = (double)fabsf((float)dx) + (double)fabsf((float)dy);
  if (distance > (double)150.f * seconds &&
      distance < (double)300.f * seconds &&
      !motion_notice(b, o, 0, narrow, e))
    return 0;
  if (distance > (double)500.f * seconds &&
      !motion_notice(b, o, 1, narrow, e))
    return 0;
  if (narrow)
    f->camera_mode = 5;
  uint32_t result;
  if (!o->normal.key)
    return fail(e, "missing release-key service");
  if (!o->normal.key(o->normal.context, 0, 0, &result, e))
    return 0;
  if (!(result & 255u))
    return 1;
  a->index = 0;
  if (narrow) {
    if (!pause(o, 5, e))
      return 0;
    *b->normal.normal_ready = f->camera_cached == 11 || f->camera_cached == 12
                                 ? 6 : 3;
    final_clip(s, f->camera_cached == 26 || f->camera_cached == 27);
    return auxiliary_request(s, o, e) && request(o, 0, s->clip, 0, e);
  }
  *b->normal.normal_ready = 4;
  final_clip(s, 0);
  if (!request(o, 0, s->clip, 0, e))
    return 0;
  int cue = a->progress < .2f ? 9
            : a->progress >= .19f && a->progress < .4f ? 10
            : a->progress >= .39f ? 11 : -1;
  if (f->group >= 5 || cue < 0)
    return fail(e, "undefined native completion voice");
  char name[32];
  snprintf(name, sizeof(name), "PH%u02%02d.wav", f->group + 1u, cue);
  memcpy(b->normal.speech_name, name, strlen(name) + 1);
  if (!load(o, 0, name, e) || !play(b, o, 0, 0, e))
    return 0;
  if (a->progress < .2f || (a->progress >= .19f && a->progress < .4f))
    return expression(b, o, 7, 6, e);
  return a->progress >= .39f ? expression(b, o, 3, 6, e) : 1;
}
static int finish(BkEndingNormalActionState *s,
                   const BkEndingNormalActionBindings *b, int special,
                   const BkEndingNormalActionOps *o, uint32_t *result,
                   char e[256]) {
  BkEndingNormalActionTiming t;
  if (!timing(s, o, &t, e))
    return 0;
  double threshold = special ? t.end : (double)t.end - 1.f;
  if ((double)t.source < threshold)
    return 1;
  if (!source_end(s, o, t.end, e))
    return 0;
  BkEndingFrameState *f = b->normal.frame;
  int active;
  if (special || b->normal.auxiliary->pending == 6) {
    if (!playing(o, 0, &active, e))
      return 0;
    if (active)
      return 1;
    if (!special && !contact(b, o, 1, 0, e))
      return 0;
  }
  if (!special) {
    *b->normal.normal_ready = 0;
    f->camera_request = 1;
    f->state_721ee0 = 0;
    f->camera_mode = 0;
  }
  if (!request(o, 0, 1, special, e))
    return 0;
  int exists;
  if (!actor_present(o, &exists, e))
    return 0;
  if (exists) {
    if (!request(o, 1, 1, special, e))
      return 0;
    if (!special) {
      if (!o->normal.root_flag)
        return fail(e, "missing auxiliary root flag service");
      if (!o->normal.root_flag(o->normal.context, 1, 1, e))
        return 0;
    }
  }
  if (special) {
    f->camera_mode = 0;
    f->camera_request = 1;
    *b->normal.normal_ready = 0;
    f->state_721ee0 = 0;
    f->camera_cached = -1;
    b->normal.auxiliary->expression_a = 7;
  } else {
    f->camera_cached = -1;
    if (!pause(o, 0, e))
      return 0;
    *b->direct_reference = *b->direct_node = 0;
  }
  *result = 1;
  return 1;
}
int bk_ending_normal_action_step(BkEndingNormalActionState *s,
                                 const BkEndingNormalActionBindings *b,
                                 int32_t requested,
                                 const BkEndingFrameInput *input, float seconds,
                                 const BkEndingNormalActionOps *o,
                                 uint32_t *result, char e[256]) {
  if (!s || !b || !input || !o || !result || !b->normal.frame ||
      !b->normal.control || !b->normal.auxiliary || !b->normal.normal_ready ||
      !b->normal.normal_target || !b->normal.inputs || !b->normal.random ||
      !b->normal.speech_name || !b->normal.voice_volume || !b->contact_index ||
      !b->side || !b->targets || !b->direct_reference || !b->direct_node ||
      !isfinite(seconds) || seconds < 0 ||
      !isfinite(b->normal.auxiliary->progress))
    return fail(e, "invalid shared action bindings/time/progress");
  (void)bk_random_next(b->normal.random);
  (void)bk_random_next(b->normal.random);
  unsigned index;
  if (!target_index(b, &index, e))
    return 0;
  if (!o->project)
    return fail(e, "missing old-world target projection");
  if (!o->project(o->normal.context, index, b->targets[index], e))
    return 0;
  *result = 0;
  switch (*b->normal.normal_ready) {
  case 0: return begin(s, b, requested, o, e);
  case 1: return manual(s, b, *input, seconds, 1, o, e);
  case 3: return finish(s, b, 0, o, result, e);
  case 4: return finish(s, b, 1, o, result, e);
  case 5: return manual(s, b, *input, seconds, 0, o, e);
  case 6: {
    int32_t target = b->normal.frame->camera_cached;
    if (target == 11 || target == 12) {
      double ticks = (double)seconds * 10000.f;
      if (!b->flip || !o->recoil || ticks > INT32_MAX)
        return fail(e, "missing recoil service or signed tick overflow");
      int done;
      if (!o->recoil(o->normal.context, target == 12, .09f, *b->flip,
                     (uint32_t)(int32_t)ticks, 0, &done, e))
        return 0;
    }
    *b->normal.normal_ready = 3; /*Original unconditional4dee83. */
    return 1;
  }
  default: return 1;
  }
}
static int manual_root(const BkEndingNormalManualOps *o, int32_t flag,
                         char e[256]) {
  return o->normal.root_flag
      ? o->normal.root_flag(o->normal.context, 1, flag, e)
      : fail(e, "missing manual auxiliary root assignment");
}
static int manual_requests(const BkEndingNormalManualOps *o,
                            const int32_t *clip, char e[256]) {
  if (!o->normal.request)
    return fail(e, "missing manual configured requests");
  if (!o->normal.request(o->normal.context, 0, *clip, e))
    return 0;
  return o->normal.request(o->normal.context, 1, *clip, e);
}
static int manual_write(const BkEndingNormalManualOps *o, int32_t clip,
                          BkEndingClipWrite field, int32_t value, char e[256]) {
  return o->write ? o->write(o->normal.context, clip, field, value, e)
                  : fail(e, "missing primary mutable clip descriptor");
}
int bk_ending_normal_manual_step(int32_t *clip, BkEndingNormalControlState *s,
                                  const BkEndingNormalControlBindings *b,
                                  const BkEndingNormalManualOps *o,
                                  char e[256]) {
  static const int32_t clips[8] = {2, 7, 12, 17, 22, 27, 30, 33};
  if (!clip || !s || !b || !b->frame || !b->auxiliary || !o)
    return fail(e, "invalid manual control bindings");
  if (!manual_root(o, 0, e))
    return 0;
  switch (s->manual_action) {
  case 0: {
    int choice = s->manual_choice;
    if (choice >= 0 && choice < 8) {
      b->auxiliary->index = choice + 1;
      if (choice != 1 && choice != 2 && !manual_root(o, 1, e))
        return 0;
      *clip = choice == 7 && b->frame->group == 1 ? 27 : clips[choice];
    }
    if (!manual_requests(o, clip, e) ||
        !manual_write(o, *clip, BK_ENDING_CLIP_CHAIN, 1, e))
      return 0;
    int offset = s->manual_choice == 5 || s->manual_choice == 6 ? 1 : 3;
    if (!manual_write(o, *clip, BK_ENDING_CLIP_NEXT, add(*clip, offset), e))
      return 0;
    *clip = add(*clip, offset);
    return 1;
  }
  case 1:
    if (s->manual_choice >= 0 && s->manual_choice < 8)
      *clip = clips[(unsigned)s->manual_choice];
    *clip = add(*clip, s->manual_choice == 5 || s->manual_choice == 6 ? 1 : 3);
    return manual_requests(o, clip, e);
  case 3:
    if (!manual_write(o, add(*clip, -3), BK_ENDING_CLIP_CHAIN, 0, e))
      return 0;
    *clip = add(*clip, 1);
    if (!manual_requests(o, clip, e) ||
        !manual_write(o, *clip, BK_ENDING_CLIP_CHAIN, 1, e) ||
        !manual_write(o, *clip, BK_ENDING_CLIP_NEXT, 1, e))
      return 0;
    b->auxiliary->index = 0;
    *clip = 1;
    s->manual_action = 4;
    return 1;
  default:
    return 1;
  }
}
