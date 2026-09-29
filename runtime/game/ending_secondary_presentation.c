#include "game/ending_secondary_presentation.h"
#include "core/random.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "secondary presentation: %s", why);
  return 0;
}
BkEndingSecondaryPresentationState bk_ending_secondary_presentation_initial(void) {
  return (BkEndingSecondaryPresentationState){.rate = .3f, .remaining = 15,
                                              .active_clip = 1, .slow_phase = 1};
}
int bk_ending_secondary_presentation_step(
    BkEndingSecondaryPresentationState *s,
    const BkEndingSecondaryPresentationBindings *b, float seconds,
    const BkEndingSecondaryPresentationOps *o, char e[256]) {
  if (!s || !b || !b->frame || !b->auxiliary || !b->automatic || !b->toggles ||
      !b->primary_root || !b->background_root || !b->hidden_nodes || !b->random ||
      !o || !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid bindings/time");
#define CALL(member, ...) do { \
  if (!o->member) return fail(e, "missing " #member " service"); \
  if (!o->member(o->context, __VA_ARGS__)) return 0; \
} while (0)
  uint32_t timestamp;
  CALL(clock, &timestamp, e);
  CALL(advance, BK_ENDING_PRESENT_BACKGROUND, seconds, e);
  CALL(hide, *b->background_root, b->toggles[0], e);
  if (b->frame->group == 2 && !b->auxiliary->variant) {
    uint32_t node = 0;
    CALL(find, *b->primary_root, "OYU", &node, e);
    CALL(hide, node, b->toggles[0], e);
  }
  if (b->frame->group == 1 && !b->toggles[0]) {
    uint32_t node = 0;
    CALL(find, *b->background_root, "Null_del2", &node, e);
    if (node) CALL(hide, node, 1, e);
  }
  if (*b->automatic && b->frame->state_721ee4 == 1) {
    int32_t clip;
    CALL(active, &clip, e);
    if (s->active_clip != clip) {
      s->rate = .3f;
      s->slow_phase = 1;
      s->remaining = 8;
      s->active_clip = clip;
    }
    BkEndingSecondaryTiming t;
    CALL(timing, (unsigned)clip, &t, e);
    /*47d548..87: rate*60 and seconds*scale are multiplied separately;
     * comparison includes equality/unordered. No source/loop flag rewrite. */
    double ahead = (double)t.rate * 60.0;
    ahead *= (double)seconds * (double)s->rate;
    if (!((double)t.end - ahead > (double)t.source)) {
      uint32_t remaining = (uint32_t)s->remaining - 1u;
      memcpy(&s->remaining, &remaining, 4);
      if (!s->remaining) {
        s->rate = s->slow_phase ? .9f : .3f;
        s->slow_phase = s->slow_phase ? 0 : 1;
        s->remaining = (int32_t)(bk_random_next(b->random) % 11u) + 10;
      }
    }
    CALL(advance, BK_ENDING_PRESENT_PRIMARY,
         (float)((double)seconds * (double)s->rate), e);
  } else if (b->frame->state_721ee4 == 5 || b->frame->state_721ee4 == 6) {
    CALL(advance, BK_ENDING_PRESENT_PRIMARY,
         (float)((double)seconds * (double)s->rate), e);
    s->remaining = 15;
    s->active_clip = s->slow_phase = 1;
  } else {
    CALL(advance, BK_ENDING_PRESENT_PRIMARY,
         (float)((double)seconds * (double).3f), e);
  }
  for (unsigned i = 0; i < 4; ++i) {
    /*54cd08 and56e368 contain identical names, checked independently. */
    const char *name = bk_ending_presentation_material(b->frame->group, i);
    if (!name) return fail(e, "group outside native material table");
    CALL(material, name, b->toggles[i < 2 ? 3 : 5], i == 1 ? .2f : 1.f, e);
  }
  for (unsigned i = 0; i < 3; ++i)
    if (b->hidden_nodes[i])
      CALL(hide, b->hidden_nodes[i], b->toggles[1], e);
  CALL(publish, e);
  CALL(face, BK_ENDING_PRESENT_EYE_RANGE, (float)b->auxiliary->expression_a,
       0, 0, e);
  CALL(face, BK_ENDING_PRESENT_EXPRESSION, 0, b->auxiliary->expression_b, 0, e);
  CALL(face, BK_ENDING_PRESENT_BLINK, 0, 0, timestamp, e);
  int32_t clip;
  CALL(active, &clip, e);
  float level;
  if (b->frame->group != 1) {
    if (clip == 1 || clip == 16) {
      CALL(level, &level, e);
      s->mouth_level = level;
    } else if (clip == 2 || clip == 14 || clip == 15 || clip == 3 || clip == 9) {
      if (!s->mouth_descending) {
        s->mouth_level = (float)((double)s->mouth_level + (double)seconds * 4.0);
        if (s->mouth_level > 9.f) {
          s->mouth_level = 9.f;
          s->mouth_descending = 1;
        }
      } else {
        s->mouth_level = (float)((double)s->mouth_level - (double)seconds * 4.0);
        if (!(s->mouth_level > 0.f)) {
          s->mouth_level = 0;
          s->mouth_descending = 0;
        }
      }
    } else {
      s->mouth_level = 9.f;
    }
    level = s->mouth_level;
  } else if (clip == 1 || clip == 2 || clip == 3 || clip == 9 || clip == 16 ||
             clip == 14 || clip == 15) {
    CALL(level, &level, e);
  } else {
    level = 9.f;
  }
  CALL(face, BK_ENDING_PRESENT_MOUTH, level, 0, timestamp, e);
#undef CALL
  return 1;
}
