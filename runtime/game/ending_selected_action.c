#include "game/ending_selected_action.h"
#include "game/ending_selected_auxiliary.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game/ending_selected_action_tables.inc"

typedef struct {
  BkEndingSelectedActionState *s;
  const BkEndingSelectedActionBindings *b;
  const BkEndingSelectedControlBindings *c;
  const BkEndingSelectedActionOps *o;
  BkEndingFrameState *f;
  BkEndingAuxiliaryState *a;
  const BkEndingFrameInput *input;
  float seconds;
  int record_group;
  char *error;
} Action;
static int fail(Action *r, const char *why) {
  if (r->error) snprintf(r->error, 256, "selected action: %s", why);
  return 0;
}
#define CALL(member, ...) do { \
  if (!r->o->control.member) return fail(r, "missing " #member " service"); \
  if (!r->o->control.member(r->o->control.context, __VA_ARGS__, r->error)) return 0; \
} while (0)
#define EXTRA(member, ...) do { \
  if (!r->o->member) return fail(r, "missing " #member " service"); \
  if (!r->o->member(r->o->control.context, __VA_ARGS__, r->error)) return 0; \
} while (0)
static int32_t signed_word(uint32_t value) {
  int32_t out; memcpy(&out, &value, 4); return out;
}
static void add_word(int32_t *out, uint32_t value) {
  *out = signed_word((uint32_t)*out + value);
}
static int equal(float a, float b) {
  /*F(C)OMP's ZF is also set for unordered operands.*/
  return a == b || isnan(a) || isnan(b);
}
static void index_set(Action *r, unsigned offset) {
  r->a->index = signed_word((uint32_t)r->a->base +
                           (uint32_t)r->a->selection * 6u + offset);
}
static void motion(Action *r, float out[2]) {
  out[0] = (float)signed_word(r->input->words[6]);
  out[1] = (float)signed_word(r->input->words[7]);
}
static int active(Action *r, unsigned *out) {
  int32_t value;
  CALL(active, &value);
  if (value < 0) return fail(r, "negative active descriptor");
  *out = (unsigned)value; return 1;
}
static int clip(Action *r, unsigned slot, BkEndingSelectedMotionClip *out) {
  EXTRA(clip, slot, out); return 1;
}
static int current(Action *r, unsigned *slot, BkEndingSelectedMotionClip *out) {
  return active(r, slot) && clip(r, *slot, out);
}
static int source(Action *r, unsigned slot, float value) {
  EXTRA(source, slot, value); return 1;
}
static int request(Action *r, unsigned slot) {
  CALL(request, slot); return 1;
}
static int write_clip(Action *r, unsigned slot, BkEndingClipWrite op, int32_t v) {
  CALL(write, slot, op, v); return 1;
}
static int expression(Action *r, int32_t a, int32_t b, int32_t mode) {
  CALL(expression, a, b, mode); return 1;
}
static int key(Action *r, unsigned code, unsigned mode, int *pressed) {
  uint32_t value;
  CALL(key, code, mode, &value);
  *pressed = (value & 255u) != 0; return 1;
}
static int both_idle(Action *r, int *idle) {
  return key(r, 0, 0, idle) && (!*idle || key(r, 1, 0, idle));
}
static int audio(Action *r, BkEndingAudioOperation op, unsigned slot,
                 int32_t cue, int32_t bank, int32_t flags, int32_t volume,
                 int *playing) {
  BkEndingAudioCall call = {op, slot, cue, bank, flags, volume};
  CALL(audio, &call, playing); return 1;
}
static int voice(Action *r, int32_t cue, unsigned slot, int flags) {
  int playing;
  return audio(r, BK_ENDING_AUDIO_VOICE, slot, cue, 0, flags,
               *r->c->voice_volume, &playing);
}
static int cue(Action *r, int32_t value, int32_t bank) {
  int playing;
  return audio(r, BK_ENDING_AUDIO_CUE, 0, value, bank, 0,
               *r->c->voice_volume, &playing);
}
static int status(Action *r, unsigned slot, int *playing) {
  return audio(r, BK_ENDING_AUDIO_STATUS, slot, 0, 0, 0, 0, playing);
}
static int stop_if_playing(Action *r, unsigned slot) {
  int playing;
  if (!status(r, slot, &playing)) return 0;
  if (playing) EXTRA(stop, slot);
  return 1;
}
static int stop_effects(Action *r, unsigned count) {
  for (unsigned i = 0; i < count; ++i)
    if (!stop_if_playing(r, i + 2)) return 0;
  return 1;
}
static int effect_switch(Action *r, int band) {
  int64_t previous = band == 0 ? 2 : (int64_t)*r->b->previous_sound + 2;
  if (previous < 0 || previous >= 6)
    return fail(r, "previous sound outside six-buffer owner");
  int playing;
  if (!audio(r, BK_ENDING_AUDIO_PAUSE, (unsigned)previous, 0, 0, 0, 0, &playing) ||
      !audio(r, BK_ENDING_AUDIO_RESTART, (unsigned)band + 2, 0, 0, 0,
              *r->c->effect_volume, &playing)) return 0;
  *r->b->previous_sound = band; return 1;
}
static int group(Action *r, unsigned *out) {
  if (r->f->group >= 5) return fail(r, "group outside five rows");
  *out = r->f->group; return 1;
}
static int table_row(Action *r, unsigned *out) {
  unsigned g;
  if (!group(r, &g)) return 0;
  if ((uint32_t)r->a->selection >= 3 || (uint32_t)r->a->variant >= 2)
    return fail(r, "selection/variant outside native tables");
  *out = g * 6 + (unsigned)r->a->variant * 3 + (unsigned)r->a->selection;
  return 1;
}
static int configuration(Action *r, int *out) {
  unsigned g;
  if (!group(r, &g)) return 0;
  if ((uint32_t)r->a->selection >= 3 || (uint32_t)*r->c->choice >= 2)
    return fail(r, "configuration index outside native row");
  *out = r->c->configuration[g][r->a->selection * 2 + *r->c->choice] != 0;
  return 1;
}
static int matched(Action *r, int *out) {
  if ((uint32_t)*r->c->choice >= 2) return fail(r, "input lane outside two words");
  *out = r->c->inputs[*r->c->choice] != 0;
  return !*out || configuration(r, out);
}
static int publish_inputs(Action *r) {
  if ((uint32_t)*r->c->choice >= 2) return fail(r, "input lane outside two words");
  r->c->inputs[*r->c->choice] = 1;
  r->c->inputs[*r->c->choice == 0] = 0;
  r->c->processed[*r->c->choice == 0] = 0;
  return 1;
}
static unsigned progress_band(float progress) {
  return !(progress >= .6f) ? 0u : progress < .8f ? 1u : 2u;
}
static int first_voice(Action *r, unsigned lane) {
  unsigned g;
  if (!group(r, &g)) return 0;
  if (!r->c->group_prefix[g][lane]) {
    *r->b->random_latch = 1;
    r->c->group_prefix[g][lane] = 1;
  } else {
    int32_t random;
    CALL(random, &random);
    *r->b->random_latch = random % 1000 <= 25;
  }
  return 1;
}
static int drag(Action *r, const float values[2]) {
  float result;
  EXTRA(drag, values, &result);
  r->s->drag_result = result; return 1;
}
static int hit(Action *r, unsigned menu, int *result) {
  int32_t point[2] = {signed_word(r->input->words[9]),
                      signed_word(r->input->words[10])};
  EXTRA(hit, menu, point, result); return 1;
}
static int clock_sample(Action *r, uint32_t *out) {
  uint32_t value;
  EXTRA(clock, &value);
  *out = value; return 1;
}
static void reset_clock(Action *r) {
  r->s->clock_sample = 0;
  r->s->counter = 0;
  r->s->crossings = 0;
  r->s->previous_clock = 0;
}
static int inactivity(Action *r) {
  if (!r->input->words[6] && !r->input->words[7]) {
    if (!clock_sample(r, &r->s->clock_sample)) return 0;
    uint32_t delta = r->s->clock_sample - r->s->previous_clock;
    if (r->s->previous_clock) add_word(&r->s->counter, delta);
    if ((uint32_t)r->s->counter > 2000u) {
      r->s->counter = 0; r->s->crossings = 100;
    }
    if (!clock_sample(r, &r->s->previous_clock)) return 0;
  } else reset_clock(r);
  return 1;
}
static int cancel_full(Action *r) {
  if (!request(r, 19)) return 0;
  r->a->progress = (float)((double)r->a->progress - .3f);
  *r->c->gauge_y = (float)(60.0 * (double)*r->c->scale + (double)*r->c->gauge_y);
  *r->c->reset_c = 0; r->f->camera_mode = 0; r->a->gate = 1;
  *r->c->choice = 0; *r->b->reverse = 0; *r->b->fast_motion = 0;
  *r->b->reset_a = 0; *r->b->once = 0;
  reset_clock(r);
  EXTRA(stop, 0);
  EXTRA(ui_uv_reset, 53);
  EXTRA(ui_uv_reset, 54);
  return write_clip(r, 7, BK_ENDING_CLIP_NEXT, 6) &&
         write_clip(r, 11, BK_ENDING_CLIP_NEXT, 10);
}
static int mode_zero(Action *r) {
  EXTRA(pointer, r->input);
  if ((r->input->words[6] || r->input->words[7]) && r->a->pending != 1) {
    if (!voice(r, 1, 0, 0) || !expression(r, 6, 6, 1)) return 0;
    r->a->pending = 1;
  }
  int pressed;
  if (!key(r, 0, 3, &pressed)) return 0;
  if (!pressed) return 1;
  r->f->camera_event = 0;
  if (!hit(r, 0, &pressed)) return 0;
  if (pressed) {
    if (!request(r, 3) || !expression(r, 0, 4, 1)) return 0;
    index_set(r, 1);
    unsigned g;
    if (!group(r, &g) || !voice(r, r->c->group_prefix[g][0] ? 4 : 2, 0, 0)) return 0;
    r->a->pending = 2; r->a->gate = 1; r->f->camera_mode = 0;
    return 1;
  }
  unsigned g;
  if (!group(r, &g)) return 0;
  char name[32];
  snprintf(name, sizeof(name), "PH%u0103.wav", g + 1);
  memcpy(r->c->speech_name, name, strlen(name) + 1);
  CALL(load, 0, name);
  int playing;
  if (!audio(r, BK_ENDING_AUDIO_RESTART, 0, 0, 0, 1,
               *r->c->voice_volume, &playing) ||
      !expression(r, 6, 3, 1) || !request(r, 1)) return 0;
  r->f->camera_mode = 0; r->a->gate = 1;
  return 1;
}
static int mode_one(Action *r) {
  float values[2]; motion(r, values);
  if (values[0] != 0) {
    if (values[0] < 140.f && values[0] > -140.f) {
      add_word(r->b->small_motion, 1);
      if (*r->b->small_motion > 4) {
        *r->b->small_motion = 0; *r->b->large_motion = 0;
        *r->b->fast_motion = 0; r->s->crossings = 0; *r->c->choice = 0;
      }
    } else {
      values[0] = values[0] >= 140.f ? 140.f : -140.f;
      add_word(r->b->large_motion, 1); *r->b->small_motion = 0;
      if (*r->b->large_motion > 4) {
        *r->b->large_motion = 0; *r->b->fast_motion = 1;
        r->s->crossings = 7; *r->c->choice = 1;
      }
    }
  }
  if (!*r->b->fast_motion) {
    if (!request(r, 5) || !expression(r, 5, 4, 1)) return 0;
    index_set(r, 2);
    if (r->a->pending != 5) {
      if (!voice(r, 5, 0, 0)) return 0;
      r->a->pending = 5;
    }
  } else {
    if (!request(r, 13) || !expression(r, 5, 4, 1)) return 0;
    index_set(r, 3);
    if (r->a->pending != 7) {
      if (!voice(r, 7, 0, 0)) return 0;
      r->a->pending = 7;
    }
  }
  unsigned slot;
  BkEndingSelectedMotionClip p;
  if (!current(r, &slot, &p)) return 0;
  if (p.source >= p.end) {
    *r->c->mode = 2;
    int32_t selected_cue = -1;
    if (slot == 5 || slot == 13) {
      unsigned family = slot == 5 ? 0 : 1;
      selected_cue = (int32_t)(11u + family * 6u + progress_band(r->a->progress));
      if (!first_voice(r, family + 1) || !request(r, family ? 10 : 6)) return 0;
    }
    if (r->c->control->toggles[7] && *r->b->random_latch) {
      if (selected_cue < 0) return fail(r, "original voice cue is uninitialized");
      if (!voice(r, selected_cue, 1, 0)) return 0;
    }
    if (!publish_inputs(r)) return 0;
    *r->b->small_motion = 0; *r->b->large_motion = 0; *r->b->fast_motion = 0;
  }
  if (!drag(r, values)) return 0;
  int idle;
  if (!key(r, 0, 0, &idle)) return 0;
  if (!idle) return 1;
  if (!expression(r, 6, 3, 1) || !request(r, 4)) return 0;
  index_set(r, 1); r->a->gate = 1; r->f->camera_mode = 0;
  r->s->crossings = 0; *r->c->choice = 0;
  *r->b->small_motion = 0; *r->b->large_motion = 0; *r->b->fast_motion = 0;
  return stop_effects(r, 4);
}
static int record(Action *r, BkEndingRecord **out) {
  if (!r->b->records || r->record_group < 0 || r->record_group >= 5)
    return fail(r, "captured record outside five groups");
  *out = &r->b->records->groups[r->record_group]; return 1;
}
static int record_write(Action *r, int32_t value) {
  BkEndingRecord *p;
  if (!record(r, &p)) return 0;
  if ((uint32_t)p->count >= BK_ENDING_RECORD_CAPACITY)
    return fail(r, "action write outside recording lane");
  p->actions[p->count] = value; return 1;
}
static int record_increment(Action *r) {
  BkEndingRecord *p;
  if (!record(r, &p)) return 0;
  add_word(&p->count, 1); return 1;
}
static int mode_three(Action *r) {
  int pressed;
  if (!key(r, 0, 3, &pressed)) return 0;
  if (!pressed) return 1;
  unsigned menu;
  for (menu = 0; menu < 2; ++menu) {
    if (!hit(r, menu, &pressed)) return 0;
    if (pressed) break;
  }
  if (menu == 2) {
    r->a->gate = 1; r->f->camera_mode = 0; r->f->camera_event = 0;
    return 1;
  }
  int32_t previous = r->a->selection;
  if (previous >= 0 && previous < 3) {
    if (!r->o->choices) return fail(r, "missing menu labels");
    int32_t next = r->o->choices[menu];
    if (next >= 0 && next < 3 && next != previous) {
      r->a->selection = next; *r->b->selected = previous + 2;
      if (*r->c->previous_flow == 8 && !record_write(r, next + 12)) return 0;
    }
    /*Even an unmatched label increments the native story count.*/
    if (*r->c->previous_flow == 8 && !record_increment(r)) return 0;
  }
  int second = r->a->variant && (r->f->group == 0 || r->f->group == 2) ? 2 : 6;
  if (!expression(r, 7, second, 1)) return 0;
  r->s->crossings = 0; r->s->counter = 0; r->f->camera_event = 0;
  *r->b->small_motion = 0; *r->b->large_motion = 0; *r->c->reset_c = 0;
  *r->b->fast_motion = 0; *r->c->choice = 0; *r->b->once = 0;
  *r->b->reverse = 0; *r->b->sequence_elapsed = 0; *r->b->sequence = 0;
  *r->c->mode = 0; r->a->gate = 1;
  if (!cue(r, signed_word((uint32_t)r->a->selection + 1u), 1)) return 0;
  r->f->transition_action = 63; r->f->curtain_wanted = 1;
  unsigned g;
  if (!group(r, &g)) return 0;
  r->c->voice_latches[g * 4 + 1] = 0;
  return 1;
}
static int mode_six(Action *r) {
  float values[2]; motion(r, values);
  unsigned captured;
  if (!active(r, &captured)) return 0;
  int special = r->f->group == 4 && r->f->phase == 6 && r->a->selection != 1;
  if (!expression(r, special ? 5 : 6, special ? 11 : 3, 1)) return 0;
  if (!*r->c->plain_scheduled) {
    if (values[0] != 0) {
      *r->b->reverse = 0;
      unsigned slot;
      if (!active(r, &slot)) return 0;
      int forward = values[0] > 0;
      if ((forward && (slot == 7 || slot == 11)) ||
          (!forward && (slot == 6 || slot == 10))) {
        BkEndingSelectedMotionClip p;
        if (!clip(r, captured, &p)) return 0;
        r->s->end_difference = (float)((double)p.end - (double)p.source);
        if (equal(r->s->end_difference, 0)) {
          if (!request(r, forward ? slot - 1 : slot + 1)) return 0;
        } else *r->b->reverse = 1;
      }
    }
  } else if (!write_clip(r, 17, BK_ENDING_CLIP_CHAIN, 1) ||
             !write_clip(r, 18, BK_ENDING_CLIP_CHAIN, 1)) return 0;
  if (!drag(r, values) || !inactivity(r)) return 0;
  int idle = r->s->crossings == 100;
  if (!idle && !both_idle(r, &idle)) return 0;
  return !idle || cancel_full(r);
}
static int rewind_source(Action *r, unsigned slot) {
  BkEndingSelectedMotionClip p;
  return clip(r, slot, &p) && source(r, slot, p.start);
}
static int bridge_expression(Action *r) {
  int special = r->f->group == 4 && r->f->phase == 6;
  return expression(r, special ? 5 : 1, special ? 11 : 4, 1);
}
static int direction_expression(Action *r, unsigned family, int positive) {
  if (family && r->f->group == 4 && r->f->phase == 6)
    return expression(r, 1, 11, 1);
  int yes;
  if (!matched(r, &yes)) return 0;
  if (!yes) return expression(r, 6, positive ? 6 : 3, 1);
  if (positive || !family)
    return r->a->variant ? expression(r, 0, 4, 0) : expression(r, 0, 3, 1);
  /*The group4 negative branch reverses the variant test.*/
  int expression_six = r->f->group == 4 ? r->a->variant != 0 : r->a->variant == 0;
  return expression_six ? expression(r, 0, 6, 1) : expression(r, 0, 4, 0);
}
static int direction_voice(Action *r, unsigned family, int positive, int changed) {
  int playing, match;
  int32_t value;
  if (positive) {
    unsigned band = progress_band(r->a->progress);
    if (!bk_ending_selected_cue((unsigned)r->a->variant, 6u + family * 12u + band, &value))
      return fail(r, "voice table outside two rows");
    int32_t story = (int32_t)(11u + family * 6u + band);
    if (!matched(r, &match)) return 0;
    if (changed) {
      if (!status(r, 0, &playing)) return 0;
      if (!playing && r->s->speech_ready == 2) {
        if (!cue(r, value + (match ? 3 : 0), (int32_t)(3 + family * 2))) return 0;
        r->s->speech_ready = 1;
      }
    }
    if (changed) {
      if (!status(r, 1, &playing)) return 0;
      if (!playing) {
        if (!first_voice(r, family + 1)) return 0;
        if (r->c->control->toggles[7] && *r->b->random_latch && !voice(r, story, 1, 0)) return 0;
      }
    }
  } else if (changed) {
    if (!status(r, 0, &playing)) return 0;
    if (!playing && r->s->speech_ready == 2) {
      unsigned band = progress_band(r->a->progress);
      if (!bk_ending_selected_cue((unsigned)r->a->variant, 12u + family * 12u + band, &value))
        return fail(r, "voice table outside two rows");
      if (!matched(r, &match) ||
          !cue(r, value + (match ? 3 : 0), (int32_t)(4 + family * 2))) return 0;
      r->s->speech_ready = 1;
    }
  }
  return 1;
}
static int ordinary_direction(Action *r, unsigned family, int positive, int *changed) {
  *r->b->reverse = 0;
  if (!positive) *r->b->once = 1;
  unsigned slot;
  if (!active(r, &slot)) return 0;
  unsigned opposite_first = family ? 6 : 10;
  unsigned bridge = family ? 13 : 14;
  unsigned forward = family ? 10 : 6;
  if (slot == opposite_first || slot == opposite_first + 1) {
    if (family && positive && !bridge_expression(r)) return 0;
    if (!request(r, bridge)) return 0;
    *r->c->reset_c = 0;
    if (!voice(r, family ? 9 : 10, 0, 0)) return 0;
    if (!family) {
      if (!expression(r, 6, 6, 1)) return 0;
    } else if (!positive && !bridge_expression(r)) return 0;
    r->a->pending = family ? 9 : 10;
  } else if (slot == bridge) {
    BkEndingSelectedMotionClip p;
    if (!clip(r, slot, &p)) return 0;
    if (p.source >= p.end) {
      if (!request(r, forward) || !publish_inputs(r)) return 0;
      if (family && positive) *changed = 1;
    }
  } else if (slot == (family ? 14u : 13u) || slot == forward + (positive ? 1u : 0u)) {
    r->s->end_difference = 0;
    if (slot == forward + (positive ? 1u : 0u)) {
      BkEndingSelectedMotionClip p;
      if (!clip(r, slot, &p)) return 0;
      r->s->end_difference = (float)((double)p.end - (double)p.source);
    }
    if (equal(r->s->end_difference, 0)) {
      if (!request(r, forward + (positive ? 0u : 1u))) return 0;
    } else *r->b->reverse = 1;
    if (family && positive) *changed = 1;
  }
  if (positive && *r->b->once) {
    if (family) add_word(r->c->reset_c, 1);
    *r->b->once = 0;
  }
  return 1;
}
static int mode_two_tail(Action *r, const float values[2]) {
  unsigned slot;
  BkEndingSelectedMotionClip p;
  if (!current(r, &slot, &p)) return 0;
  int endpoint = 0;
  if (slot == 6 || slot == 10) {
    endpoint = equal(p.source, p.end);
    if (!endpoint) *r->b->reset_a = 0;
  } else if (slot == 7 || slot == 11) {
    endpoint = equal(p.source, p.start);
    if (!endpoint) *r->b->reset_a = 0;
  }
  if (endpoint && !*r->b->reset_a) {
    int band = -1;
    if (!(r->a->progress >= .6f)) band = 0;
    else if (r->a->progress >= .59f && r->a->progress < .8f) band = 1;
    else if (r->a->progress >= .79f) band = 2;
    if (band >= 0 && !effect_switch(r, band)) return 0;
    *r->b->reset_a = 1;
  }
  if (!active(r, &slot)) return 0;
  if (slot == 6 || slot == 7 || slot == 10 || slot == 11) {
    int yes;
    if (!configuration(r, &yes)) return 0;
    if (r->s->previous_progress < 0 && values[0] > 0) {
      r->a->progress = (float)((double)r->a->progress + (yes ? .01f : .005f));
      *r->c->gauge_y = (float)((double)*r->c->gauge_y -
                               (yes ? 2.0 : 1.0) * (double)*r->c->scale);
    }
    if (r->a->progress > .99f) {
      r->a->progress = 1;
      *r->c->gauge_y = (float)(51.0 * (double)*r->c->scale);
      *r->c->mode = 6;
      if (!rewind_source(r, 17) || !rewind_source(r, 18)) return 0;
      *r->b->once = 0; index_set(r, 4); r->a->gate = 3;
      if ((uint32_t)*r->c->choice >= 2) return fail(r, "input lane outside two words");
      r->c->processed[*r->c->choice] = 0;
      r->c->inputs[*r->c->choice] = 0;
      *r->c->reset_c = 0; r->s->previous_progress = 0;
      r->s->clock_sample = 0; r->s->previous_clock = 0; r->s->crossings = 0;
      return 1; /*The old counter is intentionally retained.*/
    }
    r->s->previous_progress = values[0];
  }
  if (!current(r, &slot, &p)) return 0;
  if (p.source >= p.end && !source(r, slot, p.end)) return 0;
  if (!active(r, &slot)) return 0;
  if (slot != 13 && slot != 14 && !drag(r, values)) return 0;
  int idle;
  if (!both_idle(r, &idle)) return 0;
  if (!idle) return 1;
  if (!active(r, &slot)) return 0;
  if (slot == 5 || slot == 6 || slot == 7 || slot == 14) {
    if (!request(r, 8) || !voice(r, 6, 0, 0)) return 0;
    r->a->pending = 6;
  } else if (slot == 9 || slot == 10 || slot == 11 || slot == 13) {
    if (!request(r, 12) || !voice(r, 8, 0, 0)) return 0;
    r->a->pending = 8;
  }
  memset(r->c->inputs, 0, 8); memset(r->c->processed, 0, 8);
  if (!expression(r, 5, 6, 1)) return 0;
  memset(r->b->words_318, 0, 40);
  index_set(r, 1);
  *r->b->small_motion = 0; *r->b->large_motion = 0; *r->c->reset_c = 0;
  r->f->camera_mode = 0; r->a->gate = 1; *r->b->once = 0; *r->c->choice = 0;
  r->s->motion_state = 0; *r->b->reverse = 0; *r->b->fast_motion = 0;
  *r->b->sequence_elapsed = 0; *r->b->sequence = 0;
  r->s->previous_progress = 0; reset_clock(r);
  return stop_effects(r, 4);
}
static int mode_two(Action *r) {
  float values[2]; motion(r, values);
  int scheduled = *r->c->plain_scheduled != 0;
  if (scheduled) {
    int pressed;
    if (!key(r, 1, 1, &pressed)) return 0;
    if (pressed) {
      *r->c->choice = 1;
      if (!write_clip(r, 7, BK_ENDING_CLIP_NEXT, 13) ||
          !write_clip(r, 11, BK_ENDING_CLIP_NEXT, 10)) return 0;
    } else {
      if (!key(r, 1, 3, &pressed)) return 0;
      if (pressed) {
        *r->c->choice = 0;
        if (!write_clip(r, 7, BK_ENDING_CLIP_NEXT, 6) ||
            !write_clip(r, 11, BK_ENDING_CLIP_NEXT, 14)) return 0;
      }
    }
  }
  if (!clock_sample(r, &r->s->clock_sample)) return 0;
  uint32_t delta = r->s->clock_sample - r->s->previous_clock;
  if (!scheduled && r->s->previous_motion < 0 && values[0] > 0)
    add_word(&r->s->crossings, 1);
  if (!scheduled || r->s->previous_clock) add_word(&r->s->counter, delta);
  r->s->diagnostic_ms = r->s->counter;
  if (!scheduled) r->s->diagnostic_crossings = r->s->crossings;
  if ((uint32_t)r->s->counter > 5000u) {
    if (!scheduled) {
      *r->c->choice = r->s->crossings > 6;
      r->s->crossings = 0;
    }
    r->s->counter = 0;
  }
  if (!clock_sample(r, &r->s->previous_clock)) return 0;
  if (!scheduled) r->s->previous_motion = values[0];
  unsigned family = *r->c->choice != 0;
  index_set(r, family ? 3 : 2);
  if (values[0] != 0) {
    int positive = values[0] > 0, changed = 0;
    if (!scheduled && !ordinary_direction(r, family, positive, &changed)) return 0;
    if (!direction_expression(r, family, positive)) return 0;
    if (r->s->motion_direction == (positive ? 2 : 1)) changed = 1;
    r->s->motion_direction = positive ? 1 : 2;
    if (!direction_voice(r, family, positive, changed)) return 0;
  }
  if (scheduled) {
    static const unsigned slots[] = {6, 7, 10, 11, 13, 14};
    for (unsigned i = 0; i < 6; ++i)
      if (!write_clip(r, slots[i], BK_ENDING_CLIP_CHAIN, 1)) return 0;
    BkEndingSelectedMotionClip p;
    if (!clip(r, 13, &p)) return 0;
    int ended = p.source >= p.end;
    if (!ended) {
      if (!clip(r, 14, &p)) return 0;
      ended = p.source >= p.end;
    }
    if (ended && (!rewind_source(r, 13) || !rewind_source(r, 14) || !publish_inputs(r))) return 0;
  }
  return mode_two_tail(r, values);
}
static int camera_row(Action *r, const int32_t **out) {
  unsigned g;
  if (!group(r, &g)) return 0;
  if ((uint32_t)r->a->selection >= 3) return fail(r, "camera selection outside three rows");
  *out = r->c->camera_words + g * 15 + (unsigned)r->a->selection * 5;
  return 1;
}
static void camera_set(Action *r, const uint32_t words[4]) {
  memcpy(&r->c->camera->yaw, words, 4);
  memcpy(&r->c->camera->pitch, words + 1, 4);
  memcpy(&r->c->camera->radius, words + 2, 4);
  memcpy(&r->c->camera->height, words + 3, 4);
}
static void camera_save(Action *r) {
  memcpy(r->s->saved_camera, &r->c->camera->yaw, 4);
  memcpy(r->s->saved_camera + 1, &r->c->camera->pitch, 4);
  memcpy(r->s->saved_camera + 2, &r->c->camera->radius, 4);
  memcpy(r->s->saved_camera + 3, &r->c->camera->height, 4);
}
static int camera_default(Action *r) {
  const int32_t *row;
  if (!camera_row(r, &row)) return 0;
  uint32_t words[4];
  static const unsigned fields[] = {0, 1, 4, 3};
  for (unsigned i = 0; i < 4; ++i) memcpy(words + i, row + fields[i], 4);
  camera_set(r, words); return 1;
}
static int camera_target(Action *r, unsigned node) {
  float target[3];
  CALL(target, node, target);
  memcpy(r->f->camera_values, target, 12); return 1;
}
static int preset(Action *r, int *complete) {
  uint32_t done, offset[3];
  memcpy(offset, r->f->camera_values, 12);
  CALL(camera, BK_ENDING_OPENING_PRESET, 0, offset, 0x3dcccccdu, &done);
  *complete = (done & 255u) != 0; return 1;
}
static int camera_repeat(Action *r, int finish, int32_t counter) {
  unsigned row;
  if (!table_row(r, &row)) return 0;
  unsigned count = finish ? 3u : 2u;
  if ((uint32_t)counter >= count) return fail(r, "camera repetition outside native table");
  const uint32_t *words = (finish ? selected_finish_camera : selected_repeat_camera) +
                         row * count * 4u + (uint32_t)counter * 4u;
  camera_set(r, words); return 1;
}
static int mode_four(Action *r) {
  float values[2]; motion(r, values);
  if (!inactivity(r)) return 0;
  int idle = r->s->crossings == 100;
  if (!idle && !both_idle(r, &idle)) return 0;
  if (idle) return cancel_full(r);
  int playing;
  if (!status(r, 0, &playing)) return 0;
  if (!playing && r->a->pending != 24) {
    if (!voice(r, 24, 0, 1)) return 0;
    if (r->c->control->toggles[7] && !voice(r, 25, 1, 0)) return 0;
    r->a->pending = 24;
  }
  unsigned slot;
  BkEndingSelectedMotionClip p;
  if (!*r->c->plain_scheduled) {
    if (values[0] != 0) {
      *r->b->reverse = 0;
      if (!active(r, &slot)) return 0;
      unsigned opposite = values[0] > 0 ? 18 : 17;
      unsigned wanted = values[0] > 0 ? 17 : 18;
      if (slot == opposite) {
        if (!clip(r, opposite, &p)) return 0;
        r->s->end_difference = (float)((double)p.end - (double)p.source);
        if (equal(r->s->end_difference, 0)) {
          if (!request(r, wanted)) return 0;
        } else *r->b->reverse = 1;
      } else if (slot == 15 || slot == 16) {
        if (!clip(r, slot, &p)) return 0;
        if (p.source >= p.end && !request(r, wanted)) return 0;
      }
    }
  } else if (!write_clip(r, 17, BK_ENDING_CLIP_CHAIN, 1) ||
             !write_clip(r, 18, BK_ENDING_CLIP_CHAIN, 1)) return 0;
  if (r->s->previous_progress < 0 && values[0] > 0) {
    if (!status(r, 1, &playing)) return 0;
    if (!playing) add_word(r->c->reset_c, 1);
  }
  r->s->previous_progress = values[0];
  if (!current(r, &slot, &p)) return 0;
  int endpoint = 0;
  if (slot == 17 || slot == 18) {
    endpoint = equal(p.source, slot == 17 ? p.end : p.start);
    if (!endpoint) *r->b->reset_a = 0;
  }
  if (endpoint && !*r->b->reset_a) {
    if (!effect_switch(r, 2)) return 0;
    *r->b->reset_a = 1;
  }
  if (!current(r, &slot, &p)) return 0;
  if (p.source >= p.end && !source(r, slot, p.end)) return 0;
  if (!active(r, &slot)) return 0;
  if (slot != 15 && slot != 16 && !drag(r, values)) return 0;
  if (*r->c->reset_c <= 10) return 1;
  reset_clock(r);
  r->s->saved_toggle = r->c->control->toggles[1];
  unsigned row;
  if (!table_row(r, &row)) return 0;
  if (selected_replay_variant[row]) {
    r->s->counter = 0; *r->c->mode = 5; r->s->replay_variant = 1;
  } else {
    r->s->counter = -1; *r->c->mode = 7; r->s->replay_variant = 0;
  }
  r->f->camera_mode = 4;
  EXTRA(ui_byte, 52, BK_ENDING_SELECTED_UI_BYTE_167, 1);
  EXTRA(ui_byte, 53, BK_ENDING_SELECTED_UI_BYTE_166, 0);
  if (!request(r, 20) || !expression(r, 1, 4, 1) || !voice(r, 26, 0, 0)) return 0;
  if (r->c->control->toggles[7] && !voice(r, 27, 1, 0)) return 0;
  r->a->pending = 26;
  const int32_t *words;
  if (!camera_row(r, &words)) return 0;
  uint32_t raw[4]; memcpy(raw, words, 16);
  camera_set(r, raw); /*Here radius is row[2], NOT the usual row[4].*/
  static const unsigned fields[] = {0, 1, 4, 3};
  for (unsigned i = 0; i < 4; ++i)
    memcpy(&r->b->presets->active[i][0], words + fields[i], 4);
  r->c->control->target_choice = 0; r->f->camera_clip = 0;
  if (!camera_target(r, 5)) return 0;
  memcpy(r->s->saved_target, r->f->camera_values, 12);
  *r->b->small_motion = 0; *r->b->large_motion = 0; *r->c->reset_c = 0;
  r->s->previous_progress = 0; *r->b->once = 0;
  EXTRA(ui_byte, 52, BK_ENDING_SELECTED_UI_BYTE_134, 3);
  EXTRA(ui_fade, 52, 1.f);
  return stop_if_playing(r, 5);
}
static int finish_expression(Action *r) {
  if (!r->a->variant) return expression(r, 6, 6, 1);
  if (!expression(r, 5, 4, 1)) return 0;
  r->b->controller->expression_override = 5; *r->b->face_mode = 1;
  return 1;
}
static int finish_replay(Action *r) {
  if (!camera_target(r, 5) || !voice(r, 28, 0, 0)) return 0;
  r->f->camera_mode = 4; *r->c->mode = 7; r->s->replay_variant = 1;
  camera_save(r);
  r->s->saved_toggle = r->c->control->toggles[1];
  r->c->control->toggles[1] = 1;
  if (!camera_repeat(r, 1, 0)) return 0;
  memcpy(r->s->saved_target, r->f->camera_values, 12);
  return 1;
}
static int special_finish(Action *r) {
  return (r->f->group == 0 || r->f->group == 4) && r->a->variant == 0 &&
         (r->a->selection == 0 || r->a->selection == 1);
}
static int finish_stage_one(Action *r) {
  unsigned slot;
  if (!active(r, &slot)) return 0;
  if (slot == 22) index_set(r, 5);
  unsigned first = special_finish(r) ? 21 : 22;
  if (!active(r, &slot)) return 0;
  if (slot == first) {
    BkEndingSelectedMotionClip p;
    if (!clip(r, first, &p)) return 0;
    if (p.source >= p.end) {
      int playing;
      if (!status(r, 0, &playing)) return 0;
      if (!playing) {
        if (!request(r, first + 1)) return 0;
        unsigned row;
        if (!table_row(r, &row)) return 0;
        if (selected_replay_variant[row] && !finish_replay(r)) return 0;
      }
    }
  }
  /*The original keeps executing this stage even after requesting mode7.
   *Both the group classification and the active slot are live again here.*/
  unsigned second = special_finish(r) ? 22 : 23;
  if (!active(r, &slot)) return 0;
  if (slot == second) {
    BkEndingSelectedMotionClip p;
    if (!clip(r, second, &p)) return 0;
    if (p.source >= p.end && !request(r, slot + 1)) return 0;
  }
  unsigned row;
  if (!table_row(r, &row) || !active(r, &slot)) return 0;
  if (slot == (uint32_t)selected_terminal_clip[row]) {
    index_set(r, 5);
    if (!r->a->variant && !expression(r, 6, 3, 1)) return 0;
    if (!voice(r, 29, 0, 1)) return 0;
    if (r->c->control->toggles[7] && !voice(r, 30, 1, 0)) return 0;
    r->a->pending = 29; *r->b->stage = 2; r->s->completion_elapsed = 0;
  }
  return 1;
}
static int finish_stage_two(Action *r) {
  r->s->completion_elapsed = (float)((double)r->s->completion_elapsed + (double)r->seconds);
  int playing;
  if (!status(r, 1, &playing)) return 0;
  if (playing || !(r->s->completion_elapsed >= 20.f)) return 1;
  unsigned g;
  if (!group(r, &g)) return 0;
  memset(r->c->group_prefix[g], 0, 12); r->b->group_seen[g] = 0;
  memset(r->b->group_suffix[g], 0, 48);
  memset(r->c->voice_latches + g * 4, 0, 16);
  *r->b->small_motion = 0; *r->b->stage = 0;
  r->f->transition_action = 49; r->f->curtain_wanted = 1;
  *r->b->fast_motion = 0;
  memset(r->c->inputs, 0, 8); memset(r->c->processed, 0, 8);
  *r->b->once = 0; *r->b->animation_scale = .02f; *r->b->reverse = 0;
  *r->b->previous_sound = -1; *r->b->reset_340 = 0;
  *r->b->reset_a = 0; *r->b->reset_354 = 0;
  if (*r->c->previous_flow == 8) {
    if (!record_write(r, 21) || !record_increment(r)) return 0;
    BkEndingRecord *p;
    if (!record(r, &p)) return 0;
    unsigned lane = r->a->variant != 0;
    memcpy(p->retained[lane], p->actions, sizeof(p->actions));
    if (!group(r, &g)) return 0;
    if (!r->b->working) return fail(r, "missing working unlock owner");
    r->b->working[g][6 + lane] = 1;
  }
  r->a->gate = -1; r->f->camera_cached = -1; r->f->camera_event = 0;
  r->s->completion_elapsed = 0; r->s->counter = 0;
  return 1;
}
static int mode_five(Action *r) {
  r->c->camera->fov = .2f;
  int idle;
  if (*r->b->small_motion == 1) {
    if (!both_idle(r, &idle)) return 0;
    if (idle) r->f->camera_mode = 0;
  }
  if (r->f->camera_mode) {
    int done;
    if (!preset(r, &done)) return 0;
    if (done && !*r->b->small_motion) {
      if (!camera_default(r)) return 0;
      add_word(r->b->small_motion, 1);
    }
  }
  switch (*r->b->stage) {
  case 0: {
    unsigned slot;
    BkEndingSelectedMotionClip p;
    if (!current(r, &slot, &p)) return 0;
    if (p.source >= p.end) {
      if (!source(r, slot, p.end)) return 0;
      int playing;
      if (!status(r, 1, &playing)) return 0;
      if (!playing) {
        if (!status(r, 0, &playing)) return 0;
        if (!playing) {
          if (!finish_expression(r) || !request(r, 21)) return 0;
          r->a->pending = 28; *r->b->stage = 1;
          return stop_effects(r, 3);
        }
      }
    }
    return 1;
  }
  case 1: return finish_stage_one(r);
  case 2: return finish_stage_two(r);
  default: return 1;
  }
}
static int mode_seven(Action *r) {
  unsigned slot;
  BkEndingSelectedMotionClip p;
  if (!current(r, &slot, &p)) return 0;
  if (p.source >= p.end) {
    int playing;
    if (!status(r, 0, &playing)) return 0;
    if (!playing && r->f->camera_mode == 4) {
      r->c->control->toggles[1] = 1;
      add_word(&r->s->counter, 1);
      if (r->s->counter == (int32_t)r->s->replay_variant + 2) {
        *r->c->mode = 5; r->f->camera_mode = 0;
        if (!active(r, &slot) || !request(r, slot + 1)) return 0;
        if (!r->s->replay_variant) {
          *r->c->substate = 0;
          if (!camera_default(r) || !finish_expression(r)) return 0;
          r->a->pending = 28; *r->b->stage = 1;
          if (!stop_effects(r, 3)) return 0;
        } else {
          *r->c->substate = 1;
          camera_set(r, r->s->saved_camera);
        }
        r->s->completion_elapsed = 0;
        memcpy(r->f->camera_values, r->s->saved_target, 12);
        r->s->counter = 0;
        r->c->control->toggles[1] = r->s->saved_toggle;
        return 1;
      }
      if (!active(r, &slot)) return 0;
      EXTRA(repeat, slot);
      if (!r->s->replay_variant) {
        if (!voice(r, 26, 0, 0) || !camera_target(r, 13) ||
            !camera_repeat(r, 0, r->s->counter)) return 0;
      } else if (!voice(r, 28, 0, 0) || !camera_repeat(r, 1, r->s->counter)) return 0;
    }
  }
  if (!r->s->replay_variant && r->f->camera_mode && !*r->b->small_motion) {
    int done;
    if (!preset(r, &done)) return 0;
    if (done) {
      if (!camera_default(r)) return 0;
      add_word(r->b->small_motion, 1); r->f->camera_mode = 4;
    }
  }
  return 1;
}
BkEndingSelectedActionState bk_ending_selected_action_initial(void) {
  return (BkEndingSelectedActionState){.speech_ready = 2};
}
int bk_ending_selected_action_step(BkEndingSelectedActionState *s,
    const BkEndingSelectedActionBindings *b, const BkEndingFrameInput *input,
    float seconds, const BkEndingSelectedActionOps *o, char error[256]) {
  Action r = {.s=s, .b=b, .o=o, .input=input, .seconds=seconds, .error=error};
  if (!s || !b || !input || !o || !isfinite(seconds) || seconds < 0)
    return fail(&r, "invalid state/input/time");
  r.c = &b->control; r.f = r.c->frame; r.a = r.c->auxiliary;
  if (!r.f || !r.a || !r.c->control || !r.c->camera || !r.c->substate ||
      !r.c->mode || !r.c->choice || !r.c->reset_c || !r.c->configuration ||
      !r.c->camera_words || !r.c->group_prefix || !r.c->voice_latches ||
      !r.c->inputs || !r.c->processed || !r.c->gauge_y || !r.c->scale ||
      !r.c->plain_scheduled || !r.c->previous_flow || !r.c->voice_volume ||
      !r.c->effect_volume || !r.c->speech_name || !b->controller || !b->presets ||
      b->presets != r.c->presets || !b->selected || b->selected != r.c->selected ||
      !b->face_mode || !b->once || !b->random_latch || !b->reset_a ||
      !b->small_motion || !b->large_motion || !b->fast_motion || !b->reverse ||
      !b->previous_sound || !b->stage || !b->reset_340 || !b->reset_354 ||
      !b->words_318 || !b->group_seen || !b->group_suffix ||
      !b->sequence_elapsed || !b->sequence || !b->animation_scale)
    return fail(&r, "missing or inconsistent shared owners");
  int8_t captured_group;
  memcpy(&captured_group, &r.f->group, 1); r.record_group = captured_group;
  if (s->speech_ready == 1) {
    s->speech_elapsed = (float)((double)s->speech_elapsed + (double)seconds);
    if (s->speech_elapsed > 8.f) { s->speech_ready = 2; s->speech_elapsed = 0; }
  }
  switch (*r.c->mode) {
  case 0: return mode_zero(&r);
  case 1: return mode_one(&r);
  case 2: return mode_two(&r);
  case 3: return mode_three(&r);
  case 4: return mode_four(&r);
  case 5: return mode_five(&r);
  case 6: return mode_six(&r);
  case 7: return mode_seven(&r);
  default: return 1; /*Native unsigned jump-table guard after cooldown.*/
  }
}
#undef CALL
#undef EXTRA
