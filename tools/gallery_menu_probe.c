/* Actual Japanese gallery textures/PCM and immutable menu snapshots.
 * Unlock bytes here are explicit UI fixtures. This probe does not invent
 * production recordings or stand in for application destination loading. */
#include "scene/gallery_menu_render.h"
#include "scene/system_audio.h"
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 640
#define H 480
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
static int expected(const BkImage im[51], const BkGalleryMenuFrame *f, unsigned x,
                    unsigned y, int out[3]) {
  out[0] = 70;
  out[1] = 110;
  out[2] = 160;
  for (unsigned i = 0; i < f->count; ++i) {
    const BkGalleryMenuDraw *d = &f->draws[i];
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
    double alpha = sample(image, u, v, 3) * (unsigned)(d->alpha * 255.) / 255.;
    for (unsigned c = 0; c < 3; ++c)
      out[c] = (int)lround(sample(image, u, v, c) * 255 * alpha +
                           out[c] * (1 - alpha));
  }
  return 1;
}
typedef struct {
  BkGalleryMenuRender *render;
  BkResourceStore *store;
  BkSystemAudio *sounds[6];
  BkImage images[51], retired[49];
  float point[2];
  unsigned loads, releases, sounds_played;
} Services;
static int image(void *p, unsigned slot, const char *name, char e[256]) {
  Services *s = p;
  if (!bk_gallery_menu_render_image(s->render, slot, name, e)) return 0;
  if (!name) {
    s->retired[slot] = s->images[slot];
    memset(&s->images[slot], 0, sizeof(BkImage));
    ++s->releases;
    return 1;
  }
  BkBlob blob = {0};
  if (bk_resources_read(s->store, "bk3_00", name, &blob, e) != BK_RESOURCE_OK) return 0;
  int ok = bk_image_decode(blob.data, blob.size, &s->images[slot], e);
  bk_blob_free(&blob);
  ++s->loads;
  return ok;
}
static int sound(void *p, unsigned slot, char e[256]) {
  Services *s = p;
  if (slot >= 6 || !s->sounds[slot]) return 0;
  ++s->sounds_played;
  return bk_system_audio_restart(s->sounds[slot], e);
}
static int position(void *p, float out[2], char e[256]) {
  (void)e;
  memcpy(out, ((Services *)p)->point, 8);
  return 1;
}
static void begin(Services *s) {
  bk_gallery_menu_render_begin(s->render);
  for (unsigned i = 0; i < 49; ++i) bk_image_free(&s->retired[i]);
}
static void target(Services *s, const BkGalleryMenu *m, unsigned slot) {
  const float *r = m->sprites[slot].rect;
  s->point[0] = r[0] + r[2] * .5f;
  s->point[1] = r[1] + r[3] * .5f;
}
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  char e[256] = {0}, path[1024];
  int result = 1;
  BkResourceStore *store = bk_resources_create(e);
  BkRenderer *r = NULL;
  BkAudio *audio = NULL;
  BkAudioClip *music = NULL;
  BkTexture *background = NULL;
  uint8_t *pixels = malloc(W*H*4), *redraw = malloc(W*H*4);
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  Services services = {.store = store};
  unsigned frames = 0, samples = 0, worst = 0, redraws = 0, pictures = 0, rejects = 0;
  uint64_t pixel_hash = UINT64_C(14695981039346656037);
  CHECK(store && pixels && redraw);
  const char *packs[] = {"bk3_00", "bk3_02"};
  for (unsigned i=0;i<2;++i) {
    snprintf(path,sizeof(path),"%s/%s.pp",argv[1],packs[i]);
    CHECK(bk_resources_mount(store,packs[i],path,e));
  }
  r=bk_renderer_create(W,H,stderr,e); CHECK(r);
  BkAudioSink output={&sink,48000,240,960,submit,poll};
  audio=bk_audio_create(&output,e); CHECK(audio);
  music=bk_audio_clip_load(store,"bk3_02","bg002.wav",e); CHECK(music);
  for(unsigned i=0;i<=5;++i) {
    services.sounds[i]=bk_system_audio_create_slot(store,audio,48+i,i,-600,e);
    CHECK(services.sounds[i]);
  }
  uint8_t color[]={70,110,160,255}; BkImage solid={1,1,color};
  background=bk_texture_create(r,&solid,e); CHECK(background);
  const BkVertex quad[6]={{-1,-1,0,0,0,1,1,1,1},{1,-1,0,1,0,1,1,1,1},
    {1,1,0,1,1,1,1,1,1},{-1,-1,0,0,0,1,1,1,1},{1,1,0,1,1,1,1,1,1},{-1,1,0,0,1,1,1,1,1}};
  for(unsigned extent=0;extent<2;++extent) {
    unsigned width=extent?503:640,height=extent?377:480;
    BkViewport viewport={(W-width)/2,(H-height)/2,width,height};
    services.render=bk_gallery_menu_render_create(r,store,e); CHECK(services.render);
    for(unsigned i=49;i<51;++i) {
      BkBlob blob={0};
      CHECK(bk_resources_read(store,"bk3_00",i==49?"ma_00.tga":"ma_01.tga",&blob,e)==BK_RESOURCE_OK);
      int ok=bk_image_decode(blob.data,blob.size,&services.images[i],e);bk_blob_free(&blob);CHECK(ok);
    }
    BkGalleryMenu state={0}; uint8_t unlocked[5][8]={{0}};
    memset(&unlocked,1,sizeof(unlocked));
    BkGalleryMenuOps ops={&services,sound,image,position};
    CHECK(bk_gallery_menu_initialize(&state,width,1,0,0,unlocked,-900,&ops,e));
    BkCommonHudState common={.curtain={0,2,0}}; BkMenuCursor cursor={0};
    CHECK(bk_menu_cursor_initialize(&cursor,width,height));
    BkGalleryMenuBindings bindings={&common,&cursor};
    CHECK(bk_audio_play(audio,60,music,1,-900,0,e));
    for(unsigned group=0;group<5;++group) {
      target(&services,&state,2+group*2);
      memcpy(state.pointer,services.point,8);
      begin(&services);
      BkGalleryMenuInput in={1,.5f,width/1280.f,height}; BkGalleryMenuFrame frame;
      CHECK(bk_gallery_menu_step(&state,&bindings,&in,&ops,&frame,e));
      CHECK(state.group==(int)group);
      for(unsigned picture=0;picture<5;++picture) {
        for(unsigned tick=0;tick<18;++tick) {
          begin(&services);
          if(tick==0)target(&services,&state,41+picture);
          in.buttons=tick==1?BK_PAUSE_CONFIRM:tick==8?BK_GALLERY_BACK:0;
          sink.consumed=sink.submitted;CHECK(bk_audio_poll(audio,e));
          CHECK(bk_gallery_menu_step(&state,&bindings,&in,&ops,&frame,e));
          CHECK(frame.action==99);
          CHECK(bk_audio_fill(audio,e));
          CHECK(bk_gallery_menu_render_prepare(services.render,&frame,width,height,e));
          CHECK(bk_renderer_begin(r,e)); CHECK(bk_renderer_viewport(r,NULL,e));
          CHECK(bk_renderer_draw(r,background,quad,6,bk_identity,e));
          CHECK(bk_renderer_viewport(r,&viewport,e));
          CHECK(bk_gallery_menu_render_draw(services.render,e)); CHECK(bk_renderer_end(r,e));
          CHECK(bk_renderer_readback(r,pixels,W*H*4,e));
          BkImage expected_images[51]; memcpy(expected_images,services.images,sizeof(expected_images));
          for(unsigned i=0;i<49;++i)if(!expected_images[i].rgba)expected_images[i]=services.retired[i];
          for(unsigned y=1;y<height;y+=17)for(unsigned x=1;x<width;x+=17) {
            int rgb[3]; if(!expected(expected_images,&frame,x,y,rgb))continue;
            for(unsigned c=0;c<3;++c) {
              unsigned pixel=pixels[((size_t)(y+viewport.y)*W+x+viewport.x)*4+c];
              unsigned delta=(unsigned)abs((int)pixel-rgb[c]);if(delta>worst)worst=delta;
              if(delta>1) {snprintf(e,256,"pixel e%u g%u pic%u t%u xy%u,%u c%u got%u want%d",extent,group,picture,tick,x,y,c,pixel,rgb[c]);goto done;}
              ++samples;pixel_hash^=pixel;pixel_hash*=UINT64_C(1099511628211);
            }
          }
          if(tick==6 || tick==8) {
            BkGalleryMenu before=state;BkCommonHudState cb=common;BkMenuCursor cur=cursor;
            CHECK(bk_renderer_begin(r,e)); CHECK(bk_renderer_viewport(r,NULL,e));
            CHECK(bk_renderer_draw(r,background,quad,6,bk_identity,e));
            CHECK(bk_renderer_viewport(r,&viewport,e));CHECK(bk_gallery_menu_render_draw(services.render,e));
            CHECK(bk_renderer_end(r,e));CHECK(bk_renderer_readback(r,redraw,W*H*4,e));
            CHECK(!memcmp(pixels,redraw,W*H*4) && !memcmp(&state,&before,sizeof(state)) &&
              !memcmp(&common,&cb,sizeof(common)) && !memcmp(&cursor,&cur,sizeof(cursor)));
            ++redraws;
            if(tick==8)CHECK(!(state.loaded&(UINT64_C(1)<<48)) && services.retired[48].rgba);
          }
          ++frames;
        }
        CHECK(state.image_view==0 && !(state.loaded&(UINT64_C(1)<<48)));++pictures;
      }
    }
    begin(&services);
    CHECK(!bk_gallery_menu_render_image(services.render,48,"missing-gallery-fixture.bmp",e));
    CHECK(strstr(e,"missing-gallery-fixture.bmp"));e[0]=0;++rejects;
    bk_gallery_menu_render_destroy(services.render);services.render=NULL;
    for(unsigned i=0;i<51;++i)bk_image_free(&services.images[i]);
  }
  CHECK(sink.nonzero && pictures==50 && redraws==100);
  printf("gallery-menu GPU PASS frames=%u pictures=%u samples=%u worst=%u redraws=%u loads=%u releases=%u sounds=%u rejects=%u pixels=%016" PRIx64 " PCM=%" PRIu64 " nonzero=%" PRIu64 " hash=%016" PRIx64 "\n",frames,pictures,samples,worst,redraws,services.loads,services.releases,services.sounds_played,rejects,pixel_hash,sink.samples,sink.nonzero,sink.hash);
  result=0;
done:
  if(result)fprintf(stderr,"gallery-menu GPU FAIL: %s\n",e);
  for(unsigned i=0;i<51;++i)bk_image_free(&services.images[i]);
  for(unsigned i=0;i<49;++i)bk_image_free(&services.retired[i]);
  bk_gallery_menu_render_destroy(services.render);
  bk_texture_destroy(r,background);
  for(unsigned i=0;i<6;++i)bk_system_audio_destroy(services.sounds[i]);
  bk_audio_clip_release(music);bk_audio_destroy(audio);bk_renderer_destroy(r);bk_resources_destroy(store);
  free(pixels);free(redraw);return result;
}
