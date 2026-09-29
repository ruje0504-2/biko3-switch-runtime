#include "game/ending_tertiary_control.h"
#include "core/random.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

BkEndingTertiaryControlState bk_ending_tertiary_control_initial(void) {
  BkEndingTertiaryControlState s = {0};
  s.replay_after = 30;
  return s;
}
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "tertiary ending control: %s", why);
  return 0;
}
#define CALL(name, ...) \
  (o->name ? o->name(o->context, __VA_ARGS__) : fail(e, "missing " #name " service"))

static int key(const BkEndingTertiaryControlOps *o, unsigned code,
               unsigned mode, int *pressed, char e[256]) {
  uint32_t value;
  if (!CALL(key, code, mode, &value, e)) return 0;
  *pressed = (value & 255u) != 0;
  return 1;
}
static int playing(const BkEndingTertiaryControlOps *o, unsigned slot,
                   int *busy, char e[256]) {
  int exists;
  if (!CALL(present, slot, &exists, e)) return 0;
  if (!exists) { *busy = 0; return 1; }
  return CALL(status, slot, busy, e);
}
static int voice(const BkEndingTertiaryControlBindings *b,
                 const BkEndingTertiaryControlOps *o, int cue, unsigned slot,
                 int select, char e[256]) {
  return CALL(voice, cue, slot, 0, select, e) &&
         CALL(play, slot, *b->voice_volume, e);
}
static int special(const BkEndingTertiaryControlBindings *b) {
  return b->frame->group == 2 || b->frame->group == 4;
}
static int initial_expression(const BkEndingTertiaryControlBindings *b,
                              const BkEndingTertiaryControlOps *o,
                              char e[256]) {
  if (special(b)) return CALL(expression, 7, 2, 0, e);
  if (b->frame->group <= 1) return CALL(expression, 4, 4, 0, e);
  return CALL(expression, 7, 3, 1, e);
}
static int target(const BkEndingTertiaryControlBindings *b,
                  const BkEndingTertiaryControlOps *o, char e[256]) {
  float position[3];
  if (!CALL(target, 5, position, e)) return 0;
  memcpy(b->frame->camera_values, position, sizeof(position));
  return 1;
}
static int camera(const BkEndingTertiaryControlBindings *b,
                  const BkEndingTertiaryControlOps *o,
                  BkEndingOpeningCamera mode, int *done, char e[256]) {
  if (b->frame->group >= 5) return fail(e, "camera group outside table");
  uint32_t result;
  if (!CALL(camera, mode, b->frame->camera_clip, b->frame->camera_values,
            b->frame->camera_table[b->frame->group][3], &result, e)) return 0;
  *done = (result & 255u) != 0;
  return 1;
}
static int opening(const BkEndingTertiaryControlBindings *b, float seconds,
                   const BkEndingTertiaryControlOps *o, char e[256]) {
  int done, busy, exists;
  switch ((int8_t)*b->substate) {
  case 0:
    if (!initial_expression(b,o,e)) return 0;
    b->auxiliary->index = 54;
    b->camera->fov = *b->fov;
    if (seconds < 1.f)
      *b->fov = (float)((double)*b->fov - (double)seconds * .2f);
    if (!(*b->fov > .2f)) *b->fov = .2f;
    if (!camera(b,o,BK_ENDING_OPENING_TRACK,&done,e)) return 0;
    if (done) {
      b->camera->fov = .2f;
      *b->fov = 1.f;
      *b->substate = 1;
      memcpy(b->control->saved_camera,b->camera->matrix,64);
      if (!target(b,o,e)) return 0;
    }
    break;
  case 1:
    if (!camera(b,o,BK_ENDING_OPENING_PRESET,&done,e)) return 0;
    if (done) {
      *b->substate = 2;
      if (!voice(b,o,1,0,1,e)) return 0;
      b->frame->camera_mode = 0;
    }
    break;
  case 2:
    if (!playing(o,0,&busy,e)) return 0;
    if (!busy) {
      if (!CALL(present,1,&exists,e)) return 0;
      if (!exists && b->control->toggles[7] && !voice(b,o,2,1,1,e)) return 0;
      *b->substate = 3;
    }
    break;
  case 3:
    if (!playing(o,1,&busy,e)) return 0;
    if (!busy) {
      *b->state = 0;
      *b->substate = 0;
      b->camera->fov = .2f;
    }
    break;
  }
  return 1;
}
static int initial(const BkEndingTertiaryControlBindings *b,
                   const BkEndingTertiaryControlOps *o, char e[256]) {
  int busy, clip;
  if (!playing(o,0,&busy,e)) return 0;
  if (!busy) {
    float progress = b->auxiliary->progress;
    /* The native test uses x87 C0 alone: unordered follows the low branch. */
    int cue = !(progress >= .2f) ? 1 : (progress >= .2f && progress < .4f ? 2 :
                                  (progress >= .39f ? 3 : 0));
    if (cue && !CALL(voice,cue,0,0,0,e)) return 0;
    if (!CALL(play,0,*b->voice_volume,e)) return 0;
    b->auxiliary->pending = 1;
  }
  if (special(b) || b->frame->group <= 1) {
    if (!CALL(active,&clip,e)) return 0;
    if (clip == 4) {
      if (!CALL(expression,7,6,1,e)) return 0;
    } else if (!initial_expression(b,o,e)) return 0;
  } else if (!(b->auxiliary->progress >= .2f)) {
    if (!CALL(expression,7,3,1,e)) return 0;
    b->voice_latches[2] = 0;
  } else if (b->auxiliary->progress >= .19f) {
    if (!CALL(expression,5,4,0,e)) return 0;
    *b->expression_override = 5;
    b->voice_latches[2] = 0;
    *b->face_mode = 1;
  }
  *b->state = 1;
  return 1;
}
static int appearance(const BkEndingTertiaryControlBindings *b,
                       const BkEndingTertiaryControlOps *o,
                       BkEndingTertiaryAppearance table, unsigned slot,
                       int hidden, char e[256]) {
  if (b->frame->group >= 5) return fail(e, "appearance group outside table");
  return CALL(appearance,table,b->frame->group,slot,hidden,e);
}
static int appearance_three(const BkEndingTertiaryControlBindings *b,
                             const BkEndingTertiaryControlOps *o,
                             unsigned start, int a, int z, char e[256]) {
  return appearance(b,o,BK_ENDING_TERTIARY_APPEARANCE_SIX,start,a,e) &&
         appearance(b,o,BK_ENDING_TERTIARY_APPEARANCE_SIX,start+1,z,e) &&
         appearance(b,o,BK_ENDING_TERTIARY_APPEARANCE_SIX,start+2,1,e);
}
static int effects(const BkEndingTertiaryControlBindings *b,
                   const BkEndingTertiaryControlOps *o, char e[256]) {
  int clip;
  if (!CALL(active,&clip,e)) return 0;
  if (clip == 3 && b->frame->group == 0) {
    b->frame->camera_mode = 2;
    if (!target(b,o,e)) return 0;
    b->control->target_choice = 0;
  }
  if (!CALL(active,&clip,e)) return 0;
  if (clip == 4) {
    if (*b->pending_effect == 0) {
      if (b->frame->group == 1 &&
          (!appearance(b,o,BK_ENDING_TERTIARY_APPEARANCE_PAIR,0,1,e) ||
           !appearance(b,o,BK_ENDING_TERTIARY_APPEARANCE_PAIR,1,1,e))) return 0;
      *b->pending_effect = -1;
    } else if (*b->pending_effect == 13) {
      if (b->frame->group == 1 && !appearance_three(b,o,0,0,1,e)) return 0;
      *b->pending_effect = -1;
    } else if (*b->pending_effect == 5) {
      if (b->frame->group == 1 && !appearance_three(b,o,3,1,0,e)) return 0;
      *b->pending_effect = -1;
    }
  }
  if (b->frame->group == 2) {
    if (!CALL(active,&clip,e)) return 0;
    if (clip == 10) {
      float source;
      if (!CALL(source,10,&source,e)) return 0;
      if (source > 370.f && *b->pending_effect == 5) {
        if (!CALL(effect,*b->effect_volume,e)) return 0;
        *b->pending_effect = -1;
      }
    }
  }
  return 1;
}
static int speech(BkEndingTertiaryControlState *s,
                  const BkEndingTertiaryControlBindings *b, float seconds,
                  const BkEndingTertiaryControlOps *o, char e[256]) {
  int clip;
  if (b->auxiliary->pending == 2) {
    int chance = bk_random_next(b->random)%1000u <= 25u;
    if (!b->voice_latches[0]) {
      if (b->control->toggles[7]) {
        if (!voice(b,o,6,1,0,e)) return 0;
        b->voice_latches[0] = 1;
        b->auxiliary->pending = -1;
      }
    } else if (chance && b->control->toggles[7]) {
      if (!voice(b,o,6,1,0,e)) return 0;
      b->auxiliary->pending = -1;
    }
  } else if (b->auxiliary->pending == 3 || b->auxiliary->pending == 4) {
    int cue = b->auxiliary->pending == 3 ? 9 : 12;
    if (!CALL(active,&clip,e)) return 0;
    if (clip == 4 && b->control->toggles[7]) {
      if (!voice(b,o,cue,1,0,e)) return 0;
      b->auxiliary->pending = -1;
    }
  }
  if (b->auxiliary->pending == 1) {
    s->replay_elapsed += seconds;
    if (!(s->replay_after > (double)s->replay_elapsed)) {
      if (!CALL(play,0,*b->voice_volume,e)) return 0;
      s->replay_elapsed = 0;
      s->replay_after = (int32_t)(bk_random_next(b->random)%21u)+30;
    }
  }
  return 1;
}
static int hover(BkEndingTertiaryControlState *s,
                 const BkEndingTertiaryControlBindings *b,
                 const BkEndingTertiaryControlOps *o, char e[256]) {
  int cue, assigned = 0;
  if (b->frame->camera_cached == b->actions[0]) { cue = 5; assigned = 1; }
  else if (b->frame->camera_cached == b->actions[5]) {
    if (!b->unavailable[0]) { cue = 6; assigned = 1; }
  } else if (b->frame->camera_cached == b->actions[10]) {
    if (!b->unavailable[1]) { cue = 7; assigned = 1; }
  } else if (b->frame->camera_cached == 6 && b->auxiliary->progress >= .39f) {
    cue = 8; assigned = 1;
  }
  if (!assigned) return fail(e, "native hover cue reads an uninitialized local");
  *b->face_mode = 0;
  b->voice_latches[2] = 0;
  if (special(b)) {
    if (!CALL(expression,7,2,1,e)) return 0;
  } else if (cue == 8) {
    if (!CALL(expression,0,4,0,e)) return 0;
  } else if (!CALL(expression,7,6,1,e)) return 0;
  if (s->last_cue != cue || !b->voice_latches[1]) {
    int busy;
    if (!playing(o,0,&busy,e)) return 0;
    if (!busy) {
      if (!voice(b,o,cue,0,1,e)) return 0;
      s->last_cue = cue;
      b->auxiliary->pending = -1;
      b->voice_latches[1] = 1;
    }
  }
  return 1;
}
static int record(BkEndingRecord *r, int32_t action, char e[256]) {
  if (r->count < 0 || r->count >= BK_ENDING_RECORD_CAPACITY)
    return fail(e, "native record write outside action lane");
  r->actions[r->count++] = action;
  return 1;
}
static int confirm(const BkEndingTertiaryControlOps *o, int *pressed,
                   char e[256]) {
  const unsigned codes[] = {0,0x5a,0x33450};
  for (unsigned i=0;i<3;++i) {
    if (!key(o,codes[i],1,pressed,e)) return 0;
    if (*pressed) return 1;
  }
  return 1;
}
static int chosen(const BkEndingTertiaryControlBindings *b,
                  BkEndingRecord *r, const BkEndingTertiaryControlOps *o,
                  int *exiting, char e[256]) {
  int clip, result, changed = 0;
  if (b->frame->camera_cached < 0 || b->frame->camera_cached >= 39)
    return fail(e, "projected target outside table");
  if (!CALL(choose,b->targets[b->frame->camera_cached],&result,e)) return 0;
  if (result == -1) return 1;
  if (!CALL(active,&clip,e)) return 0;
  if (b->frame->group >= 5) return fail(e, "initial target group outside table");
  if (clip == 1 && b->frame->camera_cached == b->initial_targets[b->frame->group]) {
    if (!voice(b,o,3,0,1,e) || !CALL(request,0,2,e)) return 0;
    *b->part_mode = *b->face_mode = b->voice_latches[2] = 0;
    if (!CALL(expression,5,special(b)?2:4,0,e)) return 0;
    b->auxiliary->index = 56;
    changed = 1;
  } else {
    if (!CALL(active,&clip,e)) return 0;
    if (clip == 4) {
      int choice = b->frame->camera_cached;
      if (choice == b->actions[0]) {
        if (!voice(b,o,4,0,0,e) || !CALL(request,0,5,e)) return 0;
        *b->part_mode = *b->face_mode = b->voice_latches[2] = 0;
        if (!CALL(expression,special(b)?5:0,special(b)?2:4,0,e)) return 0;
        b->auxiliary->index = 57;
        changed = 1;
        if (*b->previous_flow == 8 && !record(r,15,e)) return 0;
      } else if (choice == b->actions[5] && !b->unavailable[0]) {
        if (!voice(b,o,7,0,0,e) || !CALL(request,0,7,e)) return 0;
        if (b->frame->group == 2 &&
            (!CALL(hidden,1,0,e) || !CALL(request,1,7,e))) return 0;
        if (b->frame->group == 2) {
          if (!appearance(b,o,BK_ENDING_TERTIARY_APPEARANCE_SIX,0,0,e)) return 0;
        } else if (b->frame->group == 3) {
          if (!appearance_three(b,o,0,0,1,e)) return 0;
        } else if (!appearance_three(b,o,0,1,0,e)) return 0;
        *b->part_mode = 1;
        *b->face_mode = b->voice_latches[2] = 0;
        if (!CALL(expression,5,special(b)?2:3,1,e)) return 0;
        b->auxiliary->index = 58;
        changed = 1;
        if (*b->previous_flow == 8 && !record(r,16,e)) return 0;
      } else if (choice == b->actions[10] && !b->unavailable[1]) {
        if (!voice(b,o,10,0,0,e) || !CALL(request,0,9,e)) return 0;
        if (b->frame->group == 2 &&
            (!CALL(hidden,2,0,e) || !CALL(request,2,9,e))) return 0;
        if (b->frame->group == 2) {
          if (!appearance(b,o,BK_ENDING_TERTIARY_APPEARANCE_SIX,3,0,e)) return 0;
        } else if (!appearance_three(b,o,3,0,1,e)) return 0;
        *b->part_mode = 1;
        *b->face_mode = b->voice_latches[2] = 0;
        if (!CALL(expression,5,special(b)?2:3,1,e)) return 0;
        b->auxiliary->index = 59;
        changed = 1;
        if (*b->previous_flow == 8 && !record(r,17,e)) return 0;
      } else if (b->auxiliary->progress >= .39f && choice == 6) {
        *b->state = 5;
        b->voice_latches[0] = 0;
        if (*b->finish_setting == 1) {
          if (!voice(b,o,13,0,0,e)) return 0;
          if (special(b)) {
            if (!CALL(expression,0,4,1,e)) return 0;
            *b->expression_override = 0;
            b->voice_latches[2] = 0;
            *b->face_mode = 1;
          } else {
            if (!CALL(expression,5,3,1,e)) return 0;
            *b->face_mode = b->voice_latches[2] = 0;
          }
        }
        *exiting = 1;
        return 1;
      }
    }
  }
  if (changed) {
    if (!CALL(begin,e)) return 0;
    *b->state = 3;
    b->frame->camera_mode = 4;
    b->voice_latches[1] = 0;
  }
  return 1;
}
static int idle(const BkEndingTertiaryControlBindings *b,
                const BkEndingTertiaryControlOps *o, char e[256]) {
  int pressed = 0;
  const unsigned codes[] = {0,1,0x5a,0x33450};
  for (unsigned i=0;i<4;++i) {
    if (!key(o,codes[i],i<2?2:1,&pressed,e)) return 0;
    if (pressed) break;
  }
  if (pressed) {
    int clip;
    if (!CALL(active,&clip,e)) return 0;
    if (clip != 6 && clip != 8 && clip != 10) *b->state = 2;
    return 1;
  }
  b->voice_latches[1] = 0;
  if (b->auxiliary->pending != -1) return 1;
  int busy;
  if (!playing(o,0,&busy,e)) return 0;
  if (busy) return 1;
  if (special(b)) {
    if (!CALL(expression,7,6,1,e)) return 0;
    float p = b->auxiliary->progress;
    int cue = !(p >= .2f) ? 1 : (p >= .19f && p < .4f ? 2 : (p >= .39f ? 3 : 0));
    if (cue && !CALL(voice,cue,0,0,0,e)) return 0;
  } else if (!(b->auxiliary->progress >= .2f)) {
    if (!CALL(voice,1,0,0,0,e) || !CALL(expression,7,3,1,e)) return 0;
    b->voice_latches[2] = 0;
  } else if (b->auxiliary->progress >= .19f && b->auxiliary->progress < .4f) {
    if (!CALL(voice,2,0,0,0,e) || !CALL(expression,5,4,0,e)) return 0;
    *b->expression_override = 5;
    b->voice_latches[2] = 0;
    *b->face_mode = 1;
  } else if (b->auxiliary->progress >= .39f) {
    if (!CALL(voice,3,0,0,0,e) || !CALL(expression,5,4,0,e)) return 0;
    *b->expression_override = 5;
    b->voice_latches[2] = 0;
    *b->face_mode = 1;
  }
  b->auxiliary->pending = 1;
  return 1;
}
static int active(BkEndingTertiaryControlState *s,
                  const BkEndingTertiaryControlBindings *b,
                  BkEndingRecord *r, const BkEndingFrameInput *input,
                  float seconds, const BkEndingTertiaryControlOps *o,
                  char e[256]) {
  if (!effects(b,o,e) || !speech(s,b,seconds,o,e)) return 0;
  int busy, clip, hit = 0;
  if (!playing(o,1,&busy,e)) return 0;
  if (!busy && b->frame->camera_mode != 1) {
    if (!CALL(active,&clip,e)) return 0;
    if (clip != 3 && clip != 6 && clip != 8 && clip != 10) {
      float point[2];
      for (unsigned i=0;i<2;++i) {
        int32_t coordinate;
        memcpy(&coordinate,input->words+9+i,4);
        point[i] = (float)coordinate;
      }
      if (!CALL(pick,point,&hit,e)) return 0;
    }
  }
  if (hit != 1) return idle(b,o,e);
  if (!*b->open) {
    if (!CALL(active,&clip,e)) return 0;
    if (clip == 4 && !hover(s,b,o,e)) return 0;
    int pressed;
    if (!confirm(o,&pressed,e)) return 0;
    if (pressed) {
      int exiting = 0;
      if (!chosen(b,r,o,&exiting,e)) return 0;
    }
  }
  return 1;
}
static int finish(BkEndingTertiaryControlState *s,
                  const BkEndingTertiaryControlBindings *b, BkEndingRecord *r,
                  const BkEndingTertiaryControlOps *o, char e[256]) {
  if (*b->previous_flow == 24) b->frame->transition_action = 49;
  else if (*b->previous_flow == 8) {
    if (*b->finish_setting == 1) {
      int busy;
      if (!playing(o,0,&busy,e)) return 0;
      if (!busy) {
        b->frame->transition_action = 8;
        if (!record(r,20,e)) return 0;
      }
    } else if (b->frame->transition_action != 7) {
      b->frame->transition_action = 7;
      if (b->frame->group >= 5) return fail(e, "finish group outside table");
      b->auxiliary->selection = b->frame->group == 2;
      if (!record(r,b->frame->group==2?13:12,e)) return 0;
    }
  }
  b->frame->curtain_wanted = 1;
  *b->substate = 0;
  *b->part_mode = 0;
  for (unsigned i=0;i<3;++i) b->voice_latches[i] = 0;
  *b->return_ready = b->unavailable[0] = b->unavailable[1] = 0;
  *b->movement_ready = 0;
  *b->pending_effect = -1;
  s->last_cue = 0;
  s->replay_elapsed = 0;
  s->replay_after = 30;
  return 1;
}
int bk_ending_tertiary_control_step(BkEndingTertiaryControlState *s,
    const BkEndingTertiaryControlBindings *b, const BkEndingFrameInput *input,
    float seconds, const BkEndingTertiaryControlOps *o, char e[256]) {
  if (!s || !b || !input || !o || !b->frame || !b->control || !b->auxiliary ||
      !b->camera || !b->records || !b->state || !b->face_mode || !b->part_mode ||
      !b->voice_latches || !b->return_ready || !b->unavailable ||
      !b->movement_ready || !b->substate || !b->expression_override ||
      !b->pending_effect || !b->fov || !b->open || !b->previous_flow ||
      !b->finish_setting || !b->actions || !b->targets || !b->initial_targets ||
      !b->voice_volume || !b->effect_volume || !b->random || b->frame->group>=5 ||
      !isfinite(seconds) || seconds<0)
    return fail(e,"invalid live bindings/time");
  BkEndingRecord *r = b->records->groups+b->frame->group;
  switch (*b->state) {
  case 0: return initial(b,o,e);
  case 1: return active(s,b,r,input,seconds,o,e);
  case 3: return CALL(action,input,seconds,e);
  case 2: {
    int released;
    if (!key(o,0,0,&released,e)) return 0;
    if (released) {
      if (!key(o,1,0,&released,e)) return 0;
      if (released) *b->state = 1;
    }
    return 1;
  }
  case 4: return opening(b,seconds,o,e);
  case 5: return finish(s,b,r,o,e);
  default: return 1;
  }
}
