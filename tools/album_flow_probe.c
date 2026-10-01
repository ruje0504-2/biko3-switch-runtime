/* Actual title -> album -> title; synthetic photos in an empty owned directory.
 * Inputs drive every page/photo/delete/slide transition, not menu state writes. */
#include "app/play_session.h"
#include "save/capture_file.h"
#include "resource/bitmap.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#define SIZE ((size_t)1280*720*4)
#define CHECK(x) do {if(!(x)){if(!*e)snprintf(e,256,"line%d: %s",__LINE__,#x);goto done;}}while(0)
typedef struct {BkRenderer *r;BkAudio *audio;BkScene *scene;uint64_t submitted,hash;unsigned frames,redraws;} Run;
static int submit(void *p,const int16_t *pcm,size_t n,char e[256]) {
  (void)e;Run *r=p;r->submitted+=n;
  for(size_t i=0;i<n*2;++i){r->hash^=(uint16_t)pcm[i];r->hash*=UINT64_C(1099511628211);}return 1;
}
static int poll(void *p,uint64_t *out,char e[256]){(void)e;*out=((Run*)p)->submitted;return 1;}
static int present(Run *r,char e[256]) {return bk_renderer_begin(r->r,e) && bk_scene_draw(r->scene,&(BkSceneFrame){0},e) && bk_renderer_end(r->r,e) && bk_play_session_after_present(r->scene,e);}
static int tick(Run *r,BkInput in,char e[256]) {++r->frames;return bk_audio_poll(r->audio,e) && bk_scene_step(r->scene,1./60,&in,e) && bk_audio_fill(r->audio,e) && present(r,e);}
static int idle(Run *r,unsigned n,char e[256]) {while(n--)if(!tick(r,(BkInput){0},e))return 0;return 1;}
static unsigned flow(Run *r){return bk_play_session_flow(r->scene)->current;}
static BkInput point(float x,float y,int click){return (BkInput){.pointer_active=1,.pointer_x=160+x*.75f,.pointer_y=y*.75f,.pressed=click?BK_BUTTON_CONFIRM:0,.held=click?BK_BUTTON_CONFIRM:0};}
static int click(Run *r,float x,float y,char e[256]){return tick(r,point(x,y,1),e) && idle(r,1,e);}
static int wait_flow(Run *r,unsigned target,unsigned n,char e[256]) {
  while(n-- && flow(r)!=target)if(!idle(r,1,e))return 0;
  if(flow(r)==target)return 1;snprintf(e,256,"flow%02x, wanted%02x",flow(r),target);return 0;
}
static int open_album(Run *r,char e[256]) {
  return idle(r,180,e) && click(r,1084,333,e) && wait_flow(r,0x60,300,e) && idle(r,100,e);
}
static int pixel(Run *r,uint8_t *rgba,unsigned x,unsigned y,const uint8_t rgb[3],char e[256]) {
  if(!bk_renderer_readback(r->r,rgba,SIZE,e))return 0;
  const uint8_t *p=rgba+((size_t)y*1280+x)*4;
  if(!memcmp(p,rgb,3))return 1;
  snprintf(e,256,"pixel%u,%u got%u,%u,%u wanted%u,%u,%u",x,y,p[0],p[1],p[2],rgb[0],rgb[1],rgb[2]);return 0;
}
static int redraw(Run *r,uint8_t *a,uint8_t *b,char e[256]) {
  BkRenderStats old=bk_renderer_stats(r->r);uint64_t audio=r->hash;
  if(!bk_renderer_readback(r->r,a,SIZE,e) || !present(r,e) || !bk_renderer_readback(r->r,b,SIZE,e))return 0;
  BkRenderStats now=bk_renderer_stats(r->r);
  if(memcmp(a,b,SIZE) || audio!=r->hash || old.mesh_updates!=now.mesh_updates || old.live_allocations!=now.live_allocations)return 0;
  ++r->redraws;return 1;
}
static int photo_color(BkCaptureFiles *files,const char *name,uint8_t rgb[3],char e[256]) {
  BkBlob raw={0};BkImage im={0};int ok=bk_capture_file_read_photo(files,name,1024,&raw,e)==BK_RESOURCE_OK && bk_image_decode(raw.data,raw.size,&im,e);
  if(ok)memcpy(rgb,im.rgba,3);bk_image_free(&im);bk_blob_free(&raw);return ok;
}
int main(int argc,char **argv) {
  if(argc!=3)return 2;
  char e[256]={0},path[1200],name[64];int result=1;Run run={.hash=UINT64_C(14695981039346656037)};Run *r=&run;
  BkResourceStore *store=NULL;BkCaptureFiles *files=NULL;BkCheckpointFiles *saves=NULL;BkUnlockFile *unlock=NULL;BkRecordFile *records=NULL;BkVolumeFile *volume=NULL;
  uint8_t *a=malloc(SIZE),*b=malloc(SIZE);BkPhotoList lists[5]={0},live={0};BkBlob encoded={0};int32_t counts[5];
  CHECK(a && b);files=bk_capture_files_create(argv[2],e);CHECK(files && bk_capture_files_count_photos(files,counts,e));
  CHECK(!memcmp(counts,(int32_t[5]){0},sizeof(counts)));
  const char *prefix[]={"ri_","re_","cr_","ma_","mi_"};
  for(unsigned g=0;g<5;++g)for(unsigned i=0;i<21;++i) {
    uint8_t pixels[4*4*4];for(unsigned j=0;j<16;++j){pixels[j*4]=40+g*40;pixels[j*4+1]=50+i*7;pixels[j*4+2]=90+i*3;pixels[j*4+3]=255;}
    BkImage im={4,4,pixels};CHECK(bk_bitmap_encode(&im,&encoded,e));
    snprintf(name,sizeof(name),"%s%03u.bmp",prefix[g],i);CHECK(bk_capture_file_write(files,1,name,&encoded,e));bk_blob_free(&encoded);
  }
  for(unsigned g=0;g<5;++g)CHECK(bk_capture_files_list_photos(files,g,100,&lists[g],e) && lists[g].count==21);
  store=bk_resources_create(e);CHECK(store);
  const char *packs[]={"bk3_00","bk3_01","bk3_02","bk3_03","bk3_04","bk3_05","bk3_06","bk3_07","bk3_15","bk3_16","bk3_18","bk3_19","bk3_20"};
  for(unsigned i=0;i<sizeof(packs)/sizeof(*packs);++i){snprintf(path,sizeof(path),"%s/%s.pp",argv[1],packs[i]);CHECK(bk_resources_mount(store,packs[i],path,e));}
  const char *loose[]={"routes","faces","collision","fonts"};for(unsigned i=0;i<4;++i)CHECK(bk_resources_mount_directory(store,loose[i],argv[1],16*1024*1024,e));
  saves=bk_checkpoint_files_create(argv[2],e);unlock=bk_unlock_file_create(argv[2],e);records=bk_record_file_create(argv[2],e);volume=bk_volume_file_create(argv[2],e);CHECK(saves && unlock && records && volume);
  r->r=bk_renderer_create(1280,720,stderr,e);CHECK(r->r);uint64_t baseline=bk_renderer_stats(r->r).live_allocations;
  BkAudioSink sink={r,48000,240,960,submit,poll};r->audio=bk_audio_create(&sink,e);CHECK(r->audio);
  BkSceneServices services={store,r->r,stderr,r->audio,files,bk_volume_file_values(volume)};
  r->scene=bk_play_session_create_with_progress(&services,saves,unlock,records,volume,e);CHECK(r->scene && present(r,e));
  CHECK(open_album(r,e));unsigned pictures=0,pages=0;uint8_t color[3];
  for(unsigned g=0;g<5;++g) {
    CHECK(click(r,92,206+g*150,e));
    CHECK(photo_color(files,lists[g].names[0],color,e));
    CHECK(pixel(r,a,400,195,color,e));
    CHECK(click(r,320,260,e) && pixel(r,a,640,360,color,e));++pictures;
    CHECK(tick(r,(BkInput){.pressed=BK_BUTTON_BACK,.held=BK_BUTTON_BACK},e) && redraw(r,a,b,e) && idle(r,1,e));
    /* Page swap must keep the preceding image on the click frame. */
    CHECK(tick(r,point(832,100,1),e) && pixel(r,a,400,195,color,e));
    CHECK(redraw(r,a,b,e) && idle(r,1,e));
    CHECK(photo_color(files,lists[g].names[20],color,e) && pixel(r,a,400,195,color,e));++pages;
    CHECK(click(r,320,260,e) && pixel(r,a,640,360,color,e));++pictures;
    CHECK(tick(r,(BkInput){.pressed=BK_BUTTON_BACK,.held=BK_BUTTON_BACK},e) && idle(r,1,e));
    CHECK(click(r,600,100,e));
  }
  CHECK(click(r,92,206,e) && click(r,1130,112,e) && click(r,320,260,e));
  BkBlob absent={0};CHECK(bk_capture_file_read_photo(files,lists[0].names[0],1024,&absent,e)==BK_RESOURCE_MISSING);
  CHECK(bk_capture_files_count_photos(files,counts,e) && counts[0]==20);
  for(unsigned g=1;g<5;++g)CHECK(counts[g]==21);
  CHECK(tick(r,(BkInput){.pressed=BK_BUTTON_BACK,.held=BK_BUTTON_BACK},e) && idle(r,1,e));
  CHECK(bk_capture_files_list_photos(files,0,100,&live,e) && live.count==20);
  CHECK(click(r,1130,50,e) && photo_color(files,live.names[0],color,e) && pixel(r,a,640,360,color,e));
  CHECK(idle(r,130,e) && photo_color(files,live.names[1],color,e) && pixel(r,a,640,360,color,e));
  CHECK(tick(r,(BkInput){.pressed=BK_BUTTON_BACK,.held=BK_BUTTON_BACK},e) && redraw(r,a,b,e) && idle(r,1,e));
  CHECK(click(r,1104,908,e) && wait_flow(r,1,300,e) && idle(r,90,e));
  CHECK(!memcmp(bk_play_session_state(r->scene)->hotkeys.photos,counts,sizeof(counts)));
  /* Reentry rebuilds the grid from the surviving files, not stale indices. */
  CHECK(open_album(r,e) && photo_color(files,live.names[0],color,e) && pixel(r,a,400,195,color,e));
  CHECK(tick(r,(BkInput){.move_x=.8f},e) && tick(r,(BkInput){.held=BK_BUTTON_DOWN|BK_BUTTON_SLOW},e));
  CHECK(redraw(r,a,b,e));
  CHECK(click(r,1104,908,e) && wait_flow(r,1,300,e) && idle(r,90,e));
  bk_scene_destroy(r->scene);r->scene=NULL;CHECK(bk_renderer_stats(r->r).live_allocations==baseline);
  printf("PASS album-flow frames%u groups5 pages%u photos%u deletes1 slideshow2 reentries1 redraws%u PCM%016" PRIx64 " GPU_baseline\n",r->frames,pages,pictures,r->redraws,r->hash);result=0;
done:
  if(result)fprintf(stderr,"FAILED album-flow frame%u flow%02x: %s\n",r->frames,r->scene?flow(r):0,e);
  bk_scene_destroy(r->scene);bk_audio_destroy(r->audio);bk_renderer_destroy(r->r);bk_resources_destroy(store);
  bk_checkpoint_files_destroy(saves);bk_unlock_file_destroy(unlock);bk_record_file_destroy(records);bk_volume_file_destroy(volume);bk_capture_files_destroy(files);
  for(unsigned g=0;g<5;++g)bk_photo_list_free(&lists[g]);bk_photo_list_free(&live);bk_blob_free(&encoded);free(a);free(b);return result;
}
