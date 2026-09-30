#include "game/ending_gallery_auxiliary.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game/ending_gallery_auxiliary_tables.inc"
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "gallery auxiliary: %s", why);
  return 0;
}
#define CALL(name, ...) (o->name ? o->name(o->context, __VA_ARGS__) : fail(e, "missing " #name " service"))
#define READ(slot) do { if (!CALL(timing, slot, &t, e)) return 0; } while (0)
#define ACTIVE() do { if (!CALL(active, &active, e)) return 0; } while (0)
#define VOICE(cue,slot) CALL(voice,cue,slot,0,e)
BkEndingGalleryAuxiliaryState bk_ending_gallery_auxiliary_initial(void) {
  BkEndingGalleryAuxiliaryState s = {0}; s.fov = 1.f; return s;
}
static int sb(uint8_t v) { return v < 128 ? v : (int)v - 256; }
static int32_t plus_one(int32_t v) { uint32_t u = (uint32_t)v + 1u; memcpy(&v, &u, 4); return v; }
static int playing(const BkEndingGalleryAuxiliaryOps *o, unsigned slot, int *busy, char e[256]) {
  int present;
  if (!CALL(present, slot, &present, e)) return 0;
  if (!present) { *busy = 0; return 1; }
  return CALL(status, slot, busy, e);
}
static int effect_slot(const BkEndingGalleryAuxiliaryBindings *b, unsigned table,
    unsigned index, unsigned *slot, char e[256]) {
  if (b->frame->group >= 5) return fail(e, "effect group outside table");
  const int8_t *p = table == 0 ? effect_start : table == 1 ? effect_middle : effect_finish;
  *slot = 2u + (unsigned)p[b->frame->group * (table == 2 ? 5u : 3u) + index];
  return 1;
}
static int effect(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o, unsigned table, unsigned index,
    int flags, char e[256]) {
  unsigned slot; int32_t volume = *b->effect_volume; /*captured before table lookup*/
  return effect_slot(b, table, index, &slot, e) && CALL(play, slot, flags, volume, e);
}
static int effect_busy(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o, unsigned table, unsigned index,
    int *busy, char e[256]) {
  unsigned slot;
  return effect_slot(b, table, index, &slot, e) && playing(o, slot, busy, e);
}
static int effect_stop(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o, unsigned table, unsigned index, char e[256]) {
  unsigned slot;
  return effect_slot(b, table, index, &slot, e) && CALL(stop, slot, e);
}
static int target(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o, unsigned node, char e[256]) {
  uint32_t v[3];
  if (!CALL(target, node, v, e)) return 0;
  memcpy(b->frame->camera_values, v, 12); return 1;
}
static void orbit(BkMenuCamera *c, const void *v) {
  const unsigned char *p = v;
  memcpy(&c->yaw, p, 4); memcpy(&c->pitch, p+4, 4);
  memcpy(&c->radius, p+8, 4); memcpy(&c->height, p+12, 4);
}
static void save_orbit(const BkEndingGalleryAuxiliaryBindings *b) {
  b->saved->orbit[0]=b->camera->yaw; b->saved->orbit[1]=b->camera->pitch;
  b->saved->orbit[2]=b->camera->radius; b->saved->orbit[3]=b->camera->height;
}
static int base_orbit(const BkEndingGalleryAuxiliaryBindings *b, int presets, char e[256]) {
  if (b->frame->group >= 5) return fail(e, "base camera group outside table");
  const uint32_t *v = camera_base + b->frame->group * 4u;
  orbit(b->camera, v);
  if (presets) for (unsigned i=0;i<4;++i) memcpy(&b->presets->active[i][0],v+i,4);
  return 1;
}
static int pass_orbit(const BkEndingGalleryAuxiliaryBindings *b, int view, char e[256]) {
  if (b->frame->group >= 5 || (unsigned)view >= 3) return fail(e, "pass camera outside table");
  orbit(b->camera, camera_pass + b->frame->group * 12u + (unsigned)view * 4u); return 1;
}
static int frame_camera(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o, int mode, char e[256]) {
  b->control->target_choice=1; b->frame->camera_clip=0;
  if (!target(b,o,5,e)) return 0;
  if (b->frame->group >= 5) return fail(e,"camera table group outside table");
  b->frame->camera_table[b->frame->group][0]=UINT32_C(0x40000000);
  b->frame->camera_mode=mode; return 1;
}
static void clear_counters(const BkEndingGalleryAuxiliaryBindings *b) { memset(b->counters,0,40); }
static int rewind_clip(const BkEndingGalleryAuxiliaryOps *o,int slot,char e[256]) {
  BkEndingClipTiming t; READ(slot); return CALL(source,slot,t.start,e);
}
static int opening(BkEndingGalleryAuxiliaryState *s, const BkEndingGalleryAuxiliaryBindings *b,
    float seconds,const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  int busy,present;
  switch(sb(*b->opening)) {
  case 0:
    b->auxiliary->index=72; s->fov=1.f;
    if (!rewind_clip(o,2,e)||!rewind_clip(o,3,e)) return 0;
    clear_counters(b); *b->opening=1; break;
  case 1: {
    if (!target(b,o,5,e)||!CALL(fov,s->fov,e)) return 0;
    if(seconds<1.f) s->fov=(float)((double)s->fov-(double)seconds*.2f);
    int done=!(s->fov>.2f); if(done) s->fov=.2f;
    if(b->frame->group>=5) return fail(e,"opening camera group outside table");
    uint32_t result;
    if(!CALL(camera,BK_ENDING_OPENING_PRESET,b->frame->camera_clip,
        b->frame->camera_values,b->frame->camera_table[b->frame->group][3],&result,e)) return 0;
    if(done&&(result&255u)) {
      *b->opening=2; s->fov=1.f;
      if(!VOICE(1,0)) return 0;
      b->auxiliary->pending=1; b->frame->camera_mode=0;
    } break;
  }
  case 2:
    if(!playing(o,0,&busy,e)) return 0;
    if(!busy) {
      if(!CALL(present,1,&present,e)) return 0;
      if(!present&&b->control->toggles[7]) { if(!VOICE(2,1)) return 0; }
      else { if(!playing(o,1,&busy,e)) return 0; if(!busy) *b->opening=3; }
    } break;
  case 3:
    if(!playing(o,1,&busy,e)) return 0;
    if(!busy) { *b->substate=1; *b->opening=0; } break;
  default: break;
  } return 1;
}
/*Timing is deliberately queried again after effects: callbacks can change
 *the current resource/state, and the native instruction stream rereads it.*/
