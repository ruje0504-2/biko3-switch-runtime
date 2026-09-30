#include "game/ending_auxiliary_sequence.h"
#include <stdio.h>
#include <string.h>

static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "auxiliary sequence: %s", why);
  return 0;
}
/*Fixed-EXE54e2bc/cc/dc. Zero entries are real buffer identities.*/
static const unsigned initial[5] = {10,29,32,18,13};
static const unsigned middle[5][3] = {
    {11,0,0},{30,0,0},{34,35,0},{21,22,23},{14,15,16}};
static const unsigned final[5][5] = {
    {12,0,0,0,0},{31,0,0,0,0},{36,37,0,0,0},
    {24,25,26,27,28},{17,0,0,0,0}};
static const float ordinary[5][4] = {
    {140,9,120,-3.5f},{0,6,108,-1},{112,21,80,2},
    {328,26,59,1},{345,2,120,-3}};
static const float orbit[5][3][4] = {
    {{77.83f,-33.61f,85.07f,2.24f},{99.09f,-41.8f,63.99f,2.79f},{270,76,49,11}},
    {{328,2,28,5},{24,12,24,6},{180,72,37,5}},
    {{138,17,22,3},{211,-6,19,3},{185,27,23,3}},
    {{320.82f,18.7f,80.52f,-3.05f},{359.8f,-5.88f,39.55f,-1.98f},{313,73,56,0}},
    {{17.22f,-28,40.8f,.86f},{117.63f,29.69f,66.2f,1.45f},{346,-11,47,1}}};

#define CALL(member, ...) do { \
  if (!o->member) return fail(e, "missing " #member " service"); \
  if (!o->member(o->context, __VA_ARGS__)) return 0; \
} while (0)

