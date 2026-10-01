/* Render every legend in a synthetic frame. Check the actual pixels outside
 * the left pillarbox, viewport restoration, unchanged redraws and GPU lifetime. */
#include "scene/control_help.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"control help line%d: %s\n",__LINE__,error); goto done; } } while (0)
static int frame(BkRenderer *r, BkTexture *white, BkControlHelp *help, BkControlHelpPage page, char e[256]) {
  BkViewport view;
  if (!bk_camera_fit(&view,1280,720,4,3) || !bk_renderer_begin(r,e) ||
      !bk_renderer_viewport(r,&view,e)) return 0;
  const BkVertex background[6] = {
    {-1,-1,0,0,0,.06f,.09f,.13f,1}, {1,-1,0,1,0,.06f,.09f,.13f,1},
    {1,1,0,1,1,.06f,.09f,.13f,1}, {-1,-1,0,0,0,.06f,.09f,.13f,1},
    {1,1,0,1,1,.06f,.09f,.13f,1}, {-1,1,0,0,1,.06f,.09f,.13f,1}};
  if (!bk_renderer_draw(r,white,background,6,bk_identity,e) ||
      !bk_renderer_viewport(r,NULL,e)) return 0;
  BkRenderStats before=bk_renderer_stats(r);
  if (!bk_control_help_draw(help,page,e)) return 0;
  BkRenderStats after=bk_renderer_stats(r);
  if (after.mesh_updates!=before.mesh_updates || after.uploaded_bytes!=before.uploaded_bytes ||
      after.live_allocations!=before.live_allocations || after.live_bytes!=before.live_bytes ||
      after.draws-before.draws!=(page!=BK_HELP_NONE)) {
    snprintf(e,256,"legend draw uploaded, allocated or submitted more than one draw"); return 0;
  }
  /* Would land in the left bar if the legend failed to restore the viewport. */
  const BkVertex marker[3]={{.88f,-.98f,0,0,0,0,1,0,1}, {.98f,-.98f,0,0,0,0,1,0,1}, {.98f,-.88f,0,0,0,0,1,0,1}};
  return bk_renderer_draw(r,white,marker,3,bk_identity,e) && bk_renderer_end(r,e);
}
int main(int argc,char **argv) {
  if (argc!=2) return 2;
  char error[256]={0}, path[2048]; int result=1;
  BkRenderer *r=NULL; BkControlHelp *help=NULL;
  BkTexture *white=NULL;
  const size_t size=1280*720*4;
  uint8_t *baseline=malloc(size), *pixels=malloc(size), *repeat=malloc(size);
  CHECK(baseline && pixels && repeat);
  CHECK(!mkdir(argv[1],0755) || errno==EEXIST);
  CHECK(r=bk_renderer_create(1280,720,stdout,error));
  uint8_t texel[4]={255,255,255,255};
  CHECK(white=bk_texture_create(r,&(BkImage){1,1,texel},error));
  BkRenderStats empty=bk_renderer_stats(r);
  CHECK(help=bk_control_help_create(r,error));
  CHECK(frame(r,white,help,BK_HELP_NONE,error));
  CHECK(bk_renderer_readback(r,baseline,size,error));
  unsigned checks=0;
  for (unsigned page=1;page<BK_HELP_COUNT;++page) {
    CHECK(frame(r,white,help,(BkControlHelpPage)page,error));
    CHECK(bk_renderer_readback(r,pixels,size,error));
    unsigned changed=0;
    for (unsigned y=0;y<720;++y) {
      CHECK(!memcmp(pixels+(y*1280+160)*4,baseline+(y*1280+160)*4,(1280-160)*4));
      for (unsigned x=0;x<160;++x)
        if (memcmp(pixels+(y*1280+x)*4,baseline+(y*1280+x)*4,4)) {
          CHECK(x>=10 && x<158 && y>=28 && y<710); ++changed;
        }
    }
    CHECK(changed>100);
    CHECK(frame(r,white,help,(BkControlHelpPage)page,error));
    CHECK(bk_renderer_readback(r,repeat,size,error));
    CHECK(!memcmp(pixels,repeat,size));
    CHECK(snprintf(path,sizeof(path),"%s/page-%02u.rgba",argv[1],page)<(int)sizeof(path));
    FILE *out=fopen(path,"wb");CHECK(out);
    size_t wrote=fwrite(pixels,1,size,out); int closed=fclose(out);
    CHECK(wrote==size && !closed);
    ++checks;
  }
  bk_control_help_destroy(help);help=NULL;
  BkRenderStats released=bk_renderer_stats(r);
  CHECK(released.live_allocations==empty.live_allocations && released.live_bytes==empty.live_bytes);
  bk_texture_destroy(r,white);white=NULL;
  bk_renderer_destroy(r);r=NULL;
  /* No black bar: do not allocate legend GPU resources or cover the picture. */
  CHECK(r=bk_renderer_create(960,720,stdout,error));
  empty=bk_renderer_stats(r);
  CHECK(help=bk_control_help_create(r,error));
  CHECK(bk_renderer_begin(r,error) && bk_control_help_draw(help,BK_HELP_ENDING,error) && bk_renderer_end(r,error));
  released=bk_renderer_stats(r);
  CHECK(released.live_allocations==empty.live_allocations && released.live_bytes==empty.live_bytes && released.draws==empty.draws);
  printf("PASS control-help pages%u redraws%u center/right unchanged; no uploads/allocations; viewport restored; 4:3 hidden; GPU baseline\n",checks,checks);
  result=0;
done:
  bk_control_help_destroy(help);bk_texture_destroy(r,white);bk_renderer_destroy(r);
  free(baseline);free(pixels);free(repeat);return result;
}
