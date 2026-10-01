#include "app/volume_session.h"
#include "scene/volume_menu_render.h"
#include "scene/volume_menu_audio.h"
#include "core/random.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkVolumeSession {
  BkVolumeSessionConfig c;
  BkVolumeMenu menu;
  BkVolumeMenuRender *render;
  BkVolumeMenuAudio *audio;
  BkVirtualPointer pointer;
  uint8_t touch;
  int stopped;
};
static int fail(char e[256],const char *why) {snprintf(e,256,"volume session: %s",why);return 0;}
static int play(void *p,unsigned slot,int32_t v,char e[256]) {return bk_volume_menu_audio_play(((BkVolumeSession *)p)->audio,slot,v,e);}
static int stop(void *p,unsigned slot,char e[256]) {return bk_volume_menu_audio_stop(((BkVolumeSession *)p)->audio,slot,e);}
static int gain(void *p,unsigned slot,int32_t v,char e[256]) {return bk_volume_menu_audio_gain(((BkVolumeSession *)p)->audio,slot,v,e);}
static int playing(void *p,unsigned slot,int *active,char e[256]) {return bk_volume_menu_audio_playing(((BkVolumeSession *)p)->audio,slot,active,e);}
static int random_value(void *p,uint32_t *value,char e[256]) {
  (void)e;BkVolumeSession *s=p;*value=bk_random_next(s->c.random);return 1;
}
static int warp(void *p,int32_t x,int32_t y,char e[256]) {
  (void)e;BkVolumeSession *s=p;s->pointer.position[0]=(float)x;s->pointer.position[1]=(float)y;return 1;
}
BkVolumeSession *bk_volume_session_create(const BkVolumeSessionConfig *c,char e[256]) {
  if(!c || !c->file || !c->common || !c->curtain || !c->random || !c->schedule ||
     (c->previous!=1 && c->previous!=4) || !c->services.audio ||
     c->services.audio_volumes != bk_volume_file_values(c->file)) {
    fail(e,"missing storage/state or unsupported entry");return NULL;
  }
  BkVolumeSession *s=calloc(1,sizeof(*s));if(!s) {fail(e,"allocation failed");return NULL;}
  s->c=*c;memcpy(s->pointer.position,c->pointer,sizeof(c->pointer));
  if(!bk_volume_menu_initialize(&s->menu,c->viewport.width,bk_volume_file_values(c->file),e))goto bad;
  s->render=bk_volume_menu_render_create(c->services.renderer,c->services.resources,e);
  /* Game occupies0..38; persistent UI uses48..56. These six voices remain
   * free both from title and while the live game is retained behind pause. */
  s->audio=bk_volume_menu_audio_create(c->services.resources,c->services.audio,40,e);
  if(!s->render || !s->audio)goto bad;
  return s;
bad:bk_volume_session_destroy(s);return NULL;
}
void bk_volume_session_destroy(BkVolumeSession *s) {
  if(!s)return;
  bk_volume_menu_audio_destroy(s->audio);bk_volume_menu_render_destroy(s->render);free(s);
}
int bk_volume_session_stop(BkVolumeSession *s,char e[256]) {
  if(!s)return fail(e,"missing owner");
  bk_volume_menu_audio_destroy(s->audio);s->audio=NULL;s->stopped=1;return 1;
}
static uint8_t key(const BkInput *in,uint32_t mask) {
  return (in->pressed&mask)?3:(in->held&mask)?1:(in->released&mask)?2:0;
}
int bk_volume_session_step(BkVolumeSession *s,double seconds,const BkInput *in,char e[256]) {
  if(!s || s->stopped || !in || !isfinite(seconds) || seconds<=0 || seconds>1)return fail(e,"invalid tick");
  BkInput pointer=*in;
  /* D-pad uses the native selection/slider keys. Left stick and touch use the
   * mouse path; do not also apply d-pad motion to that software pointer. */
  pointer.held&=~(BK_BUTTON_LEFT|BK_BUTTON_RIGHT|BK_BUTTON_UP|BK_BUTTON_DOWN);
  if(!bk_virtual_pointer_step(&s->pointer,&s->c.viewport,&pointer,seconds,e))return 0;
  uint8_t mouse=key(in,BK_BUTTON_CONFIRM);
  if(in->pointer_active)mouse=s->touch?1:3;
  else if(s->touch && !mouse)mouse=2;
  s->touch=in->pointer_active;
  BkVolumeMenuInput input={.x=(int32_t)s->pointer.position[0],.y=(int32_t)s->pointer.position[1],
      .mouse=mouse,.left=key(in,BK_BUTTON_LEFT),.right=key(in,BK_BUTTON_RIGHT),
      .up=key(in,BK_BUTTON_UP),.down=key(in,BK_BUTTON_DOWN),.back=key(in,BK_BUTTON_BACK),.fast=0};
  if(input.left || input.right || input.up || input.down)
    input.confirm=key(in,BK_BUTTON_CONFIRM);
  /* Native row5 is a hidden debug button. Keep its strict core behavior, but
   * prevent the Switch d-pad from placing focus on an invisible control. */
  if(s->menu.row==4) input.right=0;
  BkVolumeMenuFrame frame;BkVolumeMenuOps ops={s,play,stop,gain,playing,random_value,warp};
  int action;
  if(!bk_volume_menu_prepare(&s->menu,&frame,e) ||
     !bk_volume_menu_render_prepare(s->render,&frame,s->c.viewport.width,s->c.viewport.height,e) ||
     !bk_volume_menu_step(&s->menu,&input,&ops,&action,e))return 0;
  if(action==3) {
    /* Switch defaults are MAX for all categories. The original controller
     * remains unchanged; reset has stopped previews, and the next tick
     * applies these slider values before any new preview or save. */
    for(unsigned i=0;i<3;++i) {
      s->menu.values[i]=0;
      s->menu.slider[i]=s->menu.maximum;
    }
  } else if(action==4) {
    if(!bk_volume_file_store(s->c.file,s->menu.values,e))return 0;
    s->c.common->blocked=1;
  }
  BkCommonHudState *common=s->c.common;
  if(!bk_fade_sprite_advance(&common->curtain,(float)seconds))return fail(e,"invalid curtain");
  BkCommonHudFrame curtain={common->curtain.alpha};
  if(!bk_curtain_render_prepare(s->c.curtain,&curtain,s->c.viewport.width,s->c.viewport.height,e) ||
     !bk_fade_sprite_request(&common->curtain,common->blocked))return fail(e,"curtain request failed");
  if(common->blocked==1 && common->curtain.stage==3) {
    common->blocked=0;
    return s->c.schedule(s->c.context,s->c.previous,0,e);
  }
  return 1;
}
int bk_volume_session_draw(BkVolumeSession *s,char e[256]) {
  return s && bk_volume_menu_render_draw(s->render,e) && bk_curtain_render_draw(s->c.curtain,e);
}
const BkVirtualPointer *bk_volume_session_pointer(const BkVolumeSession *s) {return s?&s->pointer:NULL;}
