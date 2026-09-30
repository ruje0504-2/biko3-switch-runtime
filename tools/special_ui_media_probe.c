/* Real Japanese UI/PCM and app-owned album files. Input, time, camera values
 * and the solid pre-UI background are explicit fixtures, not a flow48 scene. */
#include "app/capture_output.h"
#include "scene/special_capture.h"
#include "scene/special_ui_render.h"
#include "scene/system_audio.h"
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 640
#define H 480
#define BYTES (W*H*4)
#define CHECK(x) do { if (!(x)) { if (!*e) snprintf(e,256,"line%d: %s",__LINE__,#x); goto done; } } while (0)
typedef struct { uint64_t submitted, consumed, samples, nonzero, hash; } Sink;
static int submit(void *ctx, const int16_t *pcm, size_t n, char e[256]) {
  (void)e; Sink *s=ctx;
  for(size_t i=0;i<n*2;++i) {
    ++s->samples;s->nonzero+=pcm[i]!=0;
    for(unsigned b=0;b<2;++b) {s->hash^=((uint16_t)pcm[i]>>(8*b))&255;s->hash*=UINT64_C(1099511628211);}
  }
  s->submitted+=n;return 1;
}
static int poll(void *ctx,uint64_t *n,char e[256]) {
  (void)e;*n=((Sink*)ctx)->consumed;return 1;
}
typedef struct {
  BkSpecialUiRender *render; BkResourceStore *store;
  BkSystemAudio *sounds[8]; BkImage images[30], watermark;
  BkSpecialCaptureService capture; BkCaptureOutput output;
  BkCaptureTime time; const char *root;
  float point[2], motion[2]; uint32_t press, held, now;
  unsigned calls, loads, sound_mask, plays, schedules, writes, failures, fail_clock;
  unsigned capture_calls[3], pixel_samples, watermark_samples, last_group;
  uint8_t color[3]; uint64_t file_hash;
} Services;
static int clock_read(void *ctx,BkCaptureTime *time,char e[256]) {
  Services*s=ctx;
  if(s->fail_clock) {--s->fail_clock;++s->failures;snprintf(e,256,"injected capture clock failure");return 0;}
  s->time=(BkCaptureTime){2026,9,30,12,34,56,s->writes+1};*time=s->time;return 1;
}
static int write_photo(void *ctx,int photo,unsigned group,const BkBlob *bmp,char e[256]) {
  Services*s=ctx; BkImage im={0};
  if(!photo || !bk_image_decode(bmp->data,bmp->size,&im,e))return 0;
  int ok=im.width==s->capture.crop.width && im.height==s->capture.crop.height;
  for(unsigned y=0;ok&&y<im.height;++y)for(unsigned x=0;ok&&x<im.width;++x) {
    const uint8_t *want=s->color;
    if(x>=im.width-136 && x<im.width-8 && y>=im.height-40 && y<im.height-8) {
      const uint8_t *p=s->watermark.rgba+((size_t)(y-(im.height-40))*s->watermark.width+x-(im.width-136))*4;
      if(!(p[0]==255 && p[1]==0 && p[2]==0)) {want=p;++s->watermark_samples;}
    }
    if(memcmp(im.rgba+((size_t)y*im.width+x)*4,want,3)) {
      snprintf(e,256,"photo includes UI/wrong frame at %u,%u",x,y);ok=0;
    }
    s->pixel_samples+=3;
  }
  bk_image_free(&im);if(!ok)return 0;
  BkScreenshotOutput out=bk_capture_output_service(&s->output);
  if(!out.write(out.context,photo,group,bmp,e))return 0;
  char name[128],path[1600];
  if(!bk_capture_photo_name(name,group,&s->time))return 0;
  snprintf(path,sizeof path,"%s/album/%s",s->root,name);
  FILE*f=fopen(path,"rb");if(!f)return 0;
  for(size_t i=0;i<bmp->size;++i) {
    int byte=fgetc(f);if(byte<0 || byte!=((const uint8_t*)bmp->data)[i]){ok=0;break;}
    s->file_hash^=(unsigned)byte;s->file_hash*=UINT64_C(1099511628211);
  }
  if(fgetc(f)!=EOF)ok=0;fclose(f);if(!ok)return 0;
  ++s->writes;s->last_group=group;return 1;
}
static int image_load(void*ctx,unsigned slot,const char*name,char e[256]) {
  Services*s=ctx;++s->calls;
  if(!bk_special_ui_render_image(s->render,slot,name,e))return 0;
  bk_image_free(&s->images[slot]);if(!name)return 1;
  BkBlob raw={0};if(bk_resources_read(s->store,"bk3_00",name,&raw,e)!=BK_RESOURCE_OK)return 0;
  int ok=bk_image_decode(raw.data,raw.size,&s->images[slot],e);bk_blob_free(&raw);++s->loads;return ok;
}
static int capture(void*ctx,BkSpecialCapture op,char e[256]) {
  Services*s=ctx;++s->calls;++s->capture_calls[op];return bk_special_capture_call(&s->capture,op,e);
}
static int position(void*ctx,float out[2],char e[256]) {
  (void)e;Services*s=ctx;++s->calls;memcpy(out,s->point,8);return 1;
}
static int motion(void*ctx,float out[2],char e[256]) {
  (void)e;Services*s=ctx;++s->calls;memcpy(out,s->motion,8);return 1;
}
static int key(void*ctx,uint32_t code,unsigned mode,uint32_t*out,char e[256]) {
  (void)e;Services*s=ctx;++s->calls;*out=code==(mode==1?s->press:s->held);return 1;
}
static int sound(void*ctx,unsigned slot,char e[256]) {
  Services*s=ctx;++s->calls;if(slot>=8||!s->sounds[slot])return 0;
  s->sound_mask|=1u<<slot;++s->plays;return bk_system_audio_restart(s->sounds[slot],e);
}
static int clock_ms(void*ctx,uint32_t*out,char e[256]) {
  (void)e;Services*s=ctx;++s->calls;*out=s->now;return 1;
}
static int warp(void*ctx,float x,float y,char e[256]) {
  (void)e;Services*s=ctx;++s->calls;s->point[0]=x;s->point[1]=y;return 1;
}
static int schedule(void*ctx,uint8_t flow,uint8_t mode,char e[256]) {
  Services*s=ctx;++s->calls;if(flow!=0x18||mode!=1){snprintf(e,256,"unexpected exit request");return 0;}
  ++s->schedules;return 1;
}
static int background(BkRenderer*r,BkTexture*t,const uint8_t color[3],const BkViewport*vp,char e[256]) {
  float red=color[0]/255.f,green=color[1]/255.f,blue=color[2]/255.f;
  BkVertex v[6]={{-1,-1,.8f,0,0,red,green,blue,1},{1,-1,.8f,1,0,red,green,blue,1},
    {1,1,.8f,1,1,red,green,blue,1},{-1,-1,.8f,0,0,red,green,blue,1},
    {1,1,.8f,1,1,red,green,blue,1},{-1,1,.8f,0,1,red,green,blue,1}};
  return bk_renderer_begin(r,e)&&bk_renderer_viewport(r,NULL,e)&&
    bk_renderer_draw(r,t,v,6,bk_identity,e)&&bk_renderer_viewport(r,vp,e);
}
static double texel(const BkImage*im,int x,int y,unsigned c) {
  int w=(int)im->width,h=(int)im->height;x=(x%w+w)%w;y=(y%h+h)%h;
  return im->rgba[((size_t)y*w+x)*4+c]/255.;
}
static double sample(const BkImage*im,double u,double v,unsigned c) {
  double x=u*im->width-.5,y=v*im->height-.5;int l=(int)floor(x),t=(int)floor(y);
  double fx=x-l,fy=y-t;
  return (texel(im,l,t,c)*(1-fx)+texel(im,l+1,t,c)*fx)*(1-fy)+
    (texel(im,l,t+1,c)*(1-fx)+texel(im,l+1,t+1,c)*fx)*fy;
}
static int expected(Services*s,const BkSpecialUiFrame*f,const BkImage*curtain,
    unsigned width,unsigned x,unsigned y,int out[3]) {
  for(unsigned c=0;c<3;++c)out[c]=s->color[c];
  for(unsigned i=0;i<=f->sprites.count;++i) {
    BkEndingUiDraw cd={.xy={0,0,width,0,width,width*.75f,0,width*.75f},
      .uv={0,0,1,1},.alpha=f->curtain.curtain_alpha,.rgb=0xffffff};
    const BkEndingUiDraw*d=i==f->sprites.count?&cd:&f->sprites.draws[i];
    const BkImage*im=i==f->sprites.count?curtain:&s->images[d->slot];
    double l=d->xy[0],t=d->xy[1],w=d->xy[2]-l,h=d->xy[7]-t;
    if(fabs(x-l)<.03||fabs(y-t)<.03||fabs(x-l-w)<.03||fabs(y-t-h)<.03)return 0;
    if(x<l||x>=l+w||y<t||y>=t+h)continue;
    double u=d->uv[0]+(x-l)/w*(d->uv[2]-d->uv[0]);
    double v=d->uv[1]+(y-t)/h*(d->uv[3]-d->uv[1]);
    double a=sample(im,u,v,3)*(unsigned)((double)d->alpha*255)/255.;
    for(unsigned c=0;c<3;++c)out[c]=(int)lround(sample(im,u,v,c)*((d->rgb>>(16-8*c))&255)*a+out[c]*(1-a));
  }
  return 1;
}
static void target(Services*s,BkSpecialUi*ui,unsigned slot) {
  const float*r=ui->sprites[slot].rect;s->point[0]=r[0]+r[2]/2;s->point[1]=r[1]+r[3]/2;
}
int main(int argc,char**argv) {
  if(argc!=3)return 2;char e[256]={0},path[1600];int rc=1;
  BkResourceStore*store=bk_resources_create(e);BkRenderer*r=NULL;BkTexture*white=NULL;
  BkAudio*audio=NULL;BkCurtainRender*curtain=NULL;BkScreenshot*shot=NULL;BkCaptureFiles*files=NULL;
  BkImage curtain_image={0};BkBlob raw={0};uint8_t*pixels=malloc(BYTES),*again=malloc(BYTES);
  Sink sink={.hash=UINT64_C(14695981039346656037)};
  Services s={.store=store,.root=argv[2],.file_hash=UINT64_C(14695981039346656037)};
  unsigned frames=0,redraws=0,samples=0,worst=0,rejects=0,curtains=0,counters=0;
  uint64_t pixels_hash=UINT64_C(14695981039346656037);
  CHECK(store&&pixels&&again);
  const char*packs[]={"bk3_00","bk3_02","bk3_15"};
  for(unsigned i=0;i<3;++i){snprintf(path,sizeof path,"%s/%s.pp",argv[1],packs[i]);CHECK(bk_resources_mount(store,packs[i],path,e));}
  CHECK(bk_resources_read(store,"bk3_00","ma_01.tga",&raw,e)==BK_RESOURCE_OK);
  CHECK(bk_image_decode(raw.data,raw.size,&curtain_image,e));bk_blob_free(&raw);
  CHECK(bk_resources_read(store,"bk3_15","cp.bmp",&raw,e)==BK_RESOURCE_OK);
  CHECK(bk_image_decode(raw.data,raw.size,&s.watermark,e));bk_blob_free(&raw);
  CHECK(s.watermark.width>=128&&s.watermark.height>=32);
  r=bk_renderer_create(W,H,stderr,e);CHECK(r);
  uint8_t rgba[]={255,255,255,255};BkImage im={1,1,rgba};white=bk_texture_create(r,&im,e);CHECK(white);
  curtain=bk_curtain_render_create(r,store,e);CHECK(curtain);
  files=bk_capture_files_create(argv[2],e);CHECK(files);s.output=(BkCaptureOutput){files,{&s,clock_read}};
  BkScreenshotOutput output={&s,write_photo};shot=bk_screenshot_create(r,store,&output,e);CHECK(shot);
  BkAudioSink output_audio={&sink,48000,240,960,submit,poll};audio=bk_audio_create(&output_audio,e);CHECK(audio);
  for(unsigned i=0;i<8;++i){s.sounds[i]=bk_system_audio_create_slot(store,audio,48+i,i,-600,e);CHECK(s.sounds[i]);}
  int32_t group=2,count=0,photos[5]={0};unsigned album=4;
  s.capture=(BkSpecialCaptureService){shot,{0,0,W,H},&group,&album,&count,photos};
  s.color[0]=70;s.color[1]=110;s.color[2]=160;
  /* No invented default configuration, and failed flush retains the request. */
  CHECK(bk_screenshot_trigger(shot,e));CHECK(!capture(&s,BK_SPECIAL_CAPTURE_FLUSH,e));e[0]=0;++rejects;
  CHECK(capture(&s,BK_SPECIAL_CAPTURE_CONFIGURE,e));CHECK(count==1&&photos[2]==1);
  CHECK(background(r,white,s.color,&s.capture.crop,e));CHECK(capture(&s,BK_SPECIAL_CAPTURE_FLUSH,e));CHECK(bk_renderer_end(r,e));
  CHECK(s.writes==1&&s.last_group==4);
  /* Actual output clock failure: counter is already committed, pending retries. */
  CHECK(capture(&s,BK_SPECIAL_CAPTURE_REQUEST,e));CHECK(capture(&s,BK_SPECIAL_CAPTURE_CONFIGURE,e));
  s.fail_clock=1;CHECK(background(r,white,s.color,&s.capture.crop,e));
  CHECK(!capture(&s,BK_SPECIAL_CAPTURE_FLUSH,e));CHECK(count==2&&photos[2]==2&&s.writes==1);e[0]=0;++rejects;
  CHECK(capture(&s,BK_SPECIAL_CAPTURE_FLUSH,e));CHECK(bk_renderer_end(r,e));CHECK(s.writes==2);
  /* Configure alone never arms; count arithmetic is native signed-wrap then cap. */
  const int32_t counts[]={0,99,100,INT32_MAX,INT32_MIN,-1};
  const int32_t after[]={1,100,100,INT32_MIN,INT32_MIN+1,0};
  for(unsigned i=0;i<6;++i){count=counts[i];CHECK(capture(&s,BK_SPECIAL_CAPTURE_CONFIGURE,e));CHECK(count==after[i]&&photos[2]==count);++counters;}
  CHECK(capture(&s,BK_SPECIAL_CAPTURE_FLUSH,e));CHECK(s.writes==2);
  for(unsigned extent=0;extent<2;++extent) {
    unsigned width=extent?503:640,height=extent?377:480;
    BkViewport vp={(W-width)/2,(H-height)/2,width,height};s.capture.crop=vp;
    s.render=bk_special_ui_render_create(r,store,curtain,e);CHECK(s.render);
    BkSpecialUi ui={0};BkSpecialUiControl control={0};BkSpecialEventState event={0};
    BkSpecialUiOps ops={&s,image_load,capture,position,motion,key,sound,clock_ms,warp,schedule};
    for(int8_t mode=0;mode<2;++mode) {
      CHECK(bk_special_ui_initialize(&ui,&control,&event,width,mode,&ops,e));
      CHECK(mode==0 || (s.images[15].rgba&&s.images[16].rgba)); /*skipped constructors retain*/
      BkCommonHudState common={.curtain={0,2,0}};BkMenuCamera camera={0};BkEndingCameraPresets presets={0};
      for(unsigned a=0;a<4;++a)for(unsigned b=0;b<3;++b)presets.active[a][b]=presets.authored[a][b]=(float)(a+b+1);
      int32_t phase=2,clip=0,cmode=0;uint8_t previous=0x18,visible=0,hover=0;count=98;
      BkSpecialUiBindings bindings={&common,&control,&camera,&presets,&phase,&clip,&cmode,&count,&mode,&previous,&visible,&hover};
      unsigned requests=s.capture_calls[BK_SPECIAL_CAPTURE_REQUEST],before_writes=s.writes;
      for(unsigned tick=0;tick<240;++tick) {
        s.now+=17;s.press=s.held=UINT32_MAX;s.motion[0]=1;s.motion[1]=0;
        s.point[0]=1184*width/1280.f;s.point[1]=90*width/1280.f;
        if(tick>=25&&tick<150) {unsigned slots[]={3,7,10,12};target(&s,&ui,slots[(tick/20)%4]);if(tick%20==5)s.press=0;}
        if(tick==20||tick==40||tick==60)s.press=0x43;
        if(tick==75)s.press=0x28;
        if(tick==85)s.press=0x26;
        if(tick>=110&&tick<116){s.point[0]=120;s.point[1]=160;s.held=tick%2;}
        if(tick==156){target(&s,&ui,5);s.press=0;}
        if(tick==230){CHECK(phase==0);phase=1;common.blocked=0;}
        s.color[0]=(uint8_t)(70+tick%80);
        sink.consumed=sink.submitted;CHECK(bk_audio_poll(audio,e));
        CHECK(bk_special_ui_render_begin(s.render,e));
        CHECK(background(r,white,s.color,&vp,e));
        BkSpecialUiFrame frame;int32_t old_count=count;
        CHECK(bk_special_ui_step(&ui,&bindings,&ops,width/1280.f,1.f/60,&frame,e));
        if(tick==20&&mode==0){CHECK(old_count==98&&count==99);CHECK(frame.sprites.draws[3].slot==25);}
        if(tick==40&&mode==0){CHECK(old_count==99&&count==100);CHECK(frame.sprites.draws[2].slot==26&&frame.sprites.draws[3].slot==26);CHECK(frame.sprites.draws[2].xy[0]!=frame.sprites.draws[3].xy[0]);}
        CHECK(bk_special_ui_render_prepare(s.render,&frame,width,height,e));
        CHECK(bk_special_ui_render_draw(s.render,e));CHECK(bk_renderer_end(r,e));
        CHECK(bk_renderer_readback(r,pixels,BYTES,e));CHECK(bk_audio_fill(audio,e));
        for(unsigned y=1;y<height;y+=13)for(unsigned x=1;x<width;x+=13) {
          int rgb[3];if(!expected(&s,&frame,&curtain_image,width,x,y,rgb))continue;
          for(unsigned c=0;c<3;++c) {
            unsigned pixel=pixels[((size_t)(y+vp.y)*W+x+vp.x)*4+c];unsigned delta=(unsigned)abs((int)pixel-rgb[c]);if(delta>worst)worst=delta;
            if(delta>1){snprintf(e,256,"UI pixel extent%u mode%d tick%u xy%u,%u c%u got%u want%d",extent,mode,tick,x,y,c,pixel,rgb[c]);goto done;}
            ++samples;pixels_hash^=pixel;pixels_hash*=UINT64_C(1099511628211);
          }
        }
        if(tick%15==0) {
          BkSpecialUi saved=ui;BkCommonHudState saved_common=common;unsigned calls=s.calls,writes=s.writes;uint64_t pcm=sink.hash;
          CHECK(background(r,white,s.color,&vp,e));CHECK(bk_special_ui_render_draw(s.render,e));CHECK(bk_renderer_end(r,e));CHECK(bk_renderer_readback(r,again,BYTES,e));
          CHECK(!memcmp(pixels,again,BYTES)&&!memcmp(&ui,&saved,sizeof ui)&&!memcmp(&common,&saved_common,sizeof common)&&calls==s.calls&&writes==s.writes&&pcm==sink.hash);++redraws;
        }
        ++frames;
      }
      CHECK(mode==0?(count==100&&s.writes==before_writes+2):(count==98&&s.writes==before_writes));
      CHECK(s.capture_calls[BK_SPECIAL_CAPTURE_REQUEST]==requests+(mode==0?2:0));
    }
    /* Pin images, then replace/release current resources. Snapshot stays exact. */
    BkSpecialUiFrame retained={.complete=1};
    retained.sprites.count=2;retained.sprites.draws[0]=(BkEndingUiDraw){.slot=3,.xy={20,20,240,20,240,140,20,140},.uv={0,0,1,1},.alpha=.6f,.rgb=0x80aaff};
    retained.sprites.draws[1]=(BkEndingUiDraw){.slot=4,.xy={240,140,460,140,460,260,240,260},.uv={0,0,1,1},.alpha=1,.rgb=0xffffff};
    CHECK(bk_special_ui_render_begin(s.render,e));CHECK(bk_special_ui_render_prepare(s.render,&retained,width,height,e));
    CHECK(background(r,white,s.color,&vp,e));CHECK(bk_special_ui_render_draw(s.render,e));CHECK(bk_renderer_end(r,e));CHECK(bk_renderer_readback(r,pixels,BYTES,e));
    CHECK(bk_special_ui_render_image(s.render,3,"ge_16.tga",e));CHECK(bk_special_ui_render_image(s.render,4,NULL,e));
    CHECK(!bk_special_ui_render_image(s.render,3,"missing-special-ui-fixture.tga",e));e[0]=0;++rejects;
    CHECK(background(r,white,s.color,&vp,e));CHECK(bk_special_ui_render_draw(s.render,e));CHECK(bk_renderer_end(r,e));CHECK(bk_renderer_readback(r,again,BYTES,e));CHECK(!memcmp(pixels,again,BYTES));++redraws;
    CHECK(bk_special_ui_render_begin(s.render,e));CHECK(!bk_special_ui_render_prepare(s.render,&retained,width,height,e));e[0]=0;++rejects;
    retained.sprites.count=1;retained.sprites.draws[0].xy[0]=NAN;
    CHECK(!bk_special_ui_render_prepare(s.render,&retained,width,height,e));e[0]=0;++rejects;
    retained.sprites.draws[0].xy[0]=20;retained.complete=0;
    CHECK(!bk_special_ui_render_prepare(s.render,&retained,width,height,e));e[0]=0;++rejects;
    for(unsigned a=0;a<6;++a) {
      BkCommonHudFrame frame={.curtain_alpha=a/5.f};
      CHECK(bk_curtain_render_prepare(curtain,&frame,width,height,e));CHECK(background(r,white,s.color,&vp,e));
      CHECK(bk_curtain_render_draw(curtain,e));CHECK(bk_renderer_end(r,e));CHECK(bk_renderer_readback(r,pixels,BYTES,e));
      CHECK(background(r,white,s.color,&vp,e));CHECK(bk_curtain_render_draw_frame(curtain,&frame,width,height,e));CHECK(bk_renderer_end(r,e));
      CHECK(bk_renderer_readback(r,again,BYTES,e));CHECK(!memcmp(pixels,again,BYTES));++curtains;
    }
    bk_special_ui_render_destroy(s.render);s.render=NULL;for(unsigned i=0;i<30;++i)bk_image_free(&s.images[i]);
  }
  if(s.sound_mask!=((1u<<0)|(1u<<2)|(1u<<3)|(1u<<5)|(1u<<7))||!sink.nonzero||s.writes!=6||s.failures!=1||s.schedules!=4) {
    snprintf(e,256,"coverage mask=%x nonzero=%" PRIu64 " writes=%u failures=%u schedules=%u",s.sound_mask,sink.nonzero,s.writes,s.failures,s.schedules);goto done;
  }
  printf("PASS special-ui media frames=%u samples=%u worst=%u redraws=%u images=%u sounds=%u mask=%x photos=%u photo_rgb=%u watermark=%u schedules=%u rejects=%u curtains=%u counters=%u pixels=%016" PRIx64 " files=%016" PRIx64 " PCM=%" PRIu64 " nonzero=%" PRIu64 " hash=%016" PRIx64 "\n",frames,samples,worst,redraws,s.loads,s.plays,s.sound_mask,s.writes,s.pixel_samples,s.watermark_samples,s.schedules,rejects,curtains,counters,pixels_hash,s.file_hash,sink.samples,sink.nonzero,sink.hash);
  rc=0;
done:
  if(rc)fprintf(stderr,"special-ui media FAIL: %s\n",e);
  for(unsigned i=0;i<8;++i)bk_system_audio_destroy(s.sounds[i]);bk_audio_destroy(audio);
  bk_special_ui_render_destroy(s.render);bk_screenshot_destroy(shot);bk_capture_files_destroy(files);
  bk_curtain_render_destroy(curtain);bk_texture_destroy(r,white);bk_renderer_destroy(r);
  for(unsigned i=0;i<30;++i)bk_image_free(&s.images[i]);bk_image_free(&s.watermark);bk_image_free(&curtain_image);bk_blob_free(&raw);
  bk_resources_destroy(store);free(pixels);free(again);return rc;
}
