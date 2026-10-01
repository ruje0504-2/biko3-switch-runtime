#include "scene/album_menu.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct { const char *name; int rect[4]; } Image;
static const Image images[BK_ALBUM_SPRITES] = {
  [1]={"al_30.tga",{0,0,160,120}},
  [2]={"al_26.tga",{976,880,256,64}},
  [3]={"al_27.tga",{976,880,256,64}},
  [4]={"al_21.tga",{808,64,48,72}},
  [5]={"al_22.tga",{808,64,48,72}},
  [6]={"al_24.tga",{576,64,48,72}},
  [7]={"al_25.tga",{576,64,48,72}},
  [8]={"ma_00.tga",{0,0,96,96}},
  [10]={"al_15.tga",{640,72,48,64}},
  [11]={"al_16.tga",{640,72,48,64}},
  [12]={"al_17.tga",{640,72,48,64}},
  [13]={"al_18.tga",{640,72,48,64}},
  [14]={"al_19.tga",{640,72,48,64}},
  [15]={"ma_01.tga",{0,0,1280,960}},
  [18]={"al_13.tga",{1024,88,208,54}},
  [19]={"al_14.tga",{1024,88,208,54}},
  [20]={"al_11.tga",{1024,26,208,54}},
  [21]={"al_12.tga",{1024,26,208,54}},
  [22]={"al_28.bmp",{0,0,160,120}},
  [23]={"ma_03.bmp",{0,0,1280,960}},
  [24]={"ma_10.tga",{32,864,712,48}},
  [25]={"ma_11.tga",{32,864,712,48}},
  [28]={"al_31.tga",{32,336,1224,200}},
  [30]={"al_01.tga",{20,134,144,144}},
  [31]={"al_02.tga",{20,134,144,144}},
  [32]={"al_03.tga",{20,284,144,144}},
  [33]={"al_04.tga",{20,284,144,144}},
  [34]={"al_05.tga",{20,434,144,144}},
  [35]={"al_06.tga",{20,434,144,144}},
  [36]={"al_07.tga",{20,584,144,144}},
  [37]={"al_08.tga",{20,584,144,144}},
  [38]={"al_09.tga",{20,734,144,144}},
  [39]={"al_10.tga",{20,734,144,144}},
};
static int fail(char e[256],const char *why) {snprintf(e,256,"album menu: %s",why);return 0;}
static int services(const BkAlbumOps *o) {
  return o && o->scan && o->catalog && o->select && o->image && o->probe &&
      o->remove && o->sound && o->playing;
}
void bk_album_menu_initialize(BkAlbumMenu *s) {
  memset(s,0,sizeof(*s));s->selected[0]=1;s->curtain=s->spare_alpha=1;s->spare_duration=1000;
}
const char *bk_album_menu_image(unsigned slot,unsigned variant) {
  return slot==8 && variant==1 ? "ma_06.tga" : slot<BK_ALBUM_SPRITES ? images[slot].name : NULL;
}
static int release_image(BkAlbumMenu *s,const BkAlbumOps *o,unsigned slot,char e[256]) {
  if(!(s->loaded & (UINT64_C(1)<<slot)))return 1;
  int present=0;
  if(!o->image(o->context,slot,BK_ALBUM_RELEASE,0,s->group,s->image_generation[slot],&present,e))return 0;
  s->loaded&=~(UINT64_C(1)<<slot);return 1;
}
static int image(BkAlbumMenu *s,const BkAlbumOps *o,unsigned slot,
                 BkAlbumImageKind kind,unsigned index,char e[256]) {
  int present=0;
  uint32_t generation=s->image_generation[slot]+1;
  if(!o->image(o->context,slot,kind,index,s->group,generation,&present,e))return 0;
  if(!present) {
    s->loaded&=~(UINT64_C(1)<<slot);
    return kind==BK_ALBUM_STILL || fail(e,"required image is missing");
  }
  s->image_generation[slot]=generation;
  BkAlbumSprite *p=&s->sprites[slot];*p=(BkAlbumSprite){.uv={0,0,1,1},.scale={1,1},.alpha=1};
  static const int full_rect[4]={0,0,1280,960};
  const int *q=kind==BK_ALBUM_ASSET?images[slot].rect:full_rect;
  for(unsigned i=0;i<4;++i)p->rect[i]=(float)(int32_t)((double)q[i]*s->scale);
  s->loaded|=UINT64_C(1)<<slot;return 1;
}
int bk_album_menu_load(BkAlbumMenu *s,unsigned width,const BkAlbumOps *o,char e[256]) {
  if(!s || s->loaded || !services(o) || width<4 || width>16384)return fail(e,"invalid entry/owner");
  static const uint8_t order[]={8,1,2,3,4,5,6,7,18,19,20,21,22,10,11,12,13,14,15,23,24,25,28,30,31,32,33,34,35,36,37,38,39};
  s->scale=(float)((double)width/1280);s->state=1;
  for(unsigned i=0;i<sizeof(order);++i)
    if(!image(s,o,order[i],BK_ALBUM_ASSET,0,e))return 0;
  static const uint8_t hidden[]={8,6,1,19,21,22,33,35,37,39};
  for(unsigned i=0;i<sizeof(hidden);++i)s->sprites[hidden[i]].hidden=1;
  s->exit=0;return 1;
}
static void position(BkAlbumMenu *s,unsigned slot,float x,float y) {
  s->sprites[slot].rect[0]=x;s->sprites[slot].rect[1]=y;
}
static void draw(BkAlbumMenu *s,BkAlbumFrame *f,unsigned slot) {
  if(!(s->loaded & (UINT64_C(1)<<slot)) || s->sprites[slot].hidden)return;
  BkAlbumSprite *p=&s->sprites[slot];BkAlbumDraw *d=&f->draws[f->count++];
  d->slot=slot;d->generation=s->image_generation[slot];d->alpha=p->alpha;memcpy(d->uv,p->uv,sizeof(d->uv));
  d->corners[0]=p->rect[0];d->corners[1]=p->rect[1];
  /*43ED45 stores the scaled extent as float before adding the position.*/
  float width=p->rect[2]*p->scale[0],height=p->rect[3]*p->scale[1];
  d->corners[2]=p->rect[0]+width;
  d->corners[3]=p->rect[1]+height;
}
static int hit(const float q[4],const BkAlbumInput *in) {
  float right=(float)((double)q[0]+q[2]),bottom=(float)((double)q[1]+q[3]);
  return q[0]<=in->x && right>in->x && q[1]<=in->y && bottom>in->y;
}
static int item_hover(BkAlbumMenu *s,const BkAlbumInput *in,const BkAlbumOps *o,char e[256]) {
  s->hover=-1;s->sprites[1].hidden=1;
  for(unsigned row=0;row<4;++row)for(unsigned col=0;col<5;++col) {
    unsigned local=row*5+col,index=(unsigned)(s->page%5)*20+local;
    float x=(float)((240.+200*col)*s->scale),y=(float)((200.+160*row)*s->scale);
    if(hit((float[]){x,y,(float)(160.*s->scale),(float)(120.*s->scale)},in)) {
      if(index<(unsigned)s->count) {
        s->hover=(int32_t)index;position(s,1,x,y);
        if(!s->deleted[s->group][index]) {
          s->sprites[1].hidden=0;
          if(!s->item_hover[local]) {
            if(!o->sound(o->context,3,e))return 0;
            s->item_hover[local]=1;
          }
        }
      }
    } else s->item_hover[local]=0;
  }
  return 1;
}
static int tabs(BkAlbumMenu *s,const BkAlbumInput *in,const BkAlbumOps *o,char e[256]) {
  for(unsigned g=0;g<5;++g) {
    unsigned slot=31+g*2;
    if(hit(s->sprites[slot].rect,in)) {
      if(!s->tab_hover[g]) {if(!o->sound(o->context,4,e))return 0;s->tab_hover[g]=1;}
      s->sprites[slot].hidden=0;
      if(in->confirm==3) {
        if(!s->selected[g]) {
          memset(s->selected,0,sizeof(s->selected));s->selected[g]=1;
          if(!o->sound(o->context,0,e))return 0;
          /*47204f copies the original frozen group, then rescans live files.*/
          s->count=s->counts[g];
          if(!release_image(s,o,BK_ALBUM_PAGE,e))return 0;
          s->page=(int32_t)g*5;s->group=(int32_t)g;
          if(!o->select(o->context,g,&s->live_count,e) ||
             !image(s,o,BK_ALBUM_PAGE,BK_ALBUM_CATALOG,s->page,e))return 0;
          s->sprites[6].hidden=1;s->sprites[4].hidden=0;
        } else if(!o->sound(o->context,2,e))return 0;
      }
    } else {
      if(!s->selected[g])s->sprites[slot].hidden=1;
      s->tab_hover[g]=0;
    }
  }
  return 1;
}
static int page_button(BkAlbumMenu *s,const BkAlbumInput *in,const BkAlbumOps *o,
                        int forward,char e[256]) {
  unsigned slot=forward?5:7,hover=forward?0:1;int32_t edge=s->group*5+(forward?4:0);
  if(hit(s->sprites[slot].rect,in)) {
    if(in->confirm==3) {
      s->page+=forward?1:-1;
      if(forward?s->page>edge:s->page<edge)s->page=edge;
      else {
        if(s->page==edge)s->sprites[forward?4:6].hidden=1;
        if(s->page==s->group*5+(forward?1:3))s->sprites[forward?6:4].hidden=0;
        if(!o->sound(o->context,0,e) || !release_image(s,o,BK_ALBUM_PAGE,e) ||
           !image(s,o,BK_ALBUM_PAGE,BK_ALBUM_CATALOG,s->page,e))return 0;
      }
    }
    if(!forward)s->hover=-2;
    if(forward?s->page<edge:s->page>edge) {
      if(!s->button_hover[hover]) {if(!o->sound(o->context,3,e))return 0;s->button_hover[hover]=1;}
      s->sprites[slot].hidden=0;
    } else s->sprites[slot].hidden=1;
    if(forward)s->hover=-2;
  } else {s->sprites[slot].hidden=1;s->button_hover[hover]=0;}
  return 1;
}
static int grid(BkAlbumMenu *s,const BkAlbumInput *in,const BkAlbumOps *o,BkAlbumFrame *f,char e[256]) {
  draw(s,f,BK_ALBUM_PAGE);
  for(unsigned i=1;i<8;++i)draw(s,f,i);
  for(unsigned i=18;i<22;++i)draw(s,f,i);
  for(unsigned i=30;i<40;++i)draw(s,f,i);
  draw(s,f,1);
  if(!item_hover(s,in,o,e))return 0;
  for(unsigned row=0;row<4;++row)for(unsigned col=0;col<5;++col)
    if(s->deleted[s->group][(s->page%5)*20+row*5+col]) {
      position(s,22,(float)((240.+200*col)*s->scale),(float)((200.+160*row)*s->scale));
      s->sprites[22].hidden=0;draw(s,f,22);
    }
  if(!page_button(s,in,o,1,e) || !page_button(s,in,o,0,e))return 0;
  if(hit(s->sprites[3].rect,in)) {
    if(!s->button_hover[2]) {if(!o->sound(o->context,3,e))return 0;s->button_hover[2]=1;}
    s->sprites[3].hidden=0;s->hover=-2;
    if(in->confirm==3) {if(!o->sound(o->context,0,e))return 0;s->state=4;}
  } else {s->sprites[3].hidden=1;s->button_hover[2]=0;}
  if(hit(s->sprites[19].rect,in) && in->confirm==3) {
    if(!o->sound(o->context,0,e) || !release_image(s,o,8,e))return 0;
    s->sprites[19].hidden=s->erase?1:0;
    if(!image(s,o,8,BK_ALBUM_ASSET,s->erase?0:1,e))return 0;
    s->erase=s->erase?0:1;
  }
  if(hit(s->sprites[21].rect,in)) {
    if(!s->button_hover[3]) {if(!o->sound(o->context,3,e))return 0;s->button_hover[3]=1;}
    s->sprites[21].hidden=0;
    if(in->confirm==3) {
      if(!o->sound(o->context,0,e))return 0;
      if(s->live_count) {s->state=7;s->slide_index=0;}
    }
  } else {s->sprites[21].hidden=1;s->button_hover[3]=0;}
  if(!tabs(s,in,o,e))return 0;
  unsigned digit=10+(unsigned)(s->page%5);
  position(s,digit,(float)(640.*s->scale),(float)(72.*s->scale));draw(s,f,digit);
  if(s->hover>=0 && in->confirm==3) {
    if(!s->erase) {
      if(!s->deleted[s->group][s->hover]) {
        if(!o->sound(o->context,0,e) || !release_image(s,o,BK_ALBUM_PHOTO,e) ||
           !image(s,o,BK_ALBUM_PHOTO,BK_ALBUM_STILL,(unsigned)s->hover,e))return 0;
        if(s->loaded & (UINT64_C(1)<<BK_ALBUM_PHOTO))s->state=3;
      }
    } else {
      if(!o->sound(o->context,5,e) || !o->remove(o->context,s->group,s->hover,e))return 0;
      s->deleted[s->group][s->hover]=1;
      if(!o->select(o->context,s->group,&s->live_count,e))return 0;
    }
  }
  if(in->back==3 && s->erase) {
    if(!o->sound(o->context,2,e))return 0;
    s->sprites[19].hidden=1;
    /*Original reload overwrites this cursor without releasing it. The service
     * owns replacement retirement; observable UI state remains identical.*/
    if(!image(s,o,8,BK_ALBUM_ASSET,0,e))return 0;
    s->erase=0;
  }
  return 1;
}
int bk_album_menu_step(BkAlbumMenu *s,const BkAlbumInput *in,const BkAlbumOps *o,
                        BkAlbumFrame *f,char e[256]) {
  if(!s || !in || !f || !services(o) || !isfinite(s->scale) || s->scale<=0 ||
     s->state<0 || s->state>8 || s->group<0 || s->group>=5 || s->page<0 || s->page>=25 ||
     s->count<0 || s->count>101 || s->live_count<0 || s->catalog_index<0 || s->catalog_index>25 ||
     s->catalog_subpage<0 || s->catalog_subpage>5 || s->slide_index<0 || in->delta_ms>100 ||
     !isfinite(s->curtain) || !isfinite(s->progress))return fail(e,"invalid state/input");
  *f=(BkAlbumFrame){0};
  switch(s->state) {
  case 1:
    if(!o->scan(o->context,s->counts,e))return 0;
    s->count=s->counts[0];
    draw(s,f,23);draw(s,f,28);
    s->sprites[25].uv[2]=s->sprites[25].scale[0]=.04f;
    draw(s,f,25);draw(s,f,24);s->state=2;s->page=0;break;
  case 2:
    draw(s,f,23);draw(s,f,28);
    if(s->catalog_index<25) {
      if(s->catalog_subpage>=5)s->catalog_subpage=0;
      if(!o->catalog(o->context,s->catalog_subpage,s->catalog_index,e))return 0;
      ++s->catalog_subpage;++s->catalog_index;s->progress=(float)((double)s->progress+(double).04f);
    } else {
      s->state=6;
      if(!image(s,o,BK_ALBUM_PAGE,BK_ALBUM_CATALOG,0,e))return 0;
    }
    s->sprites[25].uv[2]=s->sprites[25].scale[0]=s->progress;
    position(s,25,(float)(32.*s->scale),(float)(864.*s->scale));
    draw(s,f,25);draw(s,f,24);break;
  case 6:
    draw(s,f,BK_ALBUM_PAGE);s->curtain=(float)((double)s->curtain-(double)in->delta_ms/500);
    if(s->curtain<=0) {
      s->curtain=0;s->state=0;s->group=0;memset(s->deleted,0,sizeof(s->deleted));
      if(!o->select(o->context,0,&s->live_count,e))return 0;
    }
    s->sprites[15].alpha=s->curtain;draw(s,f,15);break;
  case 0:if(!grid(s,in,o,f,e))return 0;break;
  case 3:
    draw(s,f,BK_ALBUM_PHOTO);
    if(in->back==3) {
      if(!o->sound(o->context,2,e) || !release_image(s,o,BK_ALBUM_PHOTO,e))return 0;
      s->state=0;
    }
    break;
  case 7: {
    if(s->slide_index==2)s->catalog_index=0;
    int present=0;
    if(!o->probe(o->context,s->group,s->slide_index,&present,e))return 0;
    if(present) {
      if(!image(s,o,BK_ALBUM_SLIDE,BK_ALBUM_SLIDESHOW,s->slide_index,e))return 0;
      s->state=8;
      if(!o->sound(o->context,0,e))return 0;
      draw(s,f,BK_ALBUM_SLIDE);
    } else ++s->slide_index;
    if(s->slide_index>s->live_count)s->state=0;
    break;
  }
  case 8:
    draw(s,f,BK_ALBUM_SLIDE);s->slide_ms+=in->delta_ms;
    if(s->slide_ms/2000>0) {
      s->slide_ms=0;
      if(!release_image(s,o,BK_ALBUM_SLIDE,e))return 0;
      ++s->slide_index;s->state=s->slide_index>s->count?0:7;
    }
    if((in->confirm&1) || (in->back&1)) {
      if(!o->sound(o->context,2,e) || !release_image(s,o,BK_ALBUM_SLIDE,e))return 0;
      s->state=0;
    }
    break;
  case 4:
    draw(s,f,BK_ALBUM_PAGE);s->curtain=(float)((double)s->curtain+(double)in->delta_ms/500);
    if(s->curtain>=1) {
      int active=0;s->curtain=1;
      if(!o->playing(o->context,0,&active,e))return 0;
      if(!active) {
        s->exit=1;s->catalog_index=s->catalog_subpage=s->slide_index=0;
        s->curtain=s->spare_alpha=1;s->progress=0;s->spare_duration=1000;
        memset(s->button_hover,0,sizeof(s->button_hover));memset(s->tab_hover,0,sizeof(s->tab_hover));
        memset(s->selected,0,sizeof(s->selected));s->selected[0]=1;
        memset(s->spare_hover,0,sizeof(s->spare_hover));s->slide_ms=0;s->erase=0;
        memset(s->item_hover,0,sizeof(s->item_hover));
      }
    }
    s->sprites[15].alpha=s->curtain;draw(s,f,15);break;
  default:break;
  }
  if(s->loaded & (UINT64_C(1)<<8)) {
    if(s->state==7 || s->state==8)position(s,8,-100,-100);
    else position(s,8,(float)in->x,(float)in->y);
    draw(s,f,8);
    s->sprites[8].hidden=s->state==4 || s->state==3 || s->state==1 || s->state==2;
  }
  return 1;
}
int bk_album_menu_release(BkAlbumMenu *s,const BkAlbumOps *o,char e[256]) {
  if(!s || !services(o))return fail(e,"missing release owner/services");
  for(unsigned i=0;i<29;++i)if(!release_image(s,o,i,e))return 0;
  for(unsigned i=30;i<40;++i)if(!release_image(s,o,i,e))return 0;
  return release_image(s,o,BK_ALBUM_SLIDE,e) && release_image(s,o,BK_ALBUM_PHOTO,e) &&
      release_image(s,o,BK_ALBUM_PAGE,e);
}
