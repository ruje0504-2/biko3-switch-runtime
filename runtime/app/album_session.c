#include "app/album_session.h"
#include "scene/album_catalog.h"
#include "scene/album_render.h"
#include "save/capture_file.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkAlbumSession {
  BkAlbumSessionConfig c;
  BkAlbumOps ops;
  BkAlbumRender *render;
  BkPhotoList frozen[5],live;
  BkImage catalogs[25],background,border,empty;
  BkVirtualPointer pointer;
  uint32_t last_ms;
  int stopped;
};
static int fail(char e[256],const char *why) {snprintf(e,256,"album session: %s",why);return 0;}
static int asset(BkAlbumSession *s,const char *pack,const char *name,BkImage *image,char e[256]) {
  BkBlob raw={0};BkResourceResult r=bk_resources_read(s->c.services.resources,pack,name,&raw,e);
  if(r!=BK_RESOURCE_OK) {
    if(r==BK_RESOURCE_MISSING)snprintf(e,256,"album session: missing %s/%s",pack,name);
    return 0;
  }
  int ok=bk_image_decode(raw.data,raw.size,image,e);bk_blob_free(&raw);return ok;
}
/* Native optional photo loads return NULL for absent/unreadable pictures.
 * Keep that behavior distinct from required PP assets and log diagnostics. */
