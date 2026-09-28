#include "world/face_controller.h"
#include "core/random.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
static int fail(char *error) {
  snprintf(error, 256, "face controller: invalid input or nonfinite result");
  return 0;
}
static int expression_valid(int32_t n) {
  return n >= INT32_MIN / 10 && n <= INT32_MAX / 10;
}
static int valid(const BkFaceState *s) {
  return s && s->eye_count <= 8 && s->mouth_count <= 8 &&
         expression_valid(s->expression) &&
         expression_valid(s->previous_expression) && isfinite(s->eye) &&
         isfinite(s->mouth) && isfinite(s->eye_max) && isfinite(s->eye_min) &&
         isfinite(s->mouth_max) && isfinite(s->mouth_min) &&
         isfinite(s->transition) && isfinite(s->transition_rate) &&
         isfinite(s->previous_eye) && isfinite(s->previous_mouth);
}
static float factor(const BkFaceState *s, uint32_t now) {
  float f = (float)((double)(uint32_t)(now - s->transition_start_ms) *
                    s->transition_rate);
  return f > 1 ? 1 : f < 0 ? 0 : f;
}
static void submit(const BkFaceState *s, BkFaceCommands *out, unsigned group,
                    int blend, float f) {
  unsigned count = group ? s->mouth_count : s->eye_count;
  float value = group ? s->mouth : s->eye;
  float previous = group ? s->previous_mouth : s->previous_eye;
  if (!blend)
    value = value < 0 ? 0 : value > 9 ? 9 : value;
  float to = (float)((double)(s->expression * 10) + value);
  float from = blend ? (float)((double)(s->previous_expression * 10) + previous)
                     : to;
  for (unsigned i = 0; i < count; i++)
    out->commands[out->count++] =
        (BkFaceCommand){group, i, blend, from, to, blend ? f : 0};
}
static void submit_blend(BkFaceState *s, BkFaceCommands *out, float f) {
  submit(s, out, 0, 1, f);
  submit(s, out, 1, 1, f);
  if (f >= .999f)
    s->transition = 0;
}
static int commit(BkFaceState *s, BkFaceState *next, BkFaceCommands *out,
                    const BkFaceCommands *commands, char error[256]) {
  next->dirty = 0;
  if (!valid(next))
    return fail(error);
  for (unsigned i = 0; i < commands->count; i++) {
    const BkFaceCommand *c = commands->commands + i;
    if (!isfinite(c->from) || !isfinite(c->to) || !isfinite(c->weight))
      return fail(error);
  }
  *s = *next;
  *out = *commands;
  return 1;
}
int bk_face_init(BkFaceState *s, unsigned eyes, unsigned mouths,
                  char error[256]) {
  if (!s || eyes > 8 || mouths > 8)
    return fail(error);
  *s = (BkFaceState){.eye_max = 9, .mouth_max = 9,
                    .transition_rate = .0005f, .dirty = 1,
                    .eye_count = eyes, .mouth_count = mouths};
  return 1;
}
int bk_face_request(BkFaceState *s, int32_t expression, uint32_t now,
                     char error[256]) {
  if (!valid(s) || !expression_valid(expression))
    return fail(error);
  if (s->expression == expression)
    return 1;
  s->previous_expression = s->expression;
  s->transition = s->transition_rate == 0 ? 0 : 1;
  s->previous_eye = s->eye;
  s->previous_mouth = s->mouth;
  s->transition_start_ms = now;
  s->expression = expression;
  if (s->eye_count)
    s->eye = 0;
  if (s->mouth_count)
    s->mouth = 0;
  return 1;
}
int bk_face_mouth(BkFaceState *s, float level, uint32_t now,
                   BkFaceCommands *out, char error[256]) {
  if (!valid(s) || !out || !isfinite(level))
    return fail(error);
  BkFaceState next = *s;
  BkFaceCommands commands = {0};
  float value = s->mouth_mode == 2 ? s->mouth_min
                : s->mouth_mode == 1 ? s->mouth_max : level;
  if (level <= 0) {
    if (s->mouth_mode == 0)
      value = 0;
    if (value < s->mouth_min)
      value = s->mouth_min;
  } else if (value < s->mouth_min)
    value = s->mouth_min;
  else if (value >= s->mouth_max)
    value = s->mouth_max;
  if (next.transition > 0) {
    next.mouth = value;
    submit_blend(&next, &commands, factor(s, now));
  } else if (value != next.mouth) {
    next.mouth = value;
    submit(&next, &commands, 1, 0, 0);
  }
  return commit(s, &next, out, &commands, error);
}
static int32_t decrement(int32_t n) {
  return n == INT32_MIN ? INT32_MAX : n - 1;
}
static uint32_t bounded_random(uint32_t *state, uint32_t bound) {
  /* 0x41195d divides by (32767/bound), NOT modulo. The upper endpoint
   * can therefore occur (e.g. random32767 yields50 for bound50). */
  return bk_random_next(state) / (32767 / bound);
}
static void bounds(float *minimum, float *maximum) {
  if (*minimum < 0)
    *minimum = 0;
  if (*maximum > 9)
    *maximum = 9;
  if (*minimum > *maximum)
    *minimum = *maximum;
}
int bk_face_eye_range(BkFaceState *s, float minimum, float maximum,
                       uint32_t *random_state, char error[256]) {
  if (!valid(s) || !random_state || !isfinite(minimum) || !isfinite(maximum))
    return fail(error);
  bounds(&minimum, &maximum);
  s->eye_min = minimum;
  s->eye_max = maximum;
  if (s->eye > maximum || s->eye < minimum) {
    s->cycles = (int32_t)bounded_random(random_state, 30);
    s->rapid_count = (int32_t)bounded_random(random_state, 3) + 1;
    s->blink_phase = 1;
  }
  return 1;
}
int bk_face_mouth_range(BkFaceState *s, float minimum, float maximum,
                         char error[256]) {
  if (!valid(s) || !isfinite(minimum) || !isfinite(maximum))
    return fail(error);
  bounds(&minimum, &maximum);
  s->mouth_min = minimum;
  s->mouth_max = maximum;
  if (s->mouth > maximum)
    s->mouth = maximum;
  else if (s->mouth < minimum)
    s->mouth = minimum;
  return 1;
}
int bk_face_transition_seconds(BkFaceState *s, float seconds,
                                char error[256]) {
  if (!valid(s) || !isfinite(seconds))
    return fail(error);
  if (seconds < .01f) {
    s->transition_rate = 0;
    s->transition = 0;
  } else {
    s->transition_rate = (float)(1.0 / ((double)seconds * 1000));
    s->transition = 1;
  }
  return 1;
}
int bk_face_blink(BkFaceState *s, uint32_t timestamp, uint32_t now,
                   uint32_t *random_state, BkFaceCommands *out,
                   char error[256]) {
  if (!valid(s) || !out || !random_state)
    return fail(error);
  BkFaceState next = *s;
  BkFaceCommands commands = {0};
  uint32_t random = *random_state;
  float raw_factor = (float)((double)(uint32_t)(now - s->transition_start_ms) *
                             s->transition_rate);
  if (raw_factor > 1)
    next.transition = 0; /* Unlike mouth, before submitting the final blend. */
  float f = raw_factor > 1 ? 1 : raw_factor < 0 ? 0 : raw_factor;
  if (s->eye_count) {
    int automatic = s->eye_mode != 1 && s->eye_mode != 2;
    float value;
    if (!automatic)
      value = s->eye_mode == 2 ? s->eye_min : s->eye_max;
    else if (s->blink_phase == 0)
      value = s->eye_max;
    else {
      if (!s->blink_duration_ms)
        return fail(error);
      uint32_t remaining = s->deadline_ms < timestamp
                               ? 0 : s->deadline_ms - timestamp;
      float range = (float)((double)s->eye_max - s->eye_min);
      double fraction = 1.0 - (double)remaining / s->blink_duration_ms;
      value = s->blink_phase == 1
                  ? (float)((double)range - fraction * range + s->eye_min)
                  : (float)(fraction * range + s->eye_min);
      if (value < s->eye_min)
        value = s->eye_min;
      else if (value > s->eye_max)
        value = s->eye_max;
    }
    if (next.transition > 0) {
      next.eye = value;
      submit_blend(&next, &commands, f);
    } else if (value != next.eye) {
      next.eye = value;
      submit(&next, &commands, 0, 0, 0);
    }
    if (automatic && s->deadline_ms < timestamp) {
      if (s->blink_phase == 0) {
        next.blink_duration_ms = bounded_random(&random, 50) + 150;
        next.deadline_ms = timestamp + next.blink_duration_ms;
        next.cycles = decrement(next.cycles);
        if (next.cycles <= 0) {
          next.cycles = (int32_t)bounded_random(&random, 30);
          next.rapid_count = (int32_t)bounded_random(&random, 3) + 1;
          next.blink_phase = 1;
        }
      } else if (s->blink_phase == 1) {
        next.rapid_count = decrement(next.rapid_count);
        if (next.rapid_count <= 0) {
          next.blink_duration_ms = bounded_random(&random, 50) + 150;
          next.deadline_ms = timestamp + next.blink_duration_ms;
          next.rapid_count = (int32_t)bounded_random(&random, 3) + 1;
          next.blink_phase = -1;
        }
      } else if (s->blink_phase == -1) {
        next.blink_duration_ms = bounded_random(&random, 50) + 150;
        next.deadline_ms = timestamp + next.blink_duration_ms;
        next.blink_phase = 0;
      }
    }
  }
  if (!commit(s, &next, out, &commands, error))
    return 0;
  *random_state = random;
  return 1;
}
