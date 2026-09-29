#include "game/ending_selected_control.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game/ending_selected_control_tables.inc"

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "selected controller: %s", why);
  return 0;
}
BkEndingSelectedControlState bk_ending_selected_control_initial(void) {
  return (BkEndingSelectedControlState){.fov = 1};
}
static void index_set(BkEndingAuxiliaryState *s) {
  uint32_t value = (uint32_t)s->base + (uint32_t)s->selection * 6u;
  memcpy(&s->index, &value, 4);
}
static float input_coordinate(uint32_t word) {
  int32_t signed_value;
  memcpy(&signed_value, &word, sizeof(signed_value));
  return (float)signed_value; /*Native FILD reads signed32-bit pointer words.*/
}
static int audio(const BkEndingSelectedControlOps *o, BkEndingAudioOperation op,
                   unsigned slot, int32_t cue, int32_t bank, int32_t flags,
                   int32_t volume, int *playing, char e[256]) {
  BkEndingAudioCall call = {op, slot, cue, bank, flags, volume};
  return o->audio ? o->audio(o->context, &call, playing, e)
                  : fail(e, "missing audio service");
}
static int setup_camera(const BkEndingSelectedControlBindings *b,
                           const BkEndingSelectedControlOps *o, char e[256]) {
  int32_t selected = b->frame->camera_clip;
  if ((uint32_t)selected >= 3) return fail(e, "camera preset outside three choices");
  b->camera->yaw = b->presets->active[0][selected];
  b->camera->pitch = b->presets->active[1][selected];
  b->camera->radius = b->presets->active[2][selected];
  b->camera->height = b->presets->active[3][selected];
  static const unsigned nodes[] = {5, 13, 0};
  if ((uint32_t)b->control->target_choice >= 3)
    return fail(e, "camera target outside native5/13/0 table");
  if (!o->target) return fail(e, "missing target service");
  float target[3];
  if (!o->target(o->context, nodes[b->control->target_choice], target, e)) return 0;
  memcpy(b->frame->camera_values, target, 12);
  return 1;
}
static int preset(const BkEndingSelectedControlBindings *b,
                     const BkEndingSelectedControlOps *o, uint32_t *done,
                     char e[256]) {
  if (!o->camera) return fail(e, "missing camera service");
  if (b->frame->group >= 5) return fail(e, "camera group outside five rows");
  uint32_t offset[3];
  memcpy(offset, b->frame->camera_values, 12);
  return o->camera(o->context, BK_ENDING_OPENING_PRESET, b->frame->camera_clip,
                   offset, b->frame->camera_table[b->frame->group][3], done, e);
}
static int start_voice(const BkEndingSelectedControlBindings *b,
                          const BkEndingSelectedControlOps *o, char e[256]) {
  if (b->frame->group >= 5) return fail(e, "voice group outside five entries");
  char name[32];
  snprintf(name, sizeof(name), "PH%u0103.wav", (unsigned)b->frame->group + 1);
  memcpy(b->speech_name, name, strlen(name) + 1);
  if (!o->load) return fail(e, "missing speech load service");
  if (!o->load(o->context, 0, name, e)) return 0;
  int playing;
  return audio(o, BK_ENDING_AUDIO_RESTART, 0, 0, 0, 1,
                 *b->voice_volume, &playing, e);
}
static int key(const BkEndingSelectedControlOps *o, unsigned code,
                  unsigned mode, int *pressed, char e[256]) {
  uint32_t value;
  if (!o->key) return fail(e, "missing key service");
  if (!o->key(o->context, code, mode, &value, e)) return 0;
  *pressed = (value & 255u) != 0;
  return 1;
}
static int choose_key(const BkEndingSelectedControlOps *o, int *pressed,
                         char e[256]) {
  return key(o, 0, 1, pressed, e) && (*pressed ||
      (key(o, 0x5a, 1, pressed, e) && (*pressed ||
       key(o, 0x33450, 1, pressed, e))));
}
int bk_ending_selected_control_step(BkEndingSelectedControlState *s,
    const BkEndingSelectedControlBindings *b, const BkEndingFrameInput *input,
    float seconds, const BkEndingSelectedControlOps *o, char e[256]) {
  if (!s || !b || !b->frame || !b->control || !b->auxiliary || !b->camera ||
      !b->presets || !b->substate || !b->mode || !b->choice || !b->reset_c ||
      !b->configuration || !b->camera_words || !b->group_prefix ||
      !b->voice_latches || !b->inputs || !b->processed || !b->selected ||
      !b->open || !b->gauge_y || !b->scale || !b->plain_scheduled ||
      !b->previous_flow || !b->targets || !b->alternate || !b->voice_volume ||
      !b->effect_volume || !b->speech_name || !input || !o ||
      !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid live bindings/time");
  BkEndingFrameState *f = b->frame;
  BkEndingAuxiliaryState *a = b->auxiliary;
  a->base = a->variant ? 90 : 36;
#define CALL(member, ...) do { \
  if (!o->member) return fail(e, "missing " #member " service"); \
  if (!o->member(o->context, __VA_ARGS__)) return 0; \
} while (0)
  if (a->gate == 4) {
    switch (*b->substate) {
    case 0:
      *b->substate = 1;
      *b->mode = 0;
      s->replay_elapsed = 0;
      memset(b->configuration, 0, 120);
      memset(b->camera_words, 0, 300);
      memcpy(b->configuration, selected_configuration[a->variant != 0], 120);
      memcpy(b->camera_words, selected_camera_words[a->variant != 0], 300);
      s->replay_after = a->variant ? 30 : 10;
      s->fov = 1;
      CALL(expression, 6, 3, 1, e);
      index_set(a);
      break;
    case 1:
      if (*b->selected == 2 || *b->selected == 3 || *b->selected == 4) {
        *b->substate = 3;
      } else if (*b->previous_flow == 0x18) {
        b->camera->fov = s->fov;
        if (seconds < 1.f) s->fov = (float)((double)s->fov - (double)seconds * .2f);
        if (!(s->fov > .2f)) s->fov = .2f;
        uint32_t done;
        CALL(camera, BK_ENDING_OPENING_TRACK, 0, ((uint32_t[3]){0,0,0}), 0, &done, e);
        if (done & 255u) {
          b->camera->fov = .2f;
          s->fov = 1;
          *b->substate = 2;
          memcpy(b->control->saved_camera, b->camera->matrix, 64);
          if (!setup_camera(b, o, e)) return 0;
        }
      } else if (*b->previous_flow == 8) *b->substate = 3;
      break;
    case 2: {
      uint32_t done;
      if (!preset(b, o, &done, e)) return 0;
      if (done & 255u) {
        *b->substate = 0;
        f->camera_mode = 0;
        a->gate = 1;
        if (!start_voice(b, o, e)) return 0;
      }
      break;
    }
    case 3: {
      if (!setup_camera(b, o, e)) return 0;
      b->camera->fov = s->fov;
      if (seconds < 1.f) s->fov = (float)((double)s->fov - (double)seconds * .2f);
      int fov_done = !(s->fov > .2f);
      if (fov_done) s->fov = .2f;
      uint32_t done;
      if (!preset(b, o, &done, e)) return 0;
      if (fov_done && (done & 255u)) {
        s->fov = 1;
        b->camera->fov = .2f;
        if (!start_voice(b, o, e)) return 0;
        *b->substate = 0;
        f->camera_mode = 0;
        a->gate = 1;
      }
      break;
    }
    }
    return 1;
  }
  if (a->gate == 3) {
    CALL(action, input, seconds, e);
    return 1;
  }
  if (a->gate == 2) {
    int released;
    if (!key(o, 0, 0, &released, e)) return 0;
    if (released) {
      if (!key(o, 1, 0, &released, e)) return 0;
      if (released) a->gate = 1;
    }
    return 1;
  }
  if (a->gate != 1) return 1;
  int playing;
  if (a->pending == 2) {
    if (!audio(o, BK_ENDING_AUDIO_STATUS, 0, 0, 0, 0, 0, &playing, e)) return 0;
    if (!playing) {
      if (b->control->toggles[7] && !audio(o, BK_ENDING_AUDIO_VOICE, 1, 3, 0, 0,
                  *b->voice_volume, &playing, e)) return 0;
      if (f->group >= 5) return fail(e, "group prefix outside five rows");
      b->group_prefix[f->group][0] = 1;
      a->pending = 0;
    }
  }
  if (!audio(o, BK_ENDING_AUDIO_STATUS, 0, 0, 0, 0, 0, &playing, e)) return 0;
  if (!playing) {
    if (f->group >= 5) return fail(e, "voice latch outside five rows");
    if (!b->voice_latches[f->group * 4]) {
      int32_t active;
      CALL(active, &active, e);
      if (active == 4) {
        a->pending = 1;
        CALL(expression, 6, 3, 1, e);
        unsigned band = !(a->progress >= .6f) ? 0 : a->progress < .8f ? 1 : 2;
        int32_t cue;
        if (!bk_ending_selected_cue((unsigned)a->variant, 3 + band, &cue))
          return fail(e, "voice cue outside native two rows");
        if (!audio(o, BK_ENDING_AUDIO_CUE, 0, cue, 2, 0, *b->voice_volume, &playing, e)) return 0;
        if (f->group >= 5) return fail(e, "voice latch outside five rows");
        b->voice_latches[f->group * 4] = 1;
      }
    }
  }
  if (a->pending == 1 && !f->auxiliary_mode) {
    s->replay_elapsed = (float)((double)s->replay_elapsed + (double)seconds);
    if (!((double)s->replay_after > (double)s->replay_elapsed)) {
      if (!audio(o, BK_ENDING_AUDIO_RESTART, 0, 0, 0, 0, *b->voice_volume, &playing, e)) return 0;
      s->replay_elapsed = 0;
      int alternate = a->variant != 0;
      int32_t random;
      CALL(random, &random, e);
      s->replay_after = alternate ? random % 11 + 20 : random % 6 + 5;
    }
  }
  if (*b->mode == 1 || *b->mode == 2) {
    int32_t active;
    CALL(active, &active, e);
    if (active == 4 && !input->words[7] && !input->words[6] &&
        !f->auxiliary_mode && !f->camera_mode) {
      a->progress = (float)((double)a->progress - (double).01f * (double)seconds * .1f);
      if (!(a->progress > .4f)) a->progress = .4f;
      else *b->gauge_y = (float)(2.0 * (double)*b->scale * (double)seconds * .1f + (double)*b->gauge_y);
    }
  }
  if (f->camera_manual == 1) {
    uint32_t key_bits;
    int32_t accepted;
    CALL(raw_key, 0x43, &key_bits, e);
    if (key_bits & 0x8000u) {
      CALL(manual, 3, &accepted, e);
      if (accepted) s->manual_mode = 3;
    }
    CALL(raw_key, 0x56, &key_bits, e);
    if ((key_bits & 0x8000u) && s->manual_mode == 3) {
      a->gate = 3;
      *b->mode = 4;
      *b->reset_c = 11;
    }
    CALL(raw_key, 0x20, &key_bits, e);
    if (key_bits & 0x8000u) {
      CALL(manual, 0, &accepted, e);
      s->manual_mode = 0;
    }
  }
  if (!audio(o, BK_ENDING_AUDIO_STATUS, 1, 0, 0, 0, 0, &playing, e)) return 0;
  int32_t picked = 0;
  if (!playing && f->camera_mode != 1 && a->pending != 2 && !f->auxiliary_mode) {
    float pointer[2] = {input_coordinate(input->words[9]),
                       input_coordinate(input->words[10])};
    CALL(pick, pointer, &picked, e);
  }
  int pressed;
  if (picked == 1) {
    if (!*b->open) {
      if (!choose_key(o, &pressed, e)) return 0;
      if (pressed) {
        if ((uint32_t)f->camera_cached >= 39) return fail(e, "selected target outside39 nodes");
        int32_t point[2], selected;
        memcpy(point, b->targets[f->camera_cached], 8);
        CALL(choose, point, &selected, e);
        if (selected != -1) {
          CALL(expression, 6, 6, 1, e);
          a->gate = 3;
          *b->mode = 3;
          CALL(begin, e);
          f->camera_mode = 4;
          a->pending = 0;
        }
      }
    }
  } else if (picked == 2) {
    if (!*b->open) {
      if (!key(o, 0, 1, &pressed, e)) return 0;
      if (pressed) {
        int32_t point[2], selected;
        memcpy(point, b->alternate, 8);
        CALL(choose, point, &selected, e);
        if (selected != -1) {
          if (f->group >= 5) return fail(e, "voice latch outside five rows");
          b->voice_latches[f->group * 4] = 0;
          a->gate = 3;
          int32_t active;
          CALL(active, &active, e);
          if (active == 1) {
            int ordinary = !a->variant || f->group == 0 || f->group == 1;
            CALL(expression, ordinary ? 6 : 5, ordinary ? 6 : 4, 1, e);
            CALL(begin, e);
            CALL(request, 2, e);
            *b->mode = 0;
          } else {
            CALL(expression, 6, 4, 1, e);
            if (!audio(o, BK_ENDING_AUDIO_RESTART, 5, 0, 0, 1, *b->effect_volume, &playing, e)) return 0;
            *b->choice = 0;
            if (!*b->plain_scheduled) *b->mode = 1;
            else {
              *b->mode = 2;
              CALL(request, 5, e);
              CALL(write, 5, BK_ENDING_CLIP_CHAIN, 1, e);
              CALL(write, 6, BK_ENDING_CLIP_NEXT, 7, e);
              CALL(write, 7, BK_ENDING_CLIP_NEXT, 6, e);
              CALL(write, 10, BK_ENDING_CLIP_NEXT, 11, e);
              CALL(write, 11, BK_ENDING_CLIP_NEXT, 10, e);
              *b->choice = 0;
              b->inputs[0] = 1;
              b->inputs[1] = 0;
              b->processed[1] = 0;
            }
          }
          f->camera_mode = 4;
          a->pending = 0;
        }
      }
    }
  } else {
    if (!key(o, 0, 2, &pressed, e)) return 0;
    if (!pressed && !key(o, 1, 2, &pressed, e)) return 0;
    if (!pressed && !key(o, 0x5a, 1, &pressed, e)) return 0;
    if (!pressed && !key(o, 0x33450, 1, &pressed, e)) return 0;
    if (pressed) a->gate = 2;
  }
#undef CALL
  return 1;
}
