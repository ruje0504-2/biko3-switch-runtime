#include "scene/album_catalog.h"
#include <stdlib.h>
#include <string.h>
static int valid(const BkImage *p,unsigned w,unsigned h) {
  return p && p->rgba && p->width>=w && p->height>=h &&
      p->width<=16384 && p->height<=16384;
}
static void blit(BkImage *out,const BkImage *src,unsigned sw,unsigned sh,
                  unsigned x0,unsigned y0,unsigned x1,unsigned y1,int key) {
  for(unsigned y=y0;y<y1;++y)for(unsigned x=x0;x<x1;++x) {
    unsigned sx=(x-x0)*sw/(x1-x0),sy=(y-y0)*sh/(y1-y0);
    const uint8_t *p=src->rgba+((size_t)sy*src->width+sx)*4;
    if(key && !(p[0] || p[1] || p[2]))continue;
    uint8_t *d=out->rgba+((size_t)y*out->width+x)*4;
    memcpy(d,p,3);d[3]=255;
  }
}
int bk_album_catalog_build(const BkImage *background,const BkImage *border,
                            const BkImage *empty,const BkImage *const photos[20],
                            BkImage *out,char e[256]) {
  if(!out || out->rgba || out->width || out->height || !photos ||
     !valid(background,1280,960) || !valid(border,160,120) || !valid(empty,160,120)) {
    snprintf(e,256,"album catalog: invalid images/output");return 0;
  }
  for(unsigned i=0;i<20;++i)if(photos[i] && !valid(photos[i],1,1)) {
    snprintf(e,256,"album catalog: invalid photo%u",i);return 0;
  }
  BkImage result={1024,768,malloc(1024*768*4)};
  if(!result.rgba) {snprintf(e,256,"album catalog: allocation failed");return 0;}
  blit(&result,background,1280,960,0,0,1024,768,0);
  for(unsigned i=0;i<20;++i) {
    unsigned x=240+200*(i%5),y=200+160*(i/5);
    unsigned x0=(unsigned)((double)x*.8f),y0=(unsigned)((double)y*.8f);
    unsigned x1=(unsigned)((double)(x+160)*.8f),y1=(unsigned)((double)(y+120)*.8f);
    if(photos[i]) {
      blit(&result,photos[i],photos[i]->width,photos[i]->height,x0,y0,x1,y1,0);
      blit(&result,border,160,120,x0,y0,x1,y1,1);
    } else blit(&result,empty,160,120,x0,y0,x1,y1,0);
  }
  *out=result;return 1;
}