static void photo(BkAlbumSession *s,const BkPhotoList *list,unsigned index,BkImage *image) {
  if(index>=list->count)return;
  char e[256]={0};BkBlob raw={0};
  BkResourceResult r=bk_capture_file_read_photo(s->c.services.capture_files,list->names[index],16*1024*1024,&raw,e);
  if(r==BK_RESOURCE_OK && !bk_image_decode(raw.data,raw.size,image,e))r=BK_RESOURCE_ERROR;
  bk_blob_free(&raw);
  if(r==BK_RESOURCE_ERROR && s->c.services.log)
    fprintf(s->c.services.log,"Album photo skipped: %s: %s\n",list->names[index],e);
}
static int scan(void *p,int32_t counts[5],char e[256]) {
  BkAlbumSession *s=p;
  for(unsigned g=0;g<5;++g) {
    if(!bk_capture_files_list_photos(s->c.services.capture_files,g,g==4?101:100,&s->frozen[g],e))return 0;
    counts[g]=(int32_t)s->frozen[g].count;
  }
  return 1;
}
static int select_group(void *p,unsigned group,int32_t *count,char e[256]) {
  BkAlbumSession *s=p;BkPhotoList next={0};
  if(!bk_capture_files_list_photos(s->c.services.capture_files,group,INT32_MAX,&next,e))return 0;
  bk_photo_list_free(&s->live);s->live=next;*count=(int32_t)next.count;return 1;
}
static int catalog(void *p,unsigned part,unsigned page,char e[256]) {
  BkAlbumSession *s=p;
  if(page>=25 || part!=page%5)return fail(e,"invalid catalog page");
  if(!s->background.rgba && (!asset(s,"bk3_19","al_00.bmp",&s->background,e) ||
      !asset(s,"bk3_19","al_29.bmp",&s->border,e) || !asset(s,"bk3_19","al_28.bmp",&s->empty,e)))return 0;
  BkImage images[20]={0};const BkImage *views[20]={0};
  for(unsigned i=0;i<20;++i) {
    photo(s,&s->frozen[page/5],part*20+i,&images[i]);
    if(images[i].rgba)views[i]=&images[i];
  }
  int ok=bk_album_catalog_build(&s->background,&s->border,&s->empty,views,&s->catalogs[page],e);
  for(unsigned i=0;i<20;++i)bk_image_free(&images[i]);
  return ok;
}
static int image(void *p,unsigned slot,BkAlbumImageKind kind,unsigned index,
                   unsigned group,uint32_t generation,int *present,char e[256]) {
  BkAlbumSession *s=p;*present=0;
  if(kind==BK_ALBUM_RELEASE)return bk_album_render_image(s->render,slot,generation,NULL,e);
  if(kind==BK_ALBUM_CATALOG) {
    if(index>=25 || !s->catalogs[index].rgba)return fail(e,"catalog not generated");
    if(!bk_album_render_image(s->render,slot,generation,&s->catalogs[index],e))return 0;
  } else {
    BkImage decoded={0};
    if(kind==BK_ALBUM_ASSET) {
      const char *name=bk_album_menu_image(slot,index);
      if(!name || !asset(s,"bk3_00",name,&decoded,e))return 0;
    } else {
      if(group>=5)return fail(e,"invalid photo group");
      photo(s,kind==BK_ALBUM_STILL?&s->frozen[group]:&s->live,index,&decoded);
      if(!decoded.rgba)return 1;
    }
    int ok=bk_album_render_image(s->render,slot,generation,&decoded,e);
    bk_image_free(&decoded);if(!ok)return 0;
  }
  *present=1;return 1;
}
static int probe(void *p,unsigned group,unsigned index,int *present,char e[256]) {
  (void)group;(void)e;BkImage im={0};BkAlbumSession *s=p;
  photo(s,&s->live,index,&im);*present=im.rgba!=NULL;bk_image_free(&im);return 1;
}
static int remove_photo(void *p,unsigned group,unsigned index,char e[256]) {
  BkAlbumSession *s=p;
  if(group>=5 || index>=s->frozen[group].count)return fail(e,"invalid deletion selection");
  return bk_capture_file_remove_photo(s->c.services.capture_files,s->frozen[group].names[index],e);
}
static int sound(void *p,unsigned slot,char e[256]) {BkAlbumSession *s=p;return s->c.sound(s->c.context,slot,e);}
static int playing(void *p,unsigned slot,int *active,char e[256]) {BkAlbumSession *s=p;return s->c.playing(s->c.context,slot,active,e);}
BkAlbumSession *bk_album_session_create(const BkAlbumSessionConfig *c,char e[256]) {
  if(!c || !c->menu || !c->services.capture_files || !c->sound || !c->playing || !c->schedule ||
     !isfinite(c->wall_seconds) || c->wall_seconds<0 || c->wall_seconds>1e12) {
    fail(e,"missing process/file/audio services");return NULL;
  }
  BkAlbumSession *s=calloc(1,sizeof(*s));if(!s) {fail(e,"allocation failed");return NULL;}
  s->c=*c;s->ops=(BkAlbumOps){s,scan,catalog,select_group,image,probe,remove_photo,sound,playing};
  memcpy(s->pointer.position,c->pointer,sizeof(c->pointer));
  s->last_ms=(uint32_t)(uint64_t)(c->wall_seconds*1000);
  s->render=bk_album_render_create(c->services.renderer,e);
  if(!s->render || !bk_album_menu_load(c->menu,c->viewport.width,&s->ops,e) ||
     !bk_album_render_prepare(s->render,&(BkAlbumFrame){0},c->viewport.width,c->viewport.height,e)) {
    bk_album_session_destroy(s);return NULL;
  }
  return s;
}
int bk_album_session_stop(BkAlbumSession *s,char e[256]) {
  if(!s)return fail(e,"missing owner");
  if(s->stopped)return 1;
  if(!bk_album_menu_release(s->c.menu,&s->ops,e))return 0;
  s->stopped=1;return 1;
}
void bk_album_session_destroy(BkAlbumSession *s) {
  if(!s)return;
  char e[256];bk_album_session_stop(s,e);
  bk_album_render_destroy(s->render);
  for(unsigned i=0;i<25;++i)bk_image_free(&s->catalogs[i]);
  bk_image_free(&s->background);bk_image_free(&s->border);bk_image_free(&s->empty);
  for(unsigned g=0;g<5;++g)bk_photo_list_free(&s->frozen[g]);
  bk_photo_list_free(&s->live);free(s);
}
static uint8_t key(const BkInput *in,uint32_t mask) {
  return (in->pressed&mask)?3:(in->held&mask)?1:(in->released&mask)?2:0;
}
int bk_album_session_step(BkAlbumSession *s,double seconds,double wall,const BkInput *in,char e[256]) {
  if(!s || s->stopped || !in || !isfinite(seconds) || seconds<=0 || seconds>1 ||
     !isfinite(wall) || wall<0 || wall>1e12)return fail(e,"invalid tick");
  /*51BBC6 checks the exit set by the preceding frame before another step.*/
  if(s->c.menu->exit)return s->c.schedule(s->c.context,1,0,e);
  if(!bk_virtual_pointer_step(&s->pointer,&s->c.viewport,in,seconds,e))return 0;
  uint32_t now=(uint32_t)(uint64_t)(wall*1000),dt=now-s->last_ms;s->last_ms=now;
  BkAlbumInput input={(int32_t)s->pointer.position[0],(int32_t)s->pointer.position[1],dt>100?100:dt,
                       key(in,BK_BUTTON_CONFIRM),key(in,BK_BUTTON_BACK)};
  BkAlbumFrame frame;bk_album_render_begin(s->render);
  return bk_album_menu_step(s->c.menu,&input,&s->ops,&frame,e) &&
      bk_album_render_prepare(s->render,&frame,s->c.viewport.width,s->c.viewport.height,e);
}
int bk_album_session_draw(BkAlbumSession *s,char e[256]) {return s && bk_album_render_draw(s->render,e);}
const BkVirtualPointer *bk_album_session_pointer(const BkAlbumSession *s) {return s?&s->pointer:NULL;}
