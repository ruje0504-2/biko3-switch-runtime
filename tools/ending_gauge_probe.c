/* Real gauge textures; explicit progress/drift fixtures, no story injection. */
#include "scene/ending_ui_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 1280
#define H 960
#define BYTES ((size_t)W*H*4)
#define CHECK(x) do { if (!(x)) { if (!*e) snprintf(e,256,"line%d: %s",__LINE__,#x); goto done; } } while (0)
static int draw(BkRenderer *r, BkEndingUiRender *ui, const BkEndingUiFrame *frame,
                 BkViewport viewport, uint8_t *pixels, char e[256]) {
  return bk_ending_ui_render_prepare(ui,frame,viewport.width,viewport.height,e) &&
         bk_renderer_begin(r,e) && bk_renderer_viewport(r,&viewport,e) &&
         bk_ending_ui_render_draw(ui,e) && bk_renderer_end(r,e) &&
         bk_renderer_readback(r,pixels,BYTES,e);
}
static int save(const char *prefix,const char *suffix,const uint8_t *pixels) {
  char path[1024];snprintf(path,sizeof(path),"%s-%s.rgba",prefix,suffix);
  FILE *f=fopen(path,"wb");if(!f)return 0;
  int ok=fwrite(pixels,1,BYTES,f)==BYTES;return !fclose(f) && ok;
}
int main(int argc,char **argv) {
  if(argc!=3)return 2;
  int result=1;char e[256]={0},path[1024];
  BkResourceStore *store=NULL;BkRenderer *r=NULL;BkEndingUiRender *render=NULL;
  uint8_t *old=malloc(BYTES),*fixed=malloc(BYTES),*reference=malloc(BYTES),*border=malloc(BYTES);
  unsigned cases=0,old_bad=0,old_outside=0,redraws=0;
  CHECK(old && fixed && reference && border);
  store=bk_resources_create(e);CHECK(store);
  snprintf(path,sizeof(path),"%s/bk3_00.pp",argv[1]);CHECK(bk_resources_mount(store,"bk3_00",path,e));
  r=bk_renderer_create(W,H,stderr,e);CHECK(r);
  BkRenderStats baseline=bk_renderer_stats(r);
  render=bk_ending_ui_render_create(r,store,e);CHECK(render);
  const float levels[]={-.25f,0,.125f,.5f,.875f,1,1.25f};
  for(unsigned width=640;width<=1280;width+=320) {
    float scale=width/1280.f;
    BkViewport viewport={(W-width)/2,0,width,width*3/4};
    BkEndingUi ui={0};uint8_t flags[6];float gauge;
    CHECK(bk_ending_ui_initialize(&ui,width,flags,&gauge,e));
    for(unsigned level=0;level<sizeof(levels)/sizeof(*levels);++level)
      for(int drift=-80;drift<=80;drift+=80) {
        float value=levels[level];BkEndingUiFrame frame={.count=2};
        unsigned slot=value>.99f?11:10;
        ui.sprites[slot].transform.fade=(BkFadeSprite){1,2,3};
        ui.sprites[9].transform.fade=(BkFadeSprite){1,2,3};
        ui.sprites[9].transform.scale[1]=value;
        ui.sprites[9].rect[1]=(251-200*value+drift)*scale;
        CHECK(bk_ending_ui_sprite_step(&ui,slot,0,&frame.draws[0],e));
        CHECK(bk_ending_ui_sprite_step(&ui,9,0,&frame.draws[1],e));
        BkEndingUi before=ui;
        BkEndingUiFrame only_border=frame;only_border.count=1;
        CHECK(draw(r,render,&only_border,viewport,border,e));
        CHECK(draw(r,render,&frame,viewport,old,e));
        BkEndingUiFrame wanted=frame;
        /* Independent authored 35x200 interior, fixed bottom251. */
        float amount=value<0?0:value>1?1:value;
        float top=(251-200*amount)*scale,bottom=251*scale;
        wanted.draws[1].xy[1]=wanted.draws[1].xy[3]=top;
        wanted.draws[1].xy[5]=wanted.draws[1].xy[7]=bottom;
        CHECK(bk_ending_ui_fit_gauge(&frame,width,e));
        CHECK(!memcmp(&ui,&before,sizeof(ui)));
        CHECK(!memcmp(&frame.draws[0],&wanted.draws[0],sizeof(frame.draws[0])));
        CHECK(draw(r,render,&wanted,viewport,reference,e));
        CHECK(draw(r,render,&frame,viewport,fixed,e));
        CHECK(!memcmp(fixed,reference,BYTES));
        if(memcmp(old,fixed,BYTES)) ++old_bad;
        unsigned before_outside=0,after_outside=0;
        for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x) {
          size_t at=((size_t)y*W+x)*4;
          if(x+1<viewport.x+26*scale || x>viewport.x+61*scale+1 ||
             y+1<51*scale || y>251*scale+1) {
            before_outside+=memcmp(old+at,border+at,3)!=0;
            after_outside+=memcmp(fixed+at,border+at,3)!=0;
          }
        }
        old_outside+=before_outside;CHECK(!after_outside);
        if(width==960 && value==.5f && drift==80) {
          CHECK(save(argv[2],"before",old) && save(argv[2],"after",fixed));
          BkRenderStats once=bk_renderer_stats(r);
          CHECK(bk_renderer_begin(r,e) && bk_renderer_viewport(r,&viewport,e) &&
                bk_ending_ui_render_draw(render,e) && bk_renderer_end(r,e) &&
                bk_renderer_readback(r,reference,BYTES,e));
          BkRenderStats twice=bk_renderer_stats(r);
          CHECK(!memcmp(reference,fixed,BYTES) && once.mesh_updates==twice.mesh_updates &&
                once.live_allocations==twice.live_allocations);++redraws;
        }
        ++cases;
      }
  }
  CHECK(old_bad && old_outside);
  bk_ending_ui_render_destroy(render);render=NULL;
  BkRenderStats end=bk_renderer_stats(r);
  CHECK(end.live_allocations==baseline.live_allocations && end.live_bytes==baseline.live_bytes);
  printf("PASS gauge cases%u old_wrong_frames%u old_outside_pixels%u new_outside0 reference_pixel_error0 redraws%u GPU_baseline\n",cases,old_bad,old_outside,redraws);
  result=0;
done:
  if(result)fprintf(stderr,"FAILED: %s\n",e);
  bk_ending_ui_render_destroy(render);bk_renderer_destroy(r);bk_resources_destroy(store);
  free(old);free(fixed);free(reference);free(border);return result;
}