static int marked_effect(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o,int clip,float threshold,unsigned counter,
    unsigned table,unsigned index,char e[256]) {
  BkEndingClipTiming t; READ(clip);
  if(t.source>=threshold&&!b->counters[counter]) {
    if(!effect(b,o,table,index,0,e)) return 0;
    b->counters[counter]=1;
  } return 1;
}
static int start_effects(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  BkEndingClipTiming t; int busy;
  if(b->frame->group==2) {
    return marked_effect(b,o,3,60,0,0,0,e)&&marked_effect(b,o,3,65,1,0,1,e);
  } else if(b->frame->group==3) {
    if(!marked_effect(b,o,2,32,0,0,0,e)) return 0;
    if(b->counters[0]==1&&!b->counters[1]) {
      if(!effect_busy(b,o,0,0,&busy,e)) return 0;
      if(!busy) { if(!effect(b,o,0,1,1,e)) return 0; b->counters[1]=1; }
    }
    if(!marked_effect(b,o,3,56,2,0,0,e)) return 0;
    if(b->counters[2]==1&&!b->counters[3]) {
      if(!effect_busy(b,o,0,0,&busy,e)) return 0;
      if(!busy) { if(!effect(b,o,0,2,1,e)) return 0; b->counters[3]=1; }
    }
  } else if(b->frame->group==4) {
    READ(2);
    if(t.source>=t.end&&!b->counters[0]) {
      if(!effect(b,o,0,0,0,e)) return 0;
      b->counters[0]=1;
    }
  } return 1;
}
static int prelude(BkEndingGalleryAuxiliaryState *s,const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  int busy; int32_t active; BkEndingClipTiming t;
  switch(*b->opening) {
  case 0:
    if(!playing(o,0,&busy,e)) return 0;
    if(!busy) {
      ACTIVE();
      if(active==3) {
        if(b->auxiliary->pending!=3) {
          if(!VOICE(4,0)) return 0;
          b->auxiliary->pending=3;
        } else {
          READ(3);
          if(t.source>=t.end) {
            if(!CALL(request,4,e)) return 0;
            if(b->frame->group==2) b->auxiliary->index=73;
            if(!VOICE(5,0)) return 0;
            *b->opening=1;
          }
        }
      }
    }
    return start_effects(b,o,e);
  case 1:
    if(!playing(o,0,&busy,e)) return 0;
    if(!busy) { if(b->control->toggles[7]&&!VOICE(6,1)) return 0; *b->opening=2; } break;
  case 2:
    if(!CALL(request,5,e)) return 0;
    if(b->frame->group!=2) { if(!CALL(expression,7,3,1,e)) return 0; }
    else if(b->frame->group!=4) { if(!CALL(expression,5,13,1,e)) return 0; }
    else if(!CALL(expression,5,4,0,e)) return 0; /*native repeated live test*/
    if(!VOICE(7,0)) return 0;
    *b->opening=0;
    if((b->frame->group==0||b->frame->group==1)&&!effect(b,o,0,0,0,e)) return 0;
    s->view=0;
    if(b->frame->group==0||b->frame->group==1) {
      *b->substate=5; b->frame->camera_cached=-1;
      if(!VOICE(7,0)) return 0;
      if((b->frame->group==0||b->frame->group==1)&&!effect(b,o,0,0,0,e)) return 0;
      b->saved->toggle=b->control->toggles[1]; b->control->toggles[1]=1;
      if(!pass_orbit(b,0,e)||!frame_camera(b,o,4,e)) return 0;
    } else {
      if(b->frame->group!=4) *b->substate=3;
      else { *b->substate=4; clear_counters(b); }
      b->frame->camera_cached=-1;
      if(!VOICE(7,0)) return 0;
      if(b->frame->group!=4&&(!base_orbit(b,1,e)||!frame_camera(b,o,2,e))) return 0;
    } break;
  default: break;
  } return 1;
}
static int middle_start(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  int busy; int32_t active; BkEndingClipTiming t;
  if(b->frame->group==0||b->frame->group==1) {
    if(!effect_busy(b,o,0,0,&busy,e)) return 0;
    if(!busy&&!b->counters[0]) { if(!effect(b,o,1,0,1,e)) return 0; b->counters[0]=1; }
  } else if(b->frame->group==2) {
    if(!marked_effect(b,o,5,105,0,1,1,e)) return 0;
  } else if(b->frame->group==3) {
    if(!marked_effect(b,o,5,95,0,1,2,e)) return 0;
    READ(5); if(!(t.source>90.f)) b->counters[0]=0;
  }
  if(b->frame->group==2||b->frame->group==3) {
    ACTIVE();
    if(active==5) { READ(5); if(!(t.source>=t.end)) return 1; }
    else { ACTIVE(); if(active!=6) return 1; }
    if(!playing(o,0,&busy,e)) return 0;
    if(busy) return 1;
    if(!VOICE(8,0)) return 0;
    if(b->frame->group==3&&(!effect_stop(b,o,0,1,e)||!effect_stop(b,o,0,2,e)||
        !effect(b,o,1,0,1,e)||!effect(b,o,1,1,1,e))) return 0;
  } else {
    READ(5); if(!(t.source>=t.end)) return 1;
    if(!playing(o,0,&busy,e)) return 0;
    if(busy) return 1;
    if(!CALL(request,6,e)) return 0;
    if((b->frame->group==0||b->frame->group==1)&&!CALL(expression,5,6,1,e)) return 0;
    if(!VOICE(8,0)) return 0;
    if(b->frame->group==0||b->frame->group==1) {
      if(!effect_busy(b,o,1,0,&busy,e)) return 0;
      if(!busy&&!effect(b,o,1,0,1,e)) return 0;
    }
  }
  *b->opening=1; clear_counters(b); return 1;
}
static int near_end_effect(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  BkEndingClipTiming t; READ(6);
  if(!((double)t.end-10.0>(double)t.source)&&!b->counters[0]) {
    if(!effect(b,o,1,2,0,e)) return 0;
    b->counters[0]=1;
  }
  READ(6); if(!(t.source>120.f)) b->counters[0]=0; return 1;
}
static int finish_effects(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  int busy; BkEndingClipTiming t;
  if(b->frame->group==2) {
    READ(7);
    if(t.source>=160.f&&b->counters[0]<2) {
      if(!effect_busy(b,o,2,1,&busy,e)) return 0;
      if(!busy) { if(!effect(b,o,2,1,0,e)) return 0; b->counters[0]=plus_one(b->counters[0]); }
    }
  } else if(b->frame->group==3) {
    if(!marked_effect(b,o,7,152,0,2,0,e)) return 0;
    if(b->counters[0]==1&&!b->counters[1]) {
      if(!effect_busy(b,o,2,0,&busy,e)) return 0;
      if(!busy) { if(!effect_stop(b,o,1,0,e)||!effect(b,o,2,1,0,e)) return 0; b->counters[1]=1; }
    }
    if(!marked_effect(b,o,7,154,3,2,3,e)) return 0;
    if(b->counters[1]==1&&!b->counters[2]) {
      if(!effect_busy(b,o,2,1,&busy,e)) return 0;
      if(!busy) { if(!effect(b,o,2,4,0,e)) return 0; b->counters[2]=1; }
    }
    if(!marked_effect(b,o,7,160,4,2,0,e)) return 0;
    if(b->counters[4]==1&&!b->counters[5]) {
      if(!effect_busy(b,o,2,0,&busy,e)) return 0;
      if(!busy) { if(!effect_stop(b,o,1,1,e)||!effect(b,o,2,2,0,e)) return 0; b->counters[5]=1; }
    }
    if(!marked_effect(b,o,7,162,7,2,3,e)) return 0;
    if(b->counters[5]==1&&!b->counters[6]) {
      if(!effect_busy(b,o,2,2,&busy,e)) return 0;
      if(!busy) { if(!effect(b,o,2,4,0,e)) return 0; b->counters[6]=1; }
    }
  } return 1;
}
static int wait_finish(const BkEndingGalleryAuxiliaryBindings *b,float seconds,
    const BkEndingGalleryAuxiliaryOps *o,int *done,char e[256]) {
  float timer; memcpy(&timer,b->elapsed_bits,4); timer+=seconds; memcpy(b->elapsed_bits,&timer,4);
  *done=0;
  if(timer>20.f) {
    int busy; if(!playing(o,0,&busy,e)) return 0;
    if(!busy) {
      *b->elapsed_bits=0; *b->opening=0; b->frame->state_721ee0=0;
      *b->cursor=plus_one(*b->cursor); clear_counters(b); *done=1;
    }
  } return 1;
}
static int middle(BkEndingGalleryAuxiliaryState *s,const BkEndingGalleryAuxiliaryBindings *b,
    float seconds,const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  int busy,done; int32_t active; BkEndingClipTiming t;
  switch(sb(*b->opening)) {
  case 0: return middle_start(b,o,e);
  case 1:
    if(b->frame->group==2) {
      if(!marked_effect(b,o,6,125,0,1,0,e)||!marked_effect(b,o,6,140,1,1,1,e)) return 0;
      READ(6); if(!(t.source>120.f)) b->counters[0]=b->counters[1]=0;
    } else if(b->frame->group==3&&!near_end_effect(b,o,e)) return 0;
    if(!playing(o,0,&busy,e)) return 0;
    if(!busy) {
      if(b->control->toggles[7]&&!VOICE(9,1)) return 0;
      clear_counters(b); *b->opening=2;
    } break;
  case 2:
    if(b->frame->group==3&&!near_end_effect(b,o,e)) return 0;
    if(!playing(o,1,&busy,e)) return 0;
    if(!busy) {
      b->auxiliary->index=b->frame->group!=2?73:72;
      if(!CALL(request,7,e)||!CALL(expression,6,4,0,e)) return 0;
      *b->expression_override=6; *b->face_mode=1;
      if(!VOICE(10,0)) return 0;
      *b->opening=3;
      if(b->frame->group==0||b->frame->group==1||b->frame->group==2) {
        if(!effect_stop(b,o,1,0,e)||!effect(b,o,2,0,0,e)) return 0;
      }
      if(b->frame->group==2||b->frame->group==3) {
        s->view=0; b->frame->camera_mode=4; *b->substate=5;
        save_orbit(b); s->view=0;
        b->saved->toggle=b->control->toggles[1]; b->control->toggles[1]=1;
        if(!pass_orbit(b,0,e)) return 0;
        memcpy(b->saved->target,b->frame->camera_values,12);
        if(!target(b,o,b->frame->group==2?13:5,e)) return 0;
      }
    } break;
  case 3:
    ACTIVE();
    if(active==7) {
      if(b->frame->group==0) {
        b->frame->camera_mode=2;
        if(!target(b,o,13,e)) return 0;
        b->control->target_choice=0;
        READ(7);
        if(t.source>220.f&&b->auxiliary->pending!=100) {
          if(!CALL(play,9,0,*b->effect_volume,e)) return 0;
          b->auxiliary->pending=100;
        }
      } else if(!finish_effects(b,o,e)) return 0;
      READ(7);
      if(t.source>=t.end&&!CALL(request,b->frame->group==0?9:8,e)) return 0;
    }
    ACTIVE();
    if(b->frame->group>=5) return fail(e,"finish clip group outside table");
    if(active==finish_clips[b->frame->group]) {
      if(!CALL(expression,0,4,0,e)) return 0;
      *b->expression_override=0;
      if(!playing(o,0,&busy,e)) return 0;
      if(!busy) { if(!VOICE(11,0)) return 0; *b->opening=4; *b->elapsed_bits=0; clear_counters(b); }
    } break;
  case 4: return wait_finish(b,seconds,o,&done,e);
  default: break;
  } return 1;
}
static int fourth_six_effects(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  int busy;
  if(!marked_effect(b,o,6,133,1,1,0,e)||!marked_effect(b,o,6,140,2,1,2,e)||
      !effect_busy(b,o,1,2,&busy,e)) return 0;
  if(!busy&&b->counters[2]==1&&!b->counters[8]) {
    if(!effect(b,o,1,1,0,e)) return 0;
    b->counters[8]=1;
  } return 1;
}
static int ninth_effects(const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  BkEndingClipTiming t;
  if(!marked_effect(b,o,9,257,6,2,0,e)||!marked_effect(b,o,9,264,7,2,0,e)) return 0;
  READ(9);
  if((double)t.start+6.0>=(double)t.source) b->counters[6]=b->counters[7]=0;
  return 1;
}
static int fourth(BkEndingGalleryAuxiliaryState *s,const BkEndingGalleryAuxiliaryBindings *b,
    float seconds,const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  int busy,done; int32_t active; BkEndingClipTiming t;
  if(*b->opening==0) {
    if(!playing(o,0,&busy,e)) return 0;
    if(!busy) {
      if(!b->counters[0]) {
        if(!CALL(request,6,e)) return 0;
        b->frame->camera_mode=4;
        if(!VOICE(8,0)) return 0;
        *b->substate=5; save_orbit(b);
        b->saved->toggle=b->control->toggles[1]; b->control->toggles[1]=1; s->view=0;
        if(!pass_orbit(b,0,e)) return 0;
        memcpy(b->saved->target,b->frame->camera_values,12);
        return target(b,o,5,e); /*native skips all remaining effects*/
      } else if(b->counters[0]==1) {
        b->counters[0]=2; if(!VOICE(8,0)) return 0;
      } else if(b->counters[0]==2) {
        b->counters[0]=3; if(b->control->toggles[7]&&!VOICE(9,1)) return 0;
      } else if(b->counters[0]==3) {
        if(!playing(o,1,&busy,e)) return 0;
        if(!busy) { b->counters[0]=4; if(!VOICE(10,0)) return 0; }
      } else if(b->counters[0]==4) {
        *b->opening=1; if(!VOICE(11,0)) return 0;
      }
    }
    ACTIVE();
    if(active==6) {
      if(!fourth_six_effects(b,o,e)) return 0;
      READ(6);
      if(t.source>=t.end) {
        if(!playing(o,0,&busy,e)) return 0;
        if(!busy&&!CALL(request,7,e)) return 0;
      }
    } else {
      ACTIVE();
      if(active==7) {
        READ(7);
        if(t.source>=160.f&&!b->counters[3]) {
          if(!CALL(expression,0,4,0,e)) return 0;
          *b->expression_override=0; *b->face_mode=1;
          if(!effect(b,o,2,0,0,e)) return 0;
          b->counters[3]=1;
        }
      } else {
        ACTIVE();
        if(active==8) {
          if(!marked_effect(b,o,8,195,4,2,0,e)||!marked_effect(b,o,8,225,5,2,0,e)) return 0;
        } else { ACTIVE(); if(active==9&&!ninth_effects(b,o,e)) return 0; }
      }
    }
  } else if(*b->opening==1) {
    if(!wait_finish(b,seconds,o,&done,e)) return 0;
    if(done) return 1;
    ACTIVE(); if(active==9&&!ninth_effects(b,o,e)) return 0;
  } return 1;
}
static int passes(BkEndingGalleryAuxiliaryState *s,const BkEndingGalleryAuxiliaryBindings *b,
    const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  int busy; int32_t active; BkEndingClipTiming t; ACTIVE(); READ(active);
  if(t.source>=t.end) {
    if(!playing(o,0,&busy,e)) return 0;
    if(!busy) {
      ++s->view;
      if(sb(s->view)==3) {
        if(b->frame->group==0||b->frame->group==1) {
          if(!CALL(expression,5,6,1,e)||!VOICE(8,0)||!effect_busy(b,o,1,0,&busy,e)) return 0;
          if(!busy&&!effect(b,o,1,0,1,e)) return 0;
          clear_counters(b); *b->opening=1; *b->substate=3;
          ACTIVE(); if(!CALL(request,plus_one(active),e)) return 0;
        } else if(b->frame->group==3||b->frame->group==2) {
          *b->substate=3; ACTIVE(); if(!CALL(request,plus_one(active),e)) return 0;
        } else { b->counters[0]=2; *b->substate=4; }
        b->frame->camera_mode=0; b->control->toggles[1]=b->saved->toggle;
        if(b->frame->group==0||b->frame->group==1||b->frame->group==4) {
          if(!base_orbit(b,0,e)) return 0;
        } else {
          orbit(b->camera,b->saved->orbit); memcpy(b->frame->camera_values,b->saved->target,12);
        }
      } else {
        if(b->frame->group==0||b->frame->group==1) {
          ACTIVE(); if(!CALL(restart,active,e)||!VOICE(7,0)||!effect(b,o,0,0,0,e)) return 0;
        } else if(b->frame->group==3||b->frame->group==2) {
          ACTIVE(); if(!CALL(restart,active,e)||!VOICE(10,0)) return 0;
          clear_counters(b);
          if(b->frame->group==3&&(!effect(b,o,1,0,1,e)||!effect(b,o,1,1,1,e))) return 0;
        } else {
          ACTIVE(); if(!CALL(restart,active,e)||!VOICE(8,0)) return 0; clear_counters(b);
        }
        if(!pass_orbit(b,sb(s->view),e)) return 0;
      }
    }
  }
  if(b->frame->group==2||b->frame->group==3) return finish_effects(b,o,e);
  if(b->frame->group==4) return fourth_six_effects(b,o,e);
  return 1;
}
int bk_ending_gallery_auxiliary_step(BkEndingGalleryAuxiliaryState *s,
    const BkEndingGalleryAuxiliaryBindings *b,float seconds,
    const BkEndingGalleryAuxiliaryOps *o,char e[256]) {
  if(!s||!b||!o||!b->frame||!b->control||!b->auxiliary||!b->camera||!b->presets||
      !b->saved||!b->substate||!b->opening||!b->cursor||!b->counters||!b->elapsed_bits||
      !b->expression_override||!b->face_mode||!b->effect_volume||!isfinite(seconds)||seconds<0)
    return fail(e,"invalid live bindings/time");
  switch(sb(*b->substate)) {
  case 0:return opening(s,b,seconds,o,e);
  case 1:
    if(!VOICE(3,0)||!CALL(request,2,e)||!CALL(expression,5,4,0,e)) return 0;
    *b->substate=2; return 1;
  case 2:return prelude(s,b,o,e);
  case 3:return middle(s,b,seconds,o,e);
  case 4:return fourth(s,b,seconds,o,e);
  case 5:return passes(s,b,o,e);
  default:return 1;
  }
}
#undef VOICE
#undef ACTIVE
#undef READ
#undef CALL
