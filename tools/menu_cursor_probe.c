/* Actual title/selection adapters, original UI/rendering and controller input.
 * The loader boundary switches flows; no private menu/cursor state is exposed. */
#include "app/front_end.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){if(!*e)snprintf(e,256,"line%d: %s",__LINE__,#x);goto done;}}while(0)
typedef struct {
  BkRenderer *renderer;
  BkAudio *audio;
  BkFrontEnd *front;
  BkMenuCursor cursor;
  BkFlowTransition flow;
  uint64_t submitted;
  unsigned frames, visible;
  double wall;
} Run;
static int submit(void *p,const int16_t *pcm,size_t n,char e[256]) {
  (void)pcm;(void)e;((Run *)p)->submitted+=n;return 1;
}
static int poll(void *p,uint64_t *out,char e[256]) {(void)e;*out=((Run *)p)->submitted;return 1;}
static int schedule(void *p,uint8_t target,uint8_t mode,char e[256]) {
  (void)e;Run *r=p;r->flow=(BkFlowTransition){0x50,r->flow.current,target,mode};return 1;
}
static int draw(Run *r,char e[256]) {
  return bk_renderer_begin(r->renderer,e) && bk_front_end_draw(r->front,e) &&
      bk_renderer_end(r->renderer,e);
}
static int tick(Run *r,BkInput in,char e[256]) {
  r->wall+=1./60;++r->frames;
  if(!bk_audio_poll(r->audio,e) || !bk_front_end_step(r->front,1./60,r->wall,&in,e) ||
     !bk_audio_fill(r->audio,e) || !draw(r,e) || !bk_front_end_after_present(r->front,e))return 0;
  if(r->cursor.sprite.fade.alpha!=1 || r->cursor.sprite.fade.stage!=3 || !r->cursor.wanted) {
    snprintf(e,256,"cursor hidden in flow%02x frame%u",r->flow.current,r->frames);return 0;
  }
  ++r->visible;return 1;
}
static int idle(Run *r,unsigned frames,char e[256]) {
  while(frames--)if(!tick(r,(BkInput){0},e))return 0;
  return 1;
}
static int move_to(Run *r,float x,float y,char e[256]) {
  for(unsigned i=0;i<240;++i) {
    double dx=x-r->cursor.sprite.rect[0],dy=y-r->cursor.sprite.rect[1],d=hypot(dx,dy);
    if(d<.02)return 1;
    double magnitude=.18+.82*fmin(d/8.,1.);
    if(!tick(r,(BkInput){.move_x=(float)(dx/d*magnitude),.move_y=(float)(-dy/d*magnitude)},e))return 0;
  }
  snprintf(e,256,"stick did not reach %.2f,%.2f",x,y);return 0;
}
static int capture(Run *r,const char *path,char e[256]) {
  const size_t bytes=1280*720*4;uint8_t *pixels=malloc(bytes),*again=malloc(bytes);int ok=0;
  if(pixels && again && bk_renderer_readback(r->renderer,pixels,bytes,e)) {
    BkMenuCursor before=r->cursor;
    if(draw(r,e) && bk_renderer_readback(r->renderer,again,bytes,e) &&
       !memcmp(pixels,again,bytes) && !memcmp(&before,&r->cursor,sizeof(before))) {
      FILE *f=fopen(path,"wb");if(f){size_t n=fwrite(pixels,1,bytes,f);int closed=fclose(f);ok=n==bytes && !closed;}
    }
  }
  free(pixels);free(again);return ok;
}
int main(int argc,char **argv) {
  if(argc!=3)return 2;
  char e[256]={0},path[2048];int result=1;Run r={.wall=1};
  BkResourceStore *store=bk_resources_create(e);BkCurtainRender *curtain=NULL;
  BkCommonHudState common={0};uint8_t hover=0;uint32_t group=0,area=0,random=1234;
  int32_t photos[5]={0},count=0;
  CHECK(store);
  const char *packs[]={"bk3_00","bk3_01","bk3_02","bk3_03","bk3_04","bk3_06"};
  for(unsigned i=0;i<6;++i){snprintf(path,sizeof(path),"%s/%s.pp",argv[1],packs[i]);CHECK(bk_resources_mount(store,packs[i],path,e));}
  CHECK(bk_resources_mount_directory(store,"faces",argv[1],20480,e));
  r.renderer=bk_renderer_create(1280,720,stderr,e);CHECK(r.renderer);
  BkRenderStats baseline=bk_renderer_stats(r.renderer);
  r.audio=bk_audio_create(&(BkAudioSink){&r,48000,240,960,submit,poll},e);CHECK(r.audio);
  curtain=bk_curtain_render_create(r.renderer,store,e);CHECK(curtain);
  CHECK(bk_menu_cursor_initialize(&r.cursor,960,720));bk_common_hud_initialize(&common);
  BkFrontEndConfig config={.services={store,r.renderer,stderr,r.audio,NULL,NULL},
      .viewport={160,0,960,720},.common=&common,.curtain=curtain,.flow=&r.flow,
      .cursor=&r.cursor,.hover=&hover,.group=&group,.area=&area,.random=&random,
      .photos=photos,.photo_count=&count,.context=&r,.schedule=schedule};
  r.front=bk_front_end_create(&config,e);CHECK(r.front);
  unsigned movements=0;
  for(unsigned page=0;page<2;++page) {
    unsigned flow=page?0x38:1;
    r.flow=(BkFlowTransition){flow,page?1:0,0,0};
    CHECK(bk_front_end_load(r.front,flow,r.flow.previous,1,r.wall,1.f/60,e));
    /* Emulate a previous screen's hidden cursor, then idle beyond10seconds. */
    r.cursor.wanted=0;r.cursor.sprite.fade.alpha=0;r.cursor.sprite.fade.stage=0;
    r.cursor.idle.armed=1;r.cursor.idle.deadline=0;
    CHECK(idle(&r,720,e));
    CHECK(move_to(&r,480,360,e));
    const BkInput axes[]={{.move_x=1},{.move_y=1},{.move_x=-1},{.move_y=-1}};
    const float delta[][2]={{8,0},{0,-8},{-8,0},{0,8}};
    for(unsigned i=0;i<4;++i) {
      float x=r.cursor.sprite.rect[0],y=r.cursor.sprite.rect[1];
      CHECK(tick(&r,axes[i],e));
      CHECK(fabs(r.cursor.sprite.rect[0]-x-delta[i][0])<.001 && fabs(r.cursor.sprite.rect[1]-y-delta[i][1])<.001);++movements;
    }
    CHECK(tick(&r,(BkInput){.move_x=1,.held=BK_BUTTON_SLOW},e));
    CHECK(fabs(r.cursor.sprite.rect[0]-482)<.001);
    CHECK(tick(&r,(BkInput){.pointer_active=1,.pointer_x=320,.pointer_y=200},e));
    CHECK(r.cursor.sprite.rect[0]==160 && r.cursor.sprite.rect[1]==200);
    CHECK(tick(&r,(BkInput){.move_x=1},e) && r.cursor.sprite.rect[0]==168);
    CHECK(tick(&r,(BkInput){.pressed=BK_BUTTON_DOWN,.held=BK_BUTTON_DOWN},e));
    CHECK(tick(&r,(BkInput){.held=BK_BUTTON_DOWN},e));
    float x=r.cursor.sprite.rect[0],y=r.cursor.sprite.rect[1];
    for(unsigned i=0;i<12;++i)CHECK(tick(&r,(BkInput){.held=BK_BUTTON_DOWN},e));
    CHECK(r.cursor.sprite.rect[0]==x && r.cursor.sprite.rect[1]==y);
    CHECK(move_to(&r,480,360,e));
    snprintf(path,sizeof(path),"%s/%s.rgba",argv[2],page?"selection":"title");CHECK(capture(&r,path,e));
    /* Stick-only focus then A dispatches the real native button. */
    CHECK(move_to(&r,(page?1104:1084)*.75f,(page?908:53)*.75f,e));
    CHECK(tick(&r,(BkInput){.pressed=BK_BUTTON_CONFIRM},e));
    for(unsigned i=0;i<240 && r.flow.current==flow;++i)CHECK(tick(&r,(BkInput){0},e));
    CHECK(r.flow.current==0x50 && r.flow.target==(page?1:0x38));
    bk_front_end_collect(r.front);
  }
  bk_front_end_destroy(r.front);r.front=NULL;bk_curtain_render_destroy(curtain);curtain=NULL;
  CHECK(bk_renderer_stats(r.renderer).live_allocations==baseline.live_allocations &&
        bk_renderer_stats(r.renderer).live_bytes==baseline.live_bytes);
  printf("PASS menu-cursor frames=%u visible=%u axes=%u idle-seconds=12x2 stick-touch-dpad-confirm redraw=2 GPU-baseline\n",r.frames,r.visible,movements);
  result=0;
done:
  if(result)fprintf(stderr,"menu-cursor FAIL frame%u: %s\n",r.frames,e);
  bk_front_end_destroy(r.front);bk_curtain_render_destroy(curtain);bk_audio_destroy(r.audio);
  bk_renderer_destroy(r.renderer);bk_resources_destroy(store);return result;
}
