#include "scene/album_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct Texture {
  struct Texture *next;
  BkTexture *gpu;
  unsigned slot;
  uint32_t generation;
  int live;
} Texture;
struct BkAlbumRender {
  BkRenderer *renderer;
  Texture *textures,*current[BK_ALBUM_SPRITES];
  BkTexture *draw_textures[BK_ALBUM_DRAWS];
  BkGpuMesh *meshes[BK_ALBUM_DRAWS];
  unsigned count,width,height;
  int ready;
};
static int fail(char e[256],const char *why) {snprintf(e,256,"album render: %s",why);return 0;}
void bk_album_render_destroy(BkAlbumRender *s) {
  if(!s)return;
  for(unsigned i=0;i<BK_ALBUM_DRAWS;++i)bk_mesh_destroy(s->renderer,s->meshes[i]);
  while(s->textures) {
    Texture *p=s->textures;s->textures=p->next;
    bk_texture_destroy(s->renderer,p->gpu);free(p);
  }
  free(s);
}
BkAlbumRender *bk_album_render_create(BkRenderer *r,char e[256]) {
  if(!r) {fail(e,"missing renderer");return NULL;}
  BkAlbumRender *s=calloc(1,sizeof(*s));if(!s) {fail(e,"allocation failed");return NULL;}
  s->renderer=r;
  const BkVertex v[4]={{0,0,0,0,0,1,1,1,1},{1,0,0,1,0,1,1,1,1},
                      {1,1,0,1,1,1,1,1,1},{0,1,0,0,1,1,1,1,1}};
  const uint16_t ix[]={0,1,2,3,0,2};
  for(unsigned i=0;i<BK_ALBUM_DRAWS;++i)
    if(!(s->meshes[i]=bk_mesh_create(r,v,4,ix,6,e))) {bk_album_render_destroy(s);return NULL;}
  return s;
}
void bk_album_render_begin(BkAlbumRender *s) {
  if(!s)return;
  s->ready=0;
  Texture **link=&s->textures;
  while(*link) {
    Texture *p=*link;
    if(p->live)link=&p->next;
    else { *link=p->next;bk_texture_destroy(s->renderer,p->gpu);free(p); }
  }
}
int bk_album_render_image(BkAlbumRender *s,unsigned slot,uint32_t generation,
                           const BkImage *image,char e[256]) {
  if(!s || slot>=BK_ALBUM_SPRITES)return fail(e,"invalid image slot");
  Texture *old=s->current[slot];
  if(!image) {
    if(!old || old->generation!=generation)return fail(e,"release generation mismatch");
    old->live=0;s->current[slot]=NULL;return 1;
  }
  Texture *p=calloc(1,sizeof(*p));if(!p)return fail(e,"image owner allocation failed");
  p->gpu=bk_texture_create_sampled(s->renderer,image,BK_WRAP_REPEAT,e);
  if(!p->gpu) {free(p);return 0;}
  p->slot=slot;p->generation=generation;p->live=1;
  if(old)old->live=0;
  p->next=s->textures;s->textures=p;s->current[slot]=p;return 1;
}
int bk_album_render_prepare(BkAlbumRender *s,const BkAlbumFrame *f,unsigned width,
                             unsigned height,char e[256]) {
  if(!s || !f || f->count>BK_ALBUM_DRAWS || !width || !height)return fail(e,"invalid frame");
  s->ready=0;
  for(unsigned i=0;i<f->count;++i) {
    const BkAlbumDraw *d=&f->draws[i];const float *q=d->corners,*uv=d->uv;
    Texture *p=s->textures;
    while(p && (p->slot!=d->slot || p->generation!=d->generation))p=p->next;
    if(!p || !isfinite(d->alpha) || d->alpha<0 || d->alpha>1)return fail(e,"missing draw image/invalid alpha");
    for(unsigned j=0;j<4;++j)if(!isfinite(q[j]) || !isfinite(uv[j]))return fail(e,"invalid geometry");
    if(q[2]<q[0] || q[3]<q[1])return fail(e,"inverted rectangle");
    float alpha=(uint32_t)((double)d->alpha*255)/255.f;
    const BkVertex v[4]={{q[0],q[1],0,uv[0],uv[1],1,1,1,alpha},
      {q[2],q[1],0,uv[2],uv[1],1,1,1,alpha},{q[2],q[3],0,uv[2],uv[3],1,1,1,alpha},
      {q[0],q[3],0,uv[0],uv[3],1,1,1,alpha}};
    if(!bk_mesh_update(s->renderer,s->meshes[i],v,4,e))return 0;
    s->draw_textures[i]=p->gpu;
  }
  s->width=width;s->height=height;s->count=f->count;s->ready=1;return 1;
}
int bk_album_render_draw(BkAlbumRender *s,char e[256]) {
  if(!s || !s->ready)return fail(e,"missing snapshot");
  float matrix[16];memcpy(matrix,bk_identity,sizeof(matrix));
  matrix[0]=2.f/s->width;matrix[5]=2.f/s->height;
  matrix[12]=(float)(1./s->width-1);matrix[13]=(float)(1./s->height-1);
  for(unsigned i=0;i<s->count;++i)
    if(!bk_renderer_draw_mesh(s->renderer,s->draw_textures[i],s->meshes[i],matrix,
                              (BkDrawState){BK_BLEND_ALPHA,0,BK_CULL_NONE},e))return 0;
  return 1;
}