static int busy(const BkEndingAuxiliarySequenceOps *o, unsigned slot,
                int *out, char e[256]) {
  int present;
  CALL(present, slot, &present, e);
  *out = 0;
  if (present) {
    BkEndingAudioCall c = {.operation=BK_ENDING_AUDIO_STATUS,.slot=slot};
    CALL(audio, &c, out, e);
  }
  return 1;
}
static int sound(const BkEndingAuxiliarySequenceBindings *b,
                 const BkEndingAuxiliarySequenceOps *o, unsigned effect,
                 int loop, int stop, char e[256]) {
  BkEndingAudioCall c = {.operation=stop?BK_ENDING_AUDIO_PAUSE:BK_ENDING_AUDIO_RESTART,
      .slot=effect+2,.flags=loop,.volume=stop?0:*b->effect_volume};
  int ignored = 0;
  CALL(audio, &c, &ignored, e);
  return 1;
}
static int voice(const BkEndingAuxiliarySequenceBindings *b,
                 const BkEndingAuxiliarySequenceOps *o, int cue,
                 unsigned slot, char e[256]) {
  CALL(voice, cue, slot, 0, *b->voice_volume, e);
  return 1;
}
static void set_orbit(BkMenuCamera *c, const float v[4]) {
  c->yaw=v[0]; c->pitch=v[1]; c->radius=v[2]; c->height=v[3];
}
static int threshold(const BkEndingAuxiliarySequenceBindings *b,
    const BkEndingAuxiliarySequenceOps *o, unsigned slot, float bound,
    unsigned latch, unsigned effect, char e[256]) {
  BkClipTiming t;
  CALL(timing, slot, &t, e);
  /*Native fcomp/C0 excludes unordered, includes equality.*/
  if (t.source >= bound && !b->latches[latch]) {
    if (!sound(b,o,effect,0,0,e)) return 0;
    b->latches[latch]=1;
  }
  return 1;
}
static int clip9_effects(const BkEndingAuxiliarySequenceBindings *b,
                         const BkEndingAuxiliarySequenceOps *o, char e[256]) {
  unsigned effect=final[b->frame->group][0];
  if (!threshold(b,o,9,257,6,effect,e) ||
      !threshold(b,o,9,264,7,effect,e)) return 0;
  BkClipTiming t;
  CALL(timing,9,&t,e);
  if ((double)t.start+6.0 >= (double)t.source)
    b->latches[6]=b->latches[7]=0;
  return 1;
}
static int clip6_effects(const BkEndingAuxiliarySequenceBindings *b,
                         const BkEndingAuxiliarySequenceOps *o, char e[256]) {
  const unsigned *fx=middle[b->frame->group];
  if (!threshold(b,o,6,133,1,fx[0],e) ||
      !threshold(b,o,6,140,2,fx[2],e)) return 0;
  int playing;
  if (!busy(o,fx[2]+2,&playing,e)) return 0;
  if (!playing && b->latches[2]==1 && !b->latches[8]) {
    if (!sound(b,o,fx[1],0,0,e)) return 0;
    b->latches[8]=1;
  }
  return 1;
}
static int finish(const BkEndingAuxiliarySequenceBindings *b, BkEndingRecord *r,
                  char e[256]) {
  if (*b->previous_flow==0x18) b->frame->transition_action=0x31;
  else if (*b->previous_flow==8 && b->frame->transition_action!=7) {
    b->frame->transition_action=7;
    b->auxiliary->selection=b->frame->group==2;
    if (!r || r->count<0 || r->count>=BK_ENDING_RECORD_CAPACITY)
      return fail(e,"finish recording lane is unavailable/full");
    r->actions[r->count++]=b->frame->group==2?13:12;
  }
  b->frame->curtain_wanted=1;
  *b->substate=0;
  memset(b->latches,0,10);
  memset(b->voice_latches,0,8);
  *b->timer=0; *b->pass=30;
  b->frame->camera_cached=-1;
  b->control->state_721eec=4;
  return 1;
}
static int state6(const BkEndingAuxiliarySequenceBindings *b,
                   const BkEndingAuxiliarySequenceOps *o, BkEndingRecord *r,
                   char e[256]) {
  int playing;
  int32_t active;
  switch (*b->substate) {
  case 0:
    if (!busy(o,0,&playing,e)) return 0;
    if (!playing) {
      if (!b->latches[0]) {
        *b->pass=0;
        CALL(request,6,e);
        b->frame->camera_mode=4;
        if (!voice(b,o,8,0,e)) return 0;
        b->control->state_721eec=7;
        const float old[4]={b->camera->yaw,b->camera->pitch,
                            b->camera->radius,b->camera->height};
        memcpy(b->saved_orbit,old,sizeof(old));
        *b->saved_toggle=b->control->toggles[1];
        b->control->toggles[1]=1;
        set_orbit(b->camera,orbit[b->frame->group][0]);
        memcpy(b->saved_target,b->frame->camera_values,12);
        float target[3];
        CALL(target,target,e);
        memcpy(b->frame->camera_values,target,12);
        return 1; /*480258 skips all trailing effects on this path.*/
      } else if (b->latches[0]==1) {
        b->latches[0]=3;
        if (b->control->toggles[7] && !voice(b,o,9,1,e)) return 0;
      } else {
        if (b->latches[0]==3) {
          if (!busy(o,1,&playing,e)) return 0;
          if (!playing) {
            b->latches[0]=4;
            if (!voice(b,o,10,0,e)) return 0;
            goto effects;
          }
        }
        if (b->latches[0]==4) {
          *b->substate=1;
          if (!voice(b,o,11,0,e)) return 0;
        }
      }
    }
effects:
    CALL(active,&active,e);
    if (active==6) {
      if (!clip6_effects(b,o,e)) return 0;
      BkClipTiming t;
      CALL(timing,6,&t,e);
      if (t.source>=t.end) {
        if (!busy(o,0,&playing,e)) return 0;
        if (!playing) { CALL(request,7,e); }
      }
    } else if (active==7) {
      BkClipTiming t;
      CALL(timing,7,&t,e);
      if (t.source>=150 && !b->latches[3]) {
        CALL(expression,0,4,0,e);
        *b->expression_override=0; *b->face_mode=1;
        if (!sound(b,o,final[b->frame->group][0],0,0,e)) return 0;
        b->latches[3]=1;
      }
    } else if (active==8) {
      unsigned fx=final[b->frame->group][0];
      if (!threshold(b,o,8,195,4,fx,e) || !threshold(b,o,8,225,5,fx,e)) return 0;
    } else if (active==9 && !clip9_effects(b,o,e)) return 0;
    return 1;
  case 1:
    if (!busy(o,0,&playing,e)) return 0;
    if (!playing && !finish(b,r,e)) return 0;
    /*Native continues this tail after resetting the state and latches.*/
    CALL(active,&active,e);
    return active!=9 || clip9_effects(b,o,e);
  default: return 1;
  }
}
/*State7 shares the stage5 group3 sequence, including multiple effects in one
 * frame when absent/completed buffers allow the dependency chain to progress.*/
