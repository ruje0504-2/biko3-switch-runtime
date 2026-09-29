#include "game/ending_secondary_control.h"
#include "core/random.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static const float cameras[5][3][4] = {
  {{3.64f,-8.49f,31.74f,-2.17f},{342.43f,24.76f,45.49f,-4.48f},{9.02f,-29.9f,32.7f,-.94f}},
  {{358.67f,-7.35f,25.7f,-1.07f},{322.7f,-8.46f,16.69f,-1.09f},{382.44f,-3.13f,51.47f,-.26f}},
  {{314.5f,12.86f,55.6f,-1.1f},{293.07f,-15.99f,28.69f,.19f},{318.24f,-12.39f,15.5f,-.6f}},
  {{228.34f,21.11f,66.44f,-3.52f},{84.93f,11.1f,66.44f,-3.52f},{160.44f,-36.54f,68.78f,-5.06f}},
  {{1.37f,-48.3f,25.94f,-4.02f},{62.84f,-39.54f,18.35f,-2.24f},{261.23f,-2.71f,28.51f,-1.18f}}
};
const float *bk_ending_secondary_control_camera(unsigned group, unsigned row) {
  return group < 5 && row < 3 ? cameras[group][row] : NULL;
}
BkEndingSecondaryControlState bk_ending_secondary_control_initial(void) {
  BkEndingSecondaryControlState s = {0};
  s.remaining = 15;
  s.alternate = 1;
  s.fov = 1;
  return s;
}
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "secondary ending control: %s", why);
  return 0;
}
static void wrap_add(int32_t *value, uint32_t amount) {
  uint32_t bits = (uint32_t)*value + amount;
  memcpy(value, &bits, sizeof(bits));
}
#define CALL(name, ...) \
  (o->name ? o->name(o->context, __VA_ARGS__) : fail(e, "missing " #name " service"))
static int key(const BkEndingSecondaryControlOps *o, unsigned code,
               unsigned mode, int *pressed, char e[256]) {
  uint32_t result;
  if (!CALL(key, code, mode, &result, e)) return 0;
  *pressed = (result & 255u) != 0;
  return 1;
}
static int playing(const BkEndingSecondaryControlOps *o, unsigned speech,
                   int *result, char e[256]) {
  int present;
  if (!CALL(present, speech, &present, e)) return 0;
  if (!present) { *result = 0; return 1; }
  return CALL(status, speech, result, e);
}
static int expression(const BkEndingSecondaryControlBindings *b,
                      const BkEndingSecondaryControlOps *o,
                      int a, int z, char e[256]) {
  b->auxiliary->expression_a = a;
  b->auxiliary->expression_b = z;
  return CALL(eyes, 1, e);
}
static int ordinary_expression(const BkEndingSecondaryControlBindings *b,
                               const BkEndingSecondaryControlOps *o,
                               char e[256]) {
  return b->frame->group == 1 ? expression(b,o,6,3,e)
                              : expression(b,o,5,8,e);
}
static int initial_expression(const BkEndingSecondaryControlBindings *b,
                              const BkEndingSecondaryControlOps *o,
                              char e[256]) {
  return expression(b,o,6,b->frame->group <= 1 ? 3 : 1,e);
}
static int target(const BkEndingSecondaryControlBindings *b,
                  const BkEndingSecondaryControlOps *o, char e[256]) {
  float position[3];
  if (!CALL(target, position, e)) return 0;
  memcpy(b->frame->camera_values, position, sizeof(position));
  return 1;
}
static int camera(const BkEndingSecondaryControlBindings *b,
                  const BkEndingSecondaryControlOps *o,
                  BkEndingOpeningCamera mode, int *done, char e[256]) {
  if (b->frame->group >= 5) return fail(e, "camera table group outside range");
  uint32_t result;
  if (!CALL(camera, mode, b->frame->camera_clip, b->frame->camera_values,
            b->frame->camera_table[b->frame->group][3], &result, e)) return 0;
  *done = (result & 255u) != 0;
  return 1;
}
static int zoom(BkEndingSecondaryControlState *s,
                const BkEndingSecondaryControlBindings *b, float seconds) {
  b->camera->fov = s->fov;
  if (seconds < 1.f) s->fov = (float)((double)s->fov - (double)seconds * .2f);
  if (!(s->fov > .2f)) { s->fov = .2f; return 1; }
  return 0;
}
static int opening(BkEndingSecondaryControlState *s,
                   const BkEndingSecondaryControlBindings *b, float seconds,
                   const BkEndingSecondaryControlOps *o, char e[256]) {
  int done, active, present;
  switch ((int8_t)*b->substate) {
  case 0:
    if (*b->previous_flow == 0x18) *b->substate = 1;
    else if (*b->previous_flow == 8) *b->substate = 5;
    if (!initial_expression(b,o,e)) return 0;
    b->auxiliary->index = 18;
    s->fov = 1.f;
    break;
  case 1:
    zoom(s,b,seconds);
    if (!camera(b,o,BK_ENDING_OPENING_TRACK,&done,e)) return 0;
    if (done) {
      b->camera->fov = .2f;
      *b->substate = 2;
      memcpy(b->control->saved_camera,b->camera->matrix,64);
      if (!target(b,o,e)) return 0;
    }
    break;
  case 2:
    if (!camera(b,o,BK_ENDING_OPENING_PRESET,&done,e)) return 0;
    if (done) {
      *b->substate = 3;
      b->camera->fov = .2f;
      if (!CALL(voice,1,0,0,e)) return 0;
      b->auxiliary->pending = 0;
      b->frame->camera_mode = 0;
    }
    break;
  case 3:
    if (!playing(o,0,&active,e)) return 0;
    if (active) break;
    if (!CALL(present,1,&present,e)) return 0;
    if (!present && b->control->toggles[7]) {
      if (!CALL(voice,2,1,0,e)) return 0;
      break;
    }
    if (!playing(o,1,&active,e)) return 0;
    if (!active) {
      if (!CALL(voice,3,0,0,e) || !CALL(request,2,e) ||
          !ordinary_expression(b,o,e)) return 0;
      b->auxiliary->index = 19;
      *b->substate = 4;
    }
    break;
  case 4:
    if (!playing(o,0,&active,e)) return 0;
    if (!active) b->frame->state_721ee4 = 0;
    break;
  case 5: {
    if (!target(b,o,e)) return 0;
    int zoom_done = zoom(s,b,seconds);
    if (!camera(b,o,BK_ENDING_OPENING_PRESET,&done,e)) return 0;
    if (zoom_done && done) {
      *b->substate = 3;
      s->fov = 1.f;
      if (!CALL(voice,1,0,0,e)) return 0;
      b->auxiliary->pending = 0;
      b->frame->camera_mode = 0;
      b->camera->fov = .2f;
    }
    break;
  }
  }
  return 1;
}
static int active_timing(const BkEndingSecondaryControlOps *o,
                         BkEndingSecondaryControlTiming *timing, char e[256]) {
  int32_t slot;
  return CALL(active,&slot,e) && CALL(timing,slot,timing,e);
}
static void pointer(const BkEndingFrameInput *input, float point[2]) {
  for (unsigned i=0;i<2;++i) {
    int32_t coordinate;
    memcpy(&coordinate,input->words+9+i,4);
    point[i] = (float)coordinate;
  }
}
static int confirm(const BkEndingSecondaryControlOps *o, int *pressed,
                   char e[256]) {
  const unsigned codes[] = {0,0x5a,0x33450};
  for (unsigned i=0;i<3;++i) {
    if (!key(o,codes[i],1,pressed,e)) return 0;
    if (*pressed) return 1;
  }
  return 1;
}
static int active(BkEndingSecondaryControlState *s,
                  const BkEndingSecondaryControlBindings *b,
                  const BkEndingFrameInput *input, float seconds,
                  const BkEndingSecondaryControlOps *o, char e[256]) {
  int busy, pressed;
  if (s->automatic == 3) {
    BkEndingSecondaryControlTiming timing;
    if (!active_timing(o,&timing,e)) return 0;
    double threshold = timing.end - (double)seconds * *b->rate * 60.0;
    if (!(threshold > timing.source)) {
      wrap_add(&s->remaining,UINT32_MAX);
      if (!s->remaining) {
        if (s->alternate) {
          if (!CALL(request,8,e)) return 0;
          s->alternate = 0;
          if (!CALL(voice,11,0,0,e) || !expression(b,o,5,9,e)) return 0;
          b->auxiliary->index = 20;
          b->auxiliary->pending = 2;
        } else {
          if (!CALL(request,9,e)) return 0;
          s->alternate = 1;
          if (!ordinary_expression(b,o,e) || !CALL(voice,12,0,0,e)) return 0;
          b->auxiliary->index = 19;
          b->auxiliary->pending = 3;
        }
        s->remaining = (int32_t)(bk_random_next(b->random)%11u)+15;
      }
    }
  }
  int lane = (b->auxiliary->pending == 0 || b->auxiliary->pending == 3) ? 0 :
             (b->auxiliary->pending == 1 || b->auxiliary->pending == 2) ? 1 : -1;
  if (lane >= 0) {
    if (!playing(o,0,&busy,e)) return 0;
    if (!busy) {
      if (!CALL(voice,lane ? 8 : 4,0,1,e)) return 0;
      s->chance = bk_random_next(b->random)%1000u <= 25;
      if (!b->voice_latches[lane]) {
        if (b->control->toggles[7] && !CALL(voice,lane ? 9 : 5,1,0,e)) return 0;
        b->voice_latches[lane] = 1;
      } else if (s->chance && b->control->toggles[7] &&
                 !CALL(voice,lane ? 9 : 5,1,0,e)) return 0;
      b->auxiliary->pending = lane ? 11 : 10;
    }
  }
  int32_t hit = 0;
  if (!playing(o,1,&busy,e)) return 0;
  if (!busy && b->frame->camera_mode != 1) {
    float point[2]; pointer(input,point);
    if (!CALL(pick,point,0,&hit,e)) return 0;
  }
  if (hit == 1 || hit == 2) {
    if (!*b->open) {
      if (!confirm(o,&pressed,e)) return 0;
      if (pressed) {
        const int32_t *point = b->alternate_point;
        if (hit == 1) {
          if (b->frame->camera_cached < 0 || b->frame->camera_cached >= 39)
            return fail(e,"selected target outside original table");
          point = b->targets[b->frame->camera_cached];
        }
        int32_t result;
        if (!CALL(choose,point,&result,e)) return 0;
        if (result != -1) {
          b->frame->state_721ee4 = 3;
          if (!CALL(action,e)) return 0;
          b->frame->camera_mode = 4;
        }
      }
    }
  } else {
    const unsigned codes[] = {0,1,0x5a,0x33450};
    for (unsigned i=0;i<4;++i) {
      if (!key(o,codes[i],i<2 ? 2 : 1,&pressed,e)) return 0;
      if (pressed) { b->frame->state_721ee4=2; break; }
    }
  }
  return 1;
}
/*47b0c5..47b6e7; writes between services are intentional. */
static int mode(BkEndingSecondaryControlState *s,
                const BkEndingSecondaryControlBindings *b, int32_t choice,
                const BkEndingSecondaryControlOps *o, char e[256]) {
  int32_t clip = -1;
  if (s->automatic == 3 && choice < 3 && !CALL(active,&clip,e)) return 0;
  if (choice == 0) {
    if (s->automatic == 1 || (s->automatic == 3 && clip == 3)) {
      if (!CALL(request,4,e) || !CALL(voice,6,0,0,e) || !initial_expression(b,o,e)) return 0;
      b->auxiliary->index=18; b->auxiliary->pending=4;
    } else if (s->automatic == 2 || (s->automatic == 3 && clip == 6)) {
      if (!CALL(request,7,e) || !CALL(voice,10,0,0,e)) return 0;
      b->auxiliary->index=18;
      if (!initial_expression(b,o,e)) return 0;
      b->auxiliary->pending=5;
    }
    s->automatic=0;
  } else if (choice == 1) {
    if (s->automatic == 0) {
      if (!CALL(request,2,e) || !CALL(voice,3,0,0,e) || !ordinary_expression(b,o,e)) return 0;
      b->auxiliary->index=19; b->auxiliary->pending=0;
    } else if (s->automatic == 2 || (s->automatic == 3 && clip == 6)) {
      if (!CALL(request,9,e) || !CALL(voice,12,0,0,e) || !ordinary_expression(b,o,e)) return 0;
      b->auxiliary->index=19; b->auxiliary->pending=3;
    } else if (s->automatic == 3 && clip == 3 && !CALL(request,3,e)) return 0;
    s->automatic=1;
  } else if (choice == 2) {
    if (s->automatic == 0) {
      if (!CALL(request,5,e) || !CALL(voice,7,0,0,e) || !expression(b,o,0,9,e)) return 0;
      b->auxiliary->index=20; b->auxiliary->pending=1;
    } else if (s->automatic == 1 || (s->automatic == 3 && clip == 3)) {
      if (!CALL(request,8,e) || !CALL(voice,11,0,0,e) || !expression(b,o,0,9,e)) return 0;
      b->auxiliary->index=20; b->auxiliary->pending=2;
    } else if (s->automatic == 3 && clip == 6 && !CALL(request,6,e)) return 0;
    s->automatic=2;
  } else if (choice == 3) {
    if (s->automatic == 0) {
      if (!CALL(request,2,e) || !CALL(voice,3,0,0,e) || !ordinary_expression(b,o,e)) return 0;
      b->auxiliary->index=19; b->auxiliary->pending=0;
      s->alternate=1; s->remaining=8;
    } else if (s->automatic == 1) {
      if (!CALL(request,8,e) || !CALL(voice,11,0,0,e) || !expression(b,o,0,9,e)) return 0;
      b->auxiliary->index=20; b->auxiliary->pending=2;
      s->alternate=0; s->remaining=8;
    } else if (s->automatic == 2) {
      if (!CALL(request,9,e) || !CALL(voice,12,0,0,e) || !ordinary_expression(b,o,e)) return 0;
      b->auxiliary->index=19; b->auxiliary->pending=3;
      s->alternate=1; s->remaining=8;
    }
    s->automatic=3;
  }
  return 1;
}
static void leave(BkEndingSecondaryControlState *s,
                  const BkEndingSecondaryControlBindings *b, int from_menu) {
  if (*b->previous_flow == 0x18) b->frame->transition_action=0x31;
  else if (*b->previous_flow == 8) {
    b->frame->transition_action=6;
    *b->next_mode=1;
  }
  b->frame->curtain_wanted=1;
  if (from_menu) *b->rate=.3f;
  s->remaining=15; s->alternate=1;
  b->frame->state_721ee4=4;
  *b->substate=0;
  b->auxiliary->gate=-1;
  b->frame->camera_cached=-1;
  memset(b->voice_latches,0,2*sizeof(*b->voice_latches));
  b->frame->camera_event=0;
  if (from_menu) b->frame->camera_mode=0;
}
static int menu(BkEndingSecondaryControlState *s,
                const BkEndingSecondaryControlBindings *b,
                const BkEndingFrameInput *input,
                const BkEndingSecondaryControlOps *o, char e[256]) {
  int pressed;
  if (!key(o,0,3,&pressed,e)) return 0;
  if (!pressed) return 1;
  float point[2]; pointer(input,point);
  unsigned index;
  for (index=0;index<3;++index) {
    float dx=point[0]-(float)b->points[index][0];
    float dy=point[1]-(float)b->points[index][1];
    float squared=(float)((double)dx*dx+(double)dy*dy);
    float distance=(float)sqrt((double)squared);
    if (*b->menu_width/2.f > distance) break; /*strict4a777e boundary*/
  }
  if (b->frame->camera_event == 1) {
    if (index < 3 && !mode(s,b,b->choices[index],o,e)) return 0;
    b->frame->state_721ee4=1; b->frame->camera_mode=0; b->frame->camera_event=0;
  } else if (b->frame->camera_event == 2) {
    if (index < 2) {
      if (b->choices[index] == 4) {
        int32_t clip;
        if (!CALL(active,&clip,e)) return 0;
        if (clip == 2 || clip == 3 || clip == 4 || clip == 9) {
          if (!CALL(request,10,e) || !expression(b,o,0,11,e)) return 0;
        } else if (clip == 5 || clip == 6 || clip == 7 || clip == 8) {
          if (!CALL(request,11,e) || !expression(b,o,0,12,e)) return 0;
        }
        *b->rate=.3f; b->auxiliary->index=21;
        if (!CALL(voice,13,0,0,e)) return 0;
        s->remaining=20;
      } else if (b->choices[index] == 5) {
        leave(s,b,1); return 1;
      }
      b->frame->camera_event=0; b->frame->state_721ee4=5; b->frame->camera_mode=0;
    } else {
      b->frame->camera_event=0; b->frame->state_721ee4=1; b->frame->camera_mode=0;
    }
  }
  return 1;
}
static int install_camera(const BkEndingSecondaryControlBindings *b,
                          unsigned row, char e[256]) {
  const float *v=bk_ending_secondary_control_camera(b->frame->group,row);
  if (!v) return fail(e,"special camera table access outside range");
  b->camera->yaw=v[0]; b->camera->pitch=v[1]; b->camera->radius=v[2]; b->camera->height=v[3];
  return 1;
}
static void save_view(BkEndingSecondaryControlState *s,
                      const BkEndingSecondaryControlBindings *b) {
  s->saved_toggle=b->control->toggles[1];
  b->control->toggles[1]=1;
  s->saved_camera[0]=b->camera->yaw; s->saved_camera[1]=b->camera->pitch;
  s->saved_camera[2]=b->camera->radius; s->saved_camera[3]=b->camera->height;
}
static int finishing(BkEndingSecondaryControlState *s,
                     const BkEndingSecondaryControlBindings *b, float seconds,
                     const BkEndingSecondaryControlOps *o, char e[256]) {
  int32_t clip; int busy;
  if (!CALL(active,&clip,e)) return 0;
  if (clip != 12) return 1;
  if (!playing(o,0,&busy,e)) return 0;
  if (!busy) {
    if (!target(b,o,e) || !CALL(voice,14,0,1,e)) return 0;
    return !b->control->toggles[7] || CALL(voice,15,1,0,e);
  }
  BkEndingSecondaryControlTiming timing;
  if (!CALL(timing,12,&timing,e)) return 0;
  double threshold=timing.end-(double)seconds**b->rate*60.0*2.0;
  if (!(threshold > timing.source)) {
    wrap_add(&s->remaining,UINT32_MAX);
    if (!s->remaining) {
      *b->rate=b->frame->group ? .4f : .6f;
      if (!CALL(voice,16,0,0,e)) return 0;
      if (b->frame->group == 1 || b->frame->group == 2) {
        if (!expression(b,o,5,3,e)) return 0;
      } else if (!expression(b,o,0,13,e)) return 0;
      if (!CALL(request,13,e)) return 0;
      if (b->frame->group == 0 || b->frame->group == 4) {
        save_view(s,b);
        if (!install_camera(b,0,e)) return 0;
        b->frame->state_721ee4=7; b->frame->camera_mode=4;
      } else b->frame->state_721ee4=6;
      *b->substate=0; s->remaining=0;
    }
  }
  return 1;
}
static int ended(const BkEndingSecondaryControlOps *o, int32_t clip,
                 int *done, char e[256]) {
  BkEndingSecondaryControlTiming timing;
  if (!CALL(timing,clip,&timing,e)) return 0;
  *done=timing.source >= timing.end; /*unordered does not finish*/
  return 1;
}
static int completion(BkEndingSecondaryControlState *s,
                      const BkEndingSecondaryControlBindings *b,
                      const BkEndingSecondaryControlOps *o, char e[256]) {
  int done,busy; int32_t clip;
  switch (*b->substate) {
  case 0:
    if (!ended(o,13,&done,e)) return 0;
    if (!done) break;
    if (!playing(o,0,&busy,e)) return 0;
    if (busy) break;
    if (!CALL(voice,17,0,0,e)) return 0;
    *b->substate=1;
    if (!CALL(request,14,e)) return 0;
    if (b->frame->group == 1 || b->frame->group == 2) {
      s->saved_toggle=b->control->toggles[1]; b->control->toggles[1]=1;
      if (!expression(b,o,b->frame->group == 1 ? 5 : 0,3,e)) return 0;
      /*Only orbit values are captured after the eye callback.*/
      s->saved_camera[0]=b->camera->yaw; s->saved_camera[1]=b->camera->pitch;
      s->saved_camera[2]=b->camera->radius; s->saved_camera[3]=b->camera->height;
      if (!install_camera(b,0,e)) return 0;
      b->frame->state_721ee4=7; b->frame->camera_mode=4;
    }
    break;
  case 1:
    if (!CALL(active,&clip,e)) return 0;
    if (clip == 14) {
      if (!ended(o,14,&done,e)) return 0;
      if (done) {
        if (!expression(b,o,7,b->frame->group == 3 || b->frame->group == 4 ? 1 : 3,e) ||
            !CALL(request,15,e)) return 0;
        if (b->frame->group == 3) {
          save_view(s,b);
          if (!install_camera(b,0,e) || !target(b,o,e)) return 0;
          b->frame->state_721ee4=7; b->frame->camera_mode=4;
        }
      }
    }
    if (!CALL(active,&clip,e)) return 0;
    if (clip == 15) {
      if (!ended(o,15,&done,e)) return 0;
      if (done) {
        if (!CALL(request,16,e) || !playing(o,0,&busy,e)) return 0;
        if (!busy) { *b->substate=2; if (!CALL(voice,18,0,0,e)) return 0; }
      }
    }
    break;
  case 2:
    if (!playing(o,0,&busy,e)) return 0;
    if (!busy) leave(s,b,0);
    break;
  }
  return 1;
}
static int special(BkEndingSecondaryControlState *s,
                   const BkEndingSecondaryControlBindings *b,
                   const BkEndingSecondaryControlOps *o, char e[256]) {
  BkEndingSecondaryControlTiming timing;
  int busy; int32_t clip;
  if (!active_timing(o,&timing,e)) return 0;
  if (!(timing.source >= timing.end)) return 1;
  if (!playing(o,0,&busy,e)) return 0;
  if (busy) return 1;
  wrap_add(&s->remaining,1);
  if (s->remaining == 3) {
    if (b->frame->group == 0 || b->frame->group == 4) {
      if (!CALL(voice,17,0,0,e)) return 0;
      *b->substate=1;
    } else if (b->frame->group == 1 || b->frame->group == 2) {
      if (b->frame->group == 2 && !expression(b,o,5,3,e)) return 0;
      *b->substate=1;
    } else {
      *b->substate=2;
      if (!CALL(voice,18,0,0,e)) return 0;
    }
    b->control->toggles[1]=s->saved_toggle;
    if (!CALL(active,&clip,e)) return 0;
    wrap_add(&clip,1);
    if (!CALL(request,clip,e)) return 0;
    b->frame->state_721ee4=6; b->frame->camera_mode=0;
    b->camera->yaw=s->saved_camera[0]; b->camera->pitch=s->saved_camera[1];
    b->camera->radius=s->saved_camera[2]; b->camera->height=s->saved_camera[3];
  } else {
    if (!CALL(active,&clip,e) || !CALL(restart,clip,e)) return 0;
    if (b->frame->group == 0 || b->frame->group == 4) {
      if (!CALL(voice,16,0,0,e)) return 0;
    } else if (b->frame->group == 1 || b->frame->group == 2) {
      if (!CALL(voice,17,0,0,e)) return 0;
    }
    if (!install_camera(b,(unsigned)s->remaining,e)) return 0;
  }
  return 1;
}
int bk_ending_secondary_control_step(BkEndingSecondaryControlState *s,
    const BkEndingSecondaryControlBindings *b, const BkEndingFrameInput *input,
    float seconds, const BkEndingSecondaryControlOps *o, char e[256]) {
  if (!s || !b || !input || !o || !b->frame || !b->control || !b->auxiliary ||
      !b->camera || !b->substate || !b->voice_latches || !b->rate || !b->next_mode ||
      !b->previous_flow || !b->open || !b->targets || !b->points || !b->choices ||
      !b->alternate_point || !b->menu_width || !b->random || !isfinite(seconds) || seconds<0)
    return fail(e,"missing live bindings or invalid seconds");
  switch (b->frame->state_721ee4) {
  case 0:
    if (!CALL(voice,4,0,1,e)) return 0;
    if (b->control->toggles[7] && !CALL(voice,5,1,0,e)) return 0;
    b->voice_latches[0]=1; b->frame->state_721ee4=1;
    *b->rate=.3f; s->automatic=3;
    return 1;
  case 1: return active(s,b,input,seconds,o,e);
  case 2: {
    int pressed;
    if (!key(o,0,0,&pressed,e)) return 0;
    if (pressed) {
      if (!key(o,1,0,&pressed,e)) return 0;
      if (pressed) b->frame->state_721ee4=1;
    }
    return 1;
  }
  case 3: return menu(s,b,input,o,e);
  case 4: return opening(s,b,seconds,o,e);
  case 5: return finishing(s,b,seconds,o,e);
  case 6: return completion(s,b,o,e);
  case 7: return special(s,b,o,e);
  default: return 1;
  }
}
#undef CALL
