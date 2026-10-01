/* Actual Japanese volume-page textures, test buffers and frozen redraws.
 * Uses real input controls; file persistence/application flow30 remain separate. */
#include "scene/volume_menu_render.h"
#include "scene/volume_menu_audio.h"
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 960
#define H 720
#define CHECK(x) do { if (!(x)) { if (!*e) snprintf(e,256,"line%d: %s",__LINE__,#x); goto done; } } while (0)
typedef struct {
  uint64_t submitted, consumed, samples, nonzero, hash;
} Sink;
static int submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  Sink *s = p;
  for (size_t i = 0; i < frames * 2; ++i) {
    ++s->samples;
    s->nonzero += pcm[i] != 0;
    uint16_t value = (uint16_t)pcm[i];
    for (unsigned b = 0; b < 2; ++b) {
      s->hash ^= (value >> (b * 8)) & 255;
      s->hash *= UINT64_C(1099511628211);
    }
  }
  s->submitted += frames;
  return 1;
}
static int poll(void *p, uint64_t *consumed, char e[256]) {
  (void)e;
  *consumed = ((Sink *)p)->consumed;
  return 1;
}
static double texel(const BkImage *im, int x, int y, unsigned c) {
  int w = (int)im->width, h = (int)im->height;
  x = (x % w + w) % w;
  y = (y % h + h) % h;
  return im->rgba[((size_t)y * w + x) * 4 + c] / 255.;
}
static double sample(const BkImage *im, double x, double y, unsigned c) {
  int l = (int)floor(x), t = (int)floor(y);
  double fx = x - l, fy = y - t;
  return (texel(im, l, t, c) * (1 - fx) + texel(im, l + 1, t, c) * fx) *
             (1 - fy) +
         (texel(im, l, t + 1, c) * (1 - fx) + texel(im, l + 1, t + 1, c) * fx) *
             fy;
}
static int expected(const BkImage im[23], const BkVolumeMenuFrame *f, unsigned x,
                    unsigned y, int out[3]) {
  out[0] = 70;
  out[1] = 110;
  out[2] = 160;
  for (unsigned i = 0; i < f->count; ++i) {
    const BkVolumeMenuDraw *d = &f->draws[i];
    double l = d->corners[0], t = d->corners[1], width = d->corners[2] - l,
           height = d->corners[3] - t;
    /* Raster edge inclusivity is not the sampler/blend oracle's subject. */
    if (fabs(x - l) < .03 || fabs(y - t) < .03 || fabs(x - l - width) < .03 ||
        fabs(y - t - height) < .03)
      return 0;
    if (x < l || x >= l + width || y < t || y >= t + height)
      continue;
    const BkImage *image = &im[d->slot];
    double u = (x - l) / width * image->width - .5,
           v = (y - t) / height * image->height - .5;
    double alpha = sample(image, u, v, 3);
    for (unsigned c = 0; c < 3; ++c)
      out[c] = (int)lround(sample(image, u, v, c) * 255 * alpha +
                           out[c] * (1 - alpha));
  }
  return 1;
}
typedef struct {
  BkVolumeMenuAudio *media;
  BkAudio *audio;
  int32_t point[2];
  unsigned random,plays,actions[6];
} Services;
static int play(void *p,unsigned slot,int32_t volume,char e[256]) {
  Services *s=p;
  if (!bk_volume_menu_audio_play(s->media,slot,volume,e)) return 0;
  static const unsigned slots[]={0,3,6,9,10,11};
  for(unsigned i=0;i<6;++i) if(slot==slots[i]) {
    int32_t actual,pan;
    if(!bk_audio_get_gain(s->audio,i,&actual,&pan) || actual!=volume || pan) return 0;
  }
  ++s->plays;return 1;
}
static int stop(void *p,unsigned slot,char e[256]) {
  return bk_volume_menu_audio_stop(((Services *)p)->media,slot,e);
}
static int gain(void *p,unsigned slot,int32_t volume,char e[256]) {
  return bk_volume_menu_audio_gain(((Services *)p)->media,slot,volume,e);
}
static int playing(void *p,unsigned slot,int *active,char e[256]) {
  return bk_volume_menu_audio_playing(((Services *)p)->media,slot,active,e);
}
static int random_value(void *p,uint32_t *value,char e[256]) {
  (void)e;*value=((Services *)p)->random++;return 1;
}
static int warp(void *p,int32_t x,int32_t y,char e[256]) {
  (void)e;Services *s=p;s->point[0]=x;s->point[1]=y;return 1;
}
static void click(const BkVolumeMenu *s,unsigned row,BkVolumeMenuInput *in) {
  in->x=(int)(s->rect[10+row*2][0]+3);
  in->y=(int)(s->rect[10+row*2][1]+3);in->mouse=3;
}
int main(int argc,char **argv) {
  if(argc!=2) return 2;
  char e[256]={0},path[1024];int result=1;
  BkResourceStore *store=bk_resources_create(e),*empty=NULL;
  BkRenderer *r=NULL;BkAudio *audio=NULL;BkVolumeMenuRender *render=NULL;
  BkImage images[23]={{0}};BkTexture *background=NULL;
  uint8_t *pixels=malloc(W*H*4),*redraw=malloc(W*H*4);
  Services services={0};Sink sink={.hash=UINT64_C(14695981039346656037)};
  unsigned frames=0,redraws=0,samples=0,worst=0,rejects=0;uint64_t pixel_hash=UINT64_C(14695981039346656037);
  CHECK(store && pixels && redraw);
  const char *packs[]={"bk3_00","bk3_02"};
  for(unsigned i=0;i<2;++i) {
    snprintf(path,sizeof(path),"%s/%s.pp",argv[1],packs[i]);CHECK(bk_resources_mount(store,packs[i],path,e));
  }
  for(unsigned i=0;i<23;++i) {
    BkBlob blob={0};CHECK(bk_resources_read(store,"bk3_00",bk_volume_menu_image(i),&blob,e)==BK_RESOURCE_OK);
    int ok=bk_image_decode(blob.data,blob.size,&images[i],e);bk_blob_free(&blob);CHECK(ok);
  }
  r=bk_renderer_create(W,H,stderr,e);CHECK(r);
  BkAudioSink output={&sink,48000,240,960,submit,poll};audio=bk_audio_create(&output,e);CHECK(audio);
  services.audio=audio;
  uint8_t color[]={70,110,160,255};BkImage solid={1,1,color};background=bk_texture_create(r,&solid,e);CHECK(background);
  const BkVertex quad[6]={{-1,-1,0,0,0,1,1,1,1},{1,-1,0,1,0,1,1,1,1},
    {1,1,0,1,1,1,1,1,1},{-1,-1,0,0,0,1,1,1,1},{1,1,0,1,1,1,1,1,1},{-1,1,0,0,1,1,1,1,1}};
  uint64_t baseline=bk_renderer_stats(r).live_allocations;
  empty=bk_resources_create(e);CHECK(empty);
  CHECK(!bk_volume_menu_render_create(r,empty,e));e[0]=0;++rejects;
  CHECK(!bk_volume_menu_audio_create(empty,audio,0,e));e[0]=0;++rejects;
  CHECK(bk_renderer_stats(r).live_allocations==baseline);
  for(unsigned extent=0;extent<2;++extent) {
    unsigned width=extent?503:960,height=width*3/4;
    BkViewport viewport={(W-width)/2,(H-height)/2,width,height};
    render=bk_volume_menu_render_create(r,store,e);CHECK(render);
    services.media=bk_volume_menu_audio_create(store,audio,0,e);CHECK(services.media);
    BkVolumeMenu state={0};const int32_t volumes[]={-1500,-2500,-1999};
    CHECK(bk_volume_menu_initialize(&state,width,volumes,e));
    BkVolumeMenuOps ops={&services,play,stop,gain,playing,random_value,warp};
    for(unsigned tick=0;tick<300;++tick) {
      BkVolumeMenuInput in={.x=services.point[0],.y=services.point[1]};
      if(tick==0 || tick==60 || tick==120 || tick==150) click(&state,tick==0?0:tick==60?1:2,&in);
      if(tick>=20 && tick<50) {
        in.x=(int)(state.minimum+(state.maximum-state.minimum)*(tick-20)/29);
        in.y=(int)state.rect[1][1];in.mouse=tick==20?3:1;
      }
      if(tick>=80 && tick<110) {
        in.x=(int)state.slider[1];in.y=(int)state.rect[2][1];
        in.left=1; /* keyboard adjustment and warp */
      }
      if(tick==180) click(&state,3,&in);
      if(tick==220) click(&state,4,&in);
      if(tick==240) {in.up=3;in.mouse=0;}
      if(tick>=250 && tick<280) in.right=1;
      sink.consumed=sink.submitted;CHECK(bk_audio_poll(audio,e));
      BkVolumeMenuFrame frame;CHECK(bk_volume_menu_prepare(&state,&frame,e));
      int action;CHECK(bk_volume_menu_step(&state,&in,&ops,&action,e));
      if(action>=0) ++services.actions[action];
      if(tick==180) CHECK(state.values[0]==-1500 && state.values[1]==-2500 && state.values[2]==-2000);
      if(tick==1) {
        CHECK(!bk_volume_menu_audio_create(store,audio,0,e));e[0]=0;++rejects;
        int active;CHECK(bk_volume_menu_audio_playing(services.media,0,&active,e) && active);
      }
      CHECK(bk_audio_fill(audio,e));
      CHECK(bk_volume_menu_render_prepare(render,&frame,width,height,e));
      CHECK(bk_renderer_begin(r,e));CHECK(bk_renderer_viewport(r,NULL,e));
      CHECK(bk_renderer_draw(r,background,quad,6,bk_identity,e));CHECK(bk_renderer_viewport(r,&viewport,e));
      CHECK(bk_volume_menu_render_draw(render,e));CHECK(bk_renderer_end(r,e));CHECK(bk_renderer_readback(r,pixels,W*H*4,e));
      for(unsigned y=1;y<height;y+=17) for(unsigned x=1;x<width;x+=17) {
        int rgb[3];if(!expected(images,&frame,x,y,rgb))continue;
        for(unsigned c=0;c<3;++c) {
          unsigned pixel=pixels[((size_t)(y+viewport.y)*W+x+viewport.x)*4+c];
          unsigned delta=(unsigned)abs((int)pixel-rgb[c]);if(delta>worst)worst=delta;
          if(delta>1) {snprintf(e,256,"pixel e%u tick%u xy%u,%u c%u got%u want%d",extent,tick,x,y,c,pixel,rgb[c]);goto done;}
          ++samples;pixel_hash^=pixel;pixel_hash*=UINT64_C(1099511628211);
        }
      }
      if(tick%30==0 || action==4) {
        BkVolumeMenu before=state;uint64_t pcm=sink.hash;
        CHECK(bk_renderer_begin(r,e));CHECK(bk_renderer_viewport(r,NULL,e));
        CHECK(bk_renderer_draw(r,background,quad,6,bk_identity,e));CHECK(bk_renderer_viewport(r,&viewport,e));
        CHECK(bk_volume_menu_render_draw(render,e));CHECK(bk_renderer_end(r,e));CHECK(bk_renderer_readback(r,redraw,W*H*4,e));
        CHECK(!memcmp(pixels,redraw,W*H*4) && !memcmp(&before,&state,sizeof(state)) && pcm==sink.hash);++redraws;
      }
      ++frames;
    }
    bk_volume_menu_audio_destroy(services.media);services.media=NULL;
    bk_volume_menu_render_destroy(render);render=NULL;
    CHECK(bk_renderer_stats(r).live_allocations==baseline);
    for(unsigned i=0;i<6;++i) {int32_t volume,pan;CHECK(!bk_audio_get_gain(audio,i,&volume,&pan));}
  }
  CHECK(sink.nonzero && services.actions[0] && services.actions[1] && services.actions[2] && services.actions[3] && services.actions[4]);
  printf("volume-menu GPU PASS frames=%u samples=%u worst=%u redraws=%u plays=%u actions=%u,%u,%u,%u,%u,%u rejects=%u pixels=%016" PRIx64 " PCM=%" PRIu64 " nonzero=%" PRIu64 " hash=%016" PRIx64 "\n",frames,samples,worst,redraws,services.plays,services.actions[0],services.actions[1],services.actions[2],services.actions[3],services.actions[4],services.actions[5],rejects,pixel_hash,sink.samples,sink.nonzero,sink.hash);
  result=0;
done:
  if(result) fprintf(stderr,"volume-menu GPU FAIL: %s\n",e);
  bk_volume_menu_audio_destroy(services.media);bk_volume_menu_render_destroy(render);
  bk_audio_destroy(audio);bk_texture_destroy(r,background);bk_renderer_destroy(r);
  for(unsigned i=0;i<23;++i) bk_image_free(&images[i]);
  bk_resources_destroy(empty);bk_resources_destroy(store);free(pixels);free(redraw);return result;
}