static int group3_effects(const BkEndingAuxiliarySequenceBindings *b,
                          const BkEndingAuxiliarySequenceOps *o, char e[256]) {
  const unsigned *fx=final[3];
  int playing;
  if (!threshold(b,o,7,152,0,fx[0],e)) return 0;
  if (b->latches[0]==1 && !b->latches[1]) {
    if (!busy(o,fx[0]+2,&playing,e)) return 0;
    if (!playing) {
      if (!sound(b,o,middle[3][0],0,1,e) || !sound(b,o,fx[1],0,0,e)) return 0;
      b->latches[1]=1;
    }
  }
  if (!threshold(b,o,7,154,3,fx[3],e)) return 0;
  if (b->latches[1]==1 && !b->latches[2]) {
    if (!busy(o,fx[1]+2,&playing,e)) return 0;
    if (!playing) { if (!sound(b,o,fx[4],0,0,e)) return 0; b->latches[2]=1; }
  }
  if (!threshold(b,o,7,150,4,fx[0],e)) return 0;
  if (b->latches[4]==1 && !b->latches[5]) {
    if (!busy(o,fx[0]+2,&playing,e)) return 0;
    if (!playing) {
      if (!sound(b,o,middle[3][1],0,1,e) || !sound(b,o,fx[2],0,0,e)) return 0;
      b->latches[5]=1;
    }
  }
  if (!threshold(b,o,7,162,7,fx[3],e)) return 0;
  if (b->latches[5]==1 && !b->latches[6]) {
    if (!busy(o,fx[2]+2,&playing,e)) return 0;
    if (!playing) { if (!sound(b,o,fx[4],0,0,e)) return 0; b->latches[6]=1; }
  }
  return 1;
}
static int state7(const BkEndingAuxiliarySequenceBindings *b,
                   const BkEndingAuxiliarySequenceOps *o, char e[256]) {
  int32_t active;
  BkClipTiming t;
  CALL(active,&active,e);
  if (active<0) return fail(e,"negative active clip");
  CALL(timing,(unsigned)active,&t,e);
  int playing=1;
  if (t.source>=t.end && !busy(o,0,&playing,e)) return 0;
  if (t.source>=t.end && !playing) {
    uint32_t pass=(uint32_t)*b->pass+1u;
    memcpy(b->pass,&pass,4);
    unsigned g=b->frame->group;
    if (*b->pass==3) {
      if (g<2) {
        CALL(expression,5,6,1,e);
        if (!voice(b,o,8,0,e) || !busy(o,middle[g][0]+2,&playing,e)) return 0;
        if (!playing && !sound(b,o,middle[g][0],1,0,e)) return 0;
        memset(b->latches,0,10); *b->substate=1;
      }
      if (g<4) {
        b->control->state_721eec=5;
        CALL(active,&active,e);
        if (active<0 || active>=127) return fail(e,"next clip outside native slots");
        CALL(request,(unsigned)active+1,e);
      } else {
        b->latches[0]=1;
        b->control->state_721eec=6;
      }
      b->frame->camera_mode=0;
      b->control->toggles[1]=*b->saved_toggle;
      if (g==0 || g==1 || g==4) set_orbit(b->camera,ordinary[g]);
      else {
        set_orbit(b->camera,b->saved_orbit);
        memcpy(b->frame->camera_values,b->saved_target,12);
      }
    } else {
      CALL(active,&active,e);
      if (active<0) return fail(e,"negative restart clip");
      CALL(restart,(unsigned)active,e);
      if (!voice(b,o,g<2?7:g<4?10:8,0,e)) return 0;
      if (g<2) {
        if (!sound(b,o,initial[g],0,0,e)) return 0;
      } else {
        memset(b->latches,0,10);
        if (g==3 && (!sound(b,o,middle[g][0],1,0,e) ||
                     !sound(b,o,middle[g][1],1,0,e))) return 0;
      }
      if (*b->pass<0 || *b->pass>=3) return fail(e,"camera pass outside native table");
      set_orbit(b->camera,orbit[g][*b->pass]);
    }
  }
  unsigned g=b->frame->group;
  if (g==2) {
    CALL(timing,7,&t,e);
    if (t.source>=150 && (int8_t)b->latches[0]<2) {
      if (!busy(o,final[g][1]+2,&playing,e)) return 0;
      if (!playing) {
        if (!sound(b,o,final[g][1],0,0,e)) return 0;
        b->latches[0]=(uint8_t)(b->latches[0]+1u);
      }
    }
  } else if (g==3) return group3_effects(b,o,e);
  else if (g==4) return clip6_effects(b,o,e);
  return 1;
}
int bk_ending_auxiliary_sequence_step(const BkEndingAuxiliarySequenceBindings *b,
    const BkEndingAuxiliarySequenceOps *o, char e[256]) {
  if (!b || !o || !b->frame || !b->control || !b->auxiliary || !b->camera ||
      !b->substate || !b->latches || !b->saved_toggle || !b->voice_latches ||
      !b->timer || !b->pass || !b->saved_orbit || !b->saved_target ||
      !b->expression_override || !b->face_mode || !b->previous_flow ||
      !b->voice_volume || !b->effect_volume || b->frame->group>=5)
    return fail(e,"invalid state6/7 bindings");
  BkEndingRecord *r=b->records?b->records->groups+b->frame->group:NULL;
  if (b->control->state_721eec==6) return state6(b,o,r,e);
  if (b->control->state_721eec==7) return state7(b,o,e);
  return fail(e,"called outside state6/7");
}
#undef CALL
