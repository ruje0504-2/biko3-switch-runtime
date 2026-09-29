#include "game/ending_normal_control.h"
#include "core/random.h"
#include "game/ending_sound.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "normal ending control: %s", why);
  return 0;
}
BkEndingNormalControlState bk_ending_normal_control_initial(void) {
  return (BkEndingNormalControlState){.selected_clip = 1,
      .replay_delay = 20, .manual_choice = -1};
}
static int key(const BkEndingNormalControlOps *o, unsigned code, unsigned mode,
               int *pressed, char e[256]) {
  uint32_t result = 0;
  if (!o->key)
    return fail(e, "missing key service");
  if (!o->key(o->context, code, mode, &result, e))
    return 0;
  *pressed = (result & 255u) != 0;
  return 1;
}
static int playing(const BkEndingNormalControlOps *o, unsigned slot, int *out,
                   char e[256]) {
  int exists = 0;
  if (!o->present)
    return fail(e, "missing speech presence service");
  if (!o->present(o->context, slot, &exists, e))
    return 0;
  *out = 0;
  if (!exists)
    return 1;
  if (!o->status)
    return fail(e, "missing speech status service");
  return o->status(o->context, slot, out, e);
}
static int pause_playing(const BkEndingNormalControlOps *o, unsigned slot,
                         char e[256]) {
  int active;
  if (!playing(o, slot, &active, e))
    return 0;
  if (!active)
    return 1;
  if (!o->pause)
    return fail(e, "missing speech pause service");
  return o->pause(o->context, slot, e);
}
static int load(const BkEndingNormalControlOps *o, unsigned slot,
                const char *name, char e[256]) {
  return o->load ? o->load(o->context, slot, name, e)
                 : fail(e, "missing speech load service");
}
static int play(const BkEndingNormalControlBindings *b,
                const BkEndingNormalControlOps *o, unsigned slot,
                int32_t flags, char e[256]) {
  return o->play ? o->play(o->context, slot, flags, *b->voice_volume, e)
                 : fail(e, "missing speech play service");
}
static int voice(const BkEndingNormalControlBindings *b,
                 const BkEndingNormalControlOps *o, BkEndingNormalVoice kind,
                 int32_t flags, char e[256]) {
  char name[32] = {0};
  if (!o->name)
    return fail(e, "missing original speech selection service");
  if (!o->name(o->context, kind, name, e))
    return 0;
  if (!memchr(name, 0, sizeof(name)))
    return fail(e, "unterminated selected speech name");
  return load(o, 0, name, e) && play(b, o, 0, flags, e);
}
static int fixed_voice(const BkEndingNormalControlBindings *b,
                       const BkEndingNormalControlOps *o, unsigned cue,
                       int cache, int start, char e[256]) {
  char name[32];
  if (b->frame->group >= 5)
    return fail(e, "speech group outside original table");
  snprintf(name, sizeof(name), "PH%u%04u.wav", b->frame->group + 1u, cue);
  if (cache)
    memcpy(b->speech_name, name, strlen(name) + 1);
  return load(o, 0, name, e) && (!start || play(b, o, 0, 0, e));
}
static int expression(const BkEndingNormalControlBindings *b,
                      const BkEndingNormalControlOps *o, int a, int z,
                      char e[256]) {
  /*4dfb96 writes both expression words before the eye-texture call. */
  b->auxiliary->expression_a = a;
  b->auxiliary->expression_b = z;
  return o->eyes ? o->eyes(o->context, 1, e)
                 : fail(e, "missing eye-texture service");
}
static int ordinary_expression(const BkEndingNormalControlBindings *b,
                               const BkEndingNormalControlOps *o, int z,
                               char e[256]) {
  float p = b->auxiliary->progress;
  if (p < .2f || (p >= .19f && p < .4f))
    return expression(b, o, 7, z, e);
  if (p >= .39f)
    return expression(b, o, 3, z, e);
  return 1;
}
static int request(const BkEndingNormalControlOps *o, unsigned actor,
                   int32_t clip, char e[256]) {
  return o->request ? o->request(o->context, actor, clip, e)
                    : fail(e, "missing configured clip request");
}
static int table_word(const BkEndingNormalControlState *s,
                      const BkEndingNormalControlBindings *b, unsigned stride,
                      unsigned offset, int32_t *out, char e[256]) {
  int64_t index = (int64_t)s->action_kind * stride +
                  (int64_t)s->action_column * 5 + offset;
  if (!b->actions || index < 0 || index >= 80)
    return fail(e, "native action table index outside loader table");
  *out = b->actions[index];
  return 1;
}
static int manual_key(BkEndingNormalControlState *s,
                       const BkEndingNormalControlOps *o, unsigned code,
                       int8_t choice, char e[256]) {
  uint32_t raw = 0;
  if (!o->raw_key)
    return fail(e, "missing raw camera-key service");
  if (!o->raw_key(o->context, code, &raw, e))
    return 0;
  if (!(raw & 0x8000u) || (choice != -1 && s->manual_choice == choice))
    return 1;
  s->manual_action = choice == -1 ? 3 : 0;
  s->manual_choice = choice;
  return o->manual_camera ? o->manual_camera(o->context, e)
                          : fail(e, "missing manual camera controller");
}
static int manual_keys(BkEndingNormalControlState *s,
                       const BkEndingNormalControlBindings *b,
                       const BkEndingNormalControlOps *o, char e[256]) {
  if (b->frame->camera_manual != 1)
    return 1;
  const unsigned codes[5] = {0x41, 0x53, 0x44, 0x46, 0x42};
  for (unsigned i = 0; i < 5; ++i)
    if (!manual_key(s, o, codes[i], (int8_t)i, e))
      return 0;
  if (b->frame->group != 1 && b->frame->group != 2) {
    if (!manual_key(s, o, 0x58, 5, e) || !manual_key(s, o, 0x43, 6, e))
      return 0;
  }
  return manual_key(s, o, 0x56, 7, e) && manual_key(s, o, 0x20, -1, e);
}
static int hovered(BkEndingNormalControlState *s,
                    const BkEndingNormalControlBindings *b,
                    const BkEndingNormalControlOps *o, BkEndingRecord *record,
                    char e[256]) {
  BkEndingFrameState *f = b->frame;
  BkEndingAuxiliaryState *a = b->auxiliary;
  int cue = 0;
  if (*b->open)
    return 1;
  if (!b->hover_latches[0]) {
    if (a->progress < .2f)
      cue = 7;
    else if (a->progress >= .19f && a->progress < .4f)
      cue = 8;
    else if (a->progress >= .39f && a->progress < .6f)
      cue = f->camera_cached == 6 ? 11 : 9;
    else if (a->progress >= .59f)
      cue = f->camera_cached == 6 ? 11 : 10;
    if (!ordinary_expression(b, o, 6, e))
      return 0;
    if (s->hover_voice != cue || a->pending != 2) {
      a->pending = 2;
      if (!fixed_voice(b, o, (unsigned)cue, 1, 1, e))
        return 0;
      b->hover_latches[0] = 1;
      s->hover_voice = cue;
    }
  }
  int32_t clip, available;
  if (!table_word(s, b, 25, 1, &clip, e))
    return 0;
  s->selected_clip = clip;
  int pressed;
  if (!key(o, 0, 1, &pressed, e))
    return 0;
  if (!pressed && !key(o, 0x5a, 1, &pressed, e))
    return 0;
  if (!pressed && !key(o, 0x33450, 1, &pressed, e))
    return 0;
  if (!pressed)
    return 1;
  /*4dc495 uses25;4dc48f uses15. They must not be normalized to one row. */
  if (!table_word(s, b, 15, 4, &available, e))
    return 0;
  if (available == -1)
    return 1;
  if (a->progress > .39f && f->camera_cached == 6) {
    if (!fixed_voice(b, o, 260, 1, 1, e))
      return 0;
    a->pending = 13;
    f->state_721ee0 = 5;
    f->camera_mode = 0;
    return expression(b, o, 3, 6, e);
  }
  if (f->camera_cached == 1) {
    if (!request(o, 0, s->selected_clip, e))
      return 0;
    f->state_721ee0 = 3;
    if (!voice(b, o, BK_ENDING_NORMAL_ACTION_NAME, 0, e))
      return 0;
    a->pending = 3;
    a->index = 1;
  } else {
    if (!request(o, 0, s->selected_clip, e))
      return 0;
    if (!o->actor_present)
      return fail(e, "missing auxiliary actor presence service");
    int exists = 0;
    if (!o->actor_present(o->context, 1, &exists, e))
      return 0;
    if (exists) {
      if (!request(o, 1, s->selected_clip, e))
        return 0;
      int flag = !(f->camera_cached == 11 || f->camera_cached == 12 ||
                    (f->group == 1 && f->camera_cached == 10));
      if (!o->root_flag)
        return fail(e, "missing auxiliary root flag service");
      if (!o->root_flag(o->context, 1, flag, e))
        return 0;
    }
    f->state_721ee0 = 3;
    if (!pause_playing(o, 0, e) ||
        !voice(b, o, BK_ENDING_NORMAL_ACTION_NAME, 0, e))
      return 0;
    a->pending = 3;
  }
  if (!ordinary_expression(b, o, 6, e))
    return 0;
  f->camera_mode = 4;
  return bk_ending_record_normal_choice(record, *b->previous_flow,
                                         f->camera_cached, b->normal_target, e);
}
static int not_hovered(BkEndingNormalControlState *s,
                       const BkEndingNormalControlBindings *b,
                       const BkEndingNormalControlOps *o, char e[256]) {
  (void)s;
  BkEndingAuxiliaryState *a = b->auxiliary;
  int pressed, active;
  if (!key(o, 0, 2, &pressed, e))
    return 0;
  if (!pressed && !key(o, 1, 2, &pressed, e))
    return 0;
  if (!pressed && !key(o, 0x5a, 1, &pressed, e))
    return 0;
  if (!pressed && !key(o, 0x33450, 1, &pressed, e))
    return 0;
  if (pressed) {
    b->frame->state_721ee0 = 2;
    return 1;
  }
  b->hover_latches[0] = 0;
  if (a->pending == 1)
    return 1;
  if (a->progress < .2f && a->pending != 0) {
    if (!playing(o, 0, &active, e))
      return 0;
    if (!active) {
      if (!fixed_voice(b, o, 1, 1, 0, e))
        return 0;
      a->pending = 0;
    }
    return 1;
  }
  if (!(a->progress >= .2f))
    return 1;
  if (a->pending == 10 || a->pending == 11) {
    if (!playing(o, 0, &active, e))
      return 0;
    if (!active) {
      char name[32];
      if (!bk_ending_sound_contact_voice(b->frame->group, 0,
                                          a->pending == 10 ? 1 : 2, 0,
                                          name, e) || !load(o, 1, name, e))
        return 0;
      if (b->control->toggles[7] && !play(b, o, 1, 0, e))
        return 0;
      a->pending = 9;
    }
    return 1;
  }
  if (a->pending == 9) {
    if (!playing(o, 1, &active, e))
      return 0;
    if (!active)
      a->pending = 1;
    return 1;
  }
  if (!playing(o, 0, &active, e))
    return 0;
  if (!active) {
    if (!voice(b, o, BK_ENDING_NORMAL_LOOP_NAME, 1, e))
      return 0;
    a->pending = 1;
  }
  return 1;
}
static int idle(BkEndingNormalControlState *s,
                 const BkEndingNormalControlBindings *b,
                 const BkEndingFrameInput *input, float seconds,
                 const BkEndingNormalControlOps *o, BkEndingRecord *record,
                 char e[256]) {
  BkEndingFrameState *f = b->frame;
  BkEndingAuxiliaryState *a = b->auxiliary;
  if (a->progress < .2f) {
    if (!a->pending) {
      s->replay_elapsed = (float)((double)s->replay_elapsed + seconds);
      if ((double)s->replay_delay <= s->replay_elapsed) {
        if (!play(b, o, 0, 0, e))
          return 0;
        s->replay_elapsed = 0;
        if (!b->random)
          return fail(e, "missing process random state");
        s->replay_delay = bk_random_next(b->random) % 21 + 30;
      }
    }
  } else if (a->progress >= .39f && !b->hover_latches[1]) {
    if (!fixed_voice(b, o, 3, 0, 1, e))
      return 0;
    b->hover_latches[1] = 1;
    a->pending = 10;
  } else if ((f->group != 1 && a->progress >= .79f && !b->hover_latches[2]) ||
             ((f->group == 1 || f->group == 2) && a->progress >= .71f &&
              !b->hover_latches[2])) {
    if (!fixed_voice(b, o, 5, 0, 1, e))
      return 0;
    b->hover_latches[2] = 1;
    a->pending = 11;
  }
  if (*b->previous_flow != 0x18 && a->progress > .49f && !*b->next_mode &&
      a->pending != 10 && a->pending != 9) {
    if (!pause_playing(o, 1, e) || !fixed_voice(b, o, 261, 1, 1, e))
      return 0;
    a->pending = 12;
    f->state_721ee0 = 5;
    if (f->group == 0 || f->group == 1)
      return expression(b, o, 3, 6, e);
    if (f->group == 3 || f->group == 1)
      return expression(b, o, 8, 1, e);
    if (f->group == 4)
      return expression(b, o, 6, 6, e);
    return 1;
  }
  int32_t hit = 0;
  if (a->pending != 10 && a->pending != 11 && a->pending != 9 &&
      f->camera_mode != 1 && !*b->voice_wait) {
    int active;
    if (!playing(o, 1, &active, e))
      return 0;
    if (!active) {
      if (!o->target)
        return fail(e, "missing actual target selection service");
      float pointer[2];
      for (unsigned i = 0; i < 2; ++i) {
        int32_t coordinate;
        memcpy(&coordinate, &input->words[9 + i], sizeof(coordinate));
        pointer[i] = (float)coordinate;
      }
      if (!o->target(o->context, pointer, &hit, e))
        return 0;
    }
  }
  if (!manual_keys(s, b, o, e))
    return 0;
  return hit ? hovered(s, b, o, record, e) : not_hovered(s, b, o, e);
}
static int finish(BkEndingNormalControlState *s,
                   const BkEndingNormalControlBindings *b,
                   const BkEndingNormalControlOps *o, BkEndingRecord *record,
                   char e[256]) {
  BkEndingFrameState *f = b->frame;
  BkEndingAuxiliaryState *a = b->auxiliary;
  int active;
  if (a->pending == 12) {
    if (!playing(o, 0, &active, e))
      return 0;
    if (!active) {
      f->transition_action = 6;
      f->curtain_wanted = 1;
      *b->normal_ready = *b->substate = 0;
      if (*b->previous_flow == 8 &&
          !bk_ending_record_append_unique(record, 10, e))
        return 0;
    }
    return 1;
  }
  if (a->pending != 13)
    return 1;
  if (!playing(o, 0, &active, e))
    return 0;
  if (active)
    return 1;
  if (*b->previous_flow == 0x18)
    f->transition_action = 49;
  else if (*b->previous_flow == 8) {
    f->transition_action = 74;
    if (f->group < 5) {
      const int32_t selection[5] = {0, 2, 1, 0, 1};
      a->selection = selection[f->group];
      if (!bk_ending_record_append_unique(record, 12 + a->selection, e))
        return 0;
    }
  }
  memset(b->hover_latches, 0, 3 * sizeof(*b->hover_latches));
  f->curtain_wanted = 1;
  *b->normal_ready = *b->substate = 0;
  s->hover_voice = 0;
  s->selected_clip = 1;
  s->replay_delay = 20;
  s->replay_elapsed = 0;
  memset(b->processed, 0, 14 * sizeof(*b->processed));
  memset(b->inputs, 0, 14 * sizeof(*b->inputs));
  *b->voice_wait = 0;
  return 1;
}
int bk_ending_normal_control_step(BkEndingNormalControlState *s,
                                  const BkEndingNormalControlBindings *b,
                                  const BkEndingFrameInput *input,
                                  float seconds,
                                  const BkEndingNormalControlOps *o,
                                  char e[256]) {
  if (!s || !b || !b->frame || !b->control || !b->auxiliary || !b->substate ||
      !b->voice_wait || !b->hover_latches || !b->normal_ready ||
      !b->normal_target || !b->inputs || !b->processed || !b->next_mode ||
      !b->open || !b->voice_volume || !b->previous_flow || !b->speech_name ||
      !input || !o || !isfinite(seconds) || seconds < 0 || b->frame->group >= 5)
    return fail(e, "invalid parent bindings, group or time");
  /*Native saves the record pointer before any child may change group. */
  BkEndingRecord *record = b->records ? &b->records->groups[b->frame->group]
                                      : NULL;
  switch (b->frame->state_721ee0) {
  case 0:
    if (b->auxiliary->progress >= .2f) {
      if (!voice(b, o, BK_ENDING_NORMAL_LOOP_NAME, 1, e))
        return 0;
      b->auxiliary->pending = 1;
    }
    b->frame->state_721ee0 = 1;
    return ordinary_expression(b, o, 3, e);
  case 1:
    return idle(s, b, input, seconds, o, record, e);
  case 2: {
    int pressed;
    if (!key(o, 0, 0, &pressed, e))
      return 0;
    if (pressed) {
      if (!key(o, 1, 0, &pressed, e))
        return 0;
      if (pressed)
        b->frame->state_721ee0 = 1;
    }
    return 1;
  }
  case 3:
    return o->action ? o->action(o->context, s->selected_clip, input, e)
                      : fail(e, "missing actual normal action controller");
  case 4:
    return o->opening ? o->opening(o->context, seconds, e)
                       : fail(e, "missing actual opening controller");
  case 5:
    return finish(s, b, o, record, e);
  default:
    return 1;
  }
}
