/* Real title->volume->title->selection->dialogue->game->pause->volume->pause.
 * Only the initial RNG seed is a fixture; later flow/state changes use input. */
#include "app/play_session.h"
#include "save/capture_file.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) {if(!*e)snprintf(e,256,"line%d: %s",__LINE__,#x);goto done;} } while(0)
typedef struct {BkRenderer *r;BkAudio *audio;BkScene *scene;uint64_t samples,nonzero,hash,submitted;unsigned frames,redraws;} Run;
static int submit(void *p,const int16_t *pcm,size_t n,char e[256]) {
  (void)e;Run *r=p;
  for(size_t i=0;i<n*2;++i) {r->nonzero+=pcm[i]!=0;r->hash^=(uint16_t)pcm[i];r->hash*=UINT64_C(1099511628211);}
  r->submitted+=n;r->samples+=n*2;return 1;
}
static int poll(void *p,uint64_t *out,char e[256]) {(void)e;*out=((Run *)p)->submitted;return 1;}
static int present(Run *r,char e[256]) {
  return bk_renderer_begin(r->r,e) && bk_scene_draw(r->scene,&(BkSceneFrame){0},e) && bk_renderer_end(r->r,e) && bk_play_session_after_present(r->scene,e);
}
static int tick(Run *r,BkInput input,char e[256]) {
  ++r->frames;
  return bk_audio_poll(r->audio,e) && bk_scene_step(r->scene,1./60,&input,e) && bk_audio_fill(r->audio,e) && present(r,e);
}
static unsigned flow(Run *r) {return bk_play_session_flow(r->scene)->current;}
static BkInput point(float x,float y,int click) {
  return (BkInput){.pointer_active=1,.pointer_x=160+x*.75f,.pointer_y=y*.75f,
      .pressed=click?BK_BUTTON_CONFIRM:0,.held=click?BK_BUTTON_CONFIRM:0};
}
static int idle(Run *r,unsigned n,char e[256]) {while(n--)if(!tick(r,(BkInput){0},e))return 0;return 1;}
static int wait_flow(Run *r,unsigned target,unsigned limit,char e[256]) {
  for(unsigned i=0;i<limit && flow(r)!=target;++i)
    if(!tick(r,(BkInput){.pressed=i%20==19?BK_BUTTON_CONFIRM:0},e))return 0;
  if(flow(r)!=target) {snprintf(e,256,"wanted flow%02x, got%02x",target,flow(r));return 0;}return 1;
}
static int open_volume(Run *r,unsigned from,char e[256]) {
  if(!idle(r,180,e))return 0;
  for(unsigned i=0;i<180 && flow(r)==from;++i) {
    BkInput in=from==1?point(1084,192,i%20==19):point(640,408,i%20==19);
    if(!tick(r,in,e))return 0;
  }
  return wait_flow(r,0x30,600,e) && idle(r,45,e);
}
static int adjust(Run *r,const int32_t desired[3],char e[256]) {
  static const unsigned ys[]={333,477,624};
  for(unsigned i=0;i<3;++i) {
    BkInput in=point(500,ys[i],1);if(!tick(r,in,e))return 0;
    if(i==0 && desired[i]==-6000) {
      /* A held with the left stick uses the same real drag path as touch. */
      for(unsigned j=0;j<24;++j)
        if(!tick(r,(BkInput){.held=BK_BUTTON_CONFIRM,.move_x=-1},e))return 0;
      if(!tick(r,(BkInput){.released=BK_BUTTON_CONFIRM},e))return 0;
      continue;
    }
    /* Original scaled min/max are243/711, not a rounded unscaled delta. */
    in.pointer_x=160+711+(float)desired[i]/6000*468;in.pressed=0;in.held=BK_BUTTON_CONFIRM;
    if(!tick(r,in,e) || !tick(r,(BkInput){.released=BK_BUTTON_CONFIRM},e))return 0;
  }
  return 1;
}
static int redraw(Run *r,char e[256]) {
  size_t size=1280*720*4;uint8_t *a=malloc(size),*b=malloc(size);int ok=0;
  BkGameFrameState before=*bk_play_session_state(r->scene);BkCommonHudState common=*bk_play_session_common(r->scene);uint64_t pcm=r->hash;
  if(a && b && bk_renderer_readback(r->r,a,size,e) && present(r,e) && bk_renderer_readback(r->r,b,size,e))
    ok=!memcmp(a,b,size) && !memcmp(&before,bk_play_session_state(r->scene),sizeof(before)) && !memcmp(&common,bk_play_session_common(r->scene),sizeof(common)) && pcm==r->hash;
  free(a);free(b);if(ok)++r->redraws;return ok;
}
static int save_return(Run *r,BkVolumeFile *v,const int32_t wanted[3],unsigned target,char e[256]) {
  if(target==1) {
    for(unsigned i=0;i<4;++i)
      if(!tick(r,(BkInput){.pressed=BK_BUTTON_UP},e) || !idle(r,1,e))return 0;
    for(unsigned i=0;i<3;++i)
      if(!tick(r,(BkInput){.pressed=BK_BUTTON_DOWN},e) || !idle(r,1,e))return 0;
    /* Right from save must not focus the native invisible debug button. */
    if(!tick(r,(BkInput){.pressed=BK_BUTTON_RIGHT},e) || !idle(r,1,e) ||
       !tick(r,(BkInput){.pressed=BK_BUTTON_CONFIRM,.held=BK_BUTTON_CONFIRM},e))return 0;
  } else if(!tick(r,point(1104,908,1),e))return 0;
  if(memcmp(bk_volume_file_values(v),wanted,12) || !redraw(r,e))return 0;
  if(!wait_flow(r,target,600,e) || !idle(r,180,e))return 0;
  for(unsigned i=40;i<46;++i) {int32_t vol,pan;if(bk_audio_get_gain(r->audio,i,&vol,&pan))return 0;}
  return 1;
}
int main(int argc,char **argv) {
  if(argc!=4 || (strcmp(argv[3],"produce") && strcmp(argv[3],"reload") && strcmp(argv[3],"reject")))return 2;
  const int32_t first[]={-6000,-3000,0},second[]={-1500,-6000,-3000};
  int reject=!strcmp(argv[3],"reject"),reload=!strcmp(argv[3],"reload"),result=1;
  char e[256]={0},path[1200];Run run={.hash=UINT64_C(14695981039346656037)};Run *r=&run;
  BkResourceStore *store=bk_resources_create(e);BkCaptureFiles *captures=NULL;BkCheckpointFiles *saves=NULL;BkUnlockFile *unlocks=NULL;BkRecordFile *records=NULL;BkVolumeFile *volume=NULL;
  CHECK(store);
  const char *packs[]={"bk3_00","bk3_01","bk3_02","bk3_03","bk3_04","bk3_05","bk3_06","bk3_07","bk3_15","bk3_16","bk3_20","bk3_18"};
  for(unsigned i=0;i<12;++i) {snprintf(path,sizeof(path),"%s/%s.pp",argv[1],packs[i]);CHECK(bk_resources_mount(store,packs[i],path,e));}
  const char *loose[]={"routes","faces","collision","fonts"};
  for(unsigned i=0;i<4;++i) CHECK(bk_resources_mount_directory(store,loose[i],argv[1],16*1024*1024,e));
  r->r=bk_renderer_create(1280,720,stderr,e);CHECK(r->r);
  captures=bk_capture_files_create(argv[2],e);saves=bk_checkpoint_files_create(argv[2],e);unlocks=bk_unlock_file_create(argv[2],e);records=bk_record_file_create(argv[2],e);volume=bk_volume_file_create(argv[2],NULL,e);CHECK(captures && saves && unlocks && records && volume);
  BkAudioSink sink={r,48000,240,960,submit,poll};r->audio=bk_audio_create(&sink,e);CHECK(r->audio);
  uint64_t baseline=bk_renderer_stats(r->r).live_allocations;
  BkSceneServices services={store,r->r,stderr,r->audio,captures,bk_volume_file_values(volume)};
  r->scene=bk_play_session_create_with_progress(&services,saves,unlocks,records,volume,e);CHECK(r->scene && present(r,e));
  ((BkGameFrameState *)bk_play_session_state(r->scene))->random=0x4ac6a7; /*initial RNG fixture only*/
  CHECK(idle(r,180,e));
  int32_t gain,pan;CHECK(bk_audio_get_gain(r->audio,60,&gain,&pan) && gain==bk_volume_file_values(volume)[1]);
  if(reload) {CHECK(!memcmp(bk_volume_file_values(volume),second,12));goto success;}
  int32_t old[3];memcpy(old,bk_volume_file_values(volume),12);
  CHECK(open_volume(r,1,e) && redraw(r,e));
  CHECK(tick(r,(BkInput){.pressed=BK_BUTTON_UP},e) && idle(r,1,e));
  for(unsigned i=0;i<3;++i)CHECK(tick(r,(BkInput){.held=BK_BUTTON_LEFT},e));
  CHECK(idle(r,1,e));
  CHECK(tick(r,(BkInput){.pressed=BK_BUTTON_CONFIRM,.held=BK_BUTTON_CONFIRM},e));
  CHECK(bk_audio_get_gain(r->audio,40,&gain,&pan) && gain==-76);
  CHECK(idle(r,1,e) && adjust(r,first,e));
  CHECK(!memcmp(old,bk_volume_file_values(volume),12)); /*private edits until save*/
  if(reject) {
    snprintf(path,sizeof(path),"%s/save/volume.cfg.part",argv[2]);CHECK(!mkdir(path,0777));
    CHECK(!tick(r,point(1104,908,1),e) && strstr(e,"temporary settings") && flow(r)==0x30);
    CHECK(!memcmp(old,bk_volume_file_values(volume),12));e[0]=0;CHECK(!rmdir(path));goto success;
  }
  /* Real sample voices use their category values. First two previews are
   * deterministic; the original effects sample may select NULL slot7. */
  CHECK(tick(r,point(990,278,1),e) && bk_audio_get_gain(r->audio,40,&gain,&pan) && gain==first[0]);
  CHECK(tick(r,(BkInput){0},e));
  CHECK(tick(r,point(990,424,1),e) && bk_audio_get_gain(r->audio,41,&gain,&pan) && gain==first[1]);
  CHECK(tick(r,(BkInput){0},e));
  CHECK(save_return(r,volume,first,1,e) && bk_audio_get_gain(r->audio,60,&gain,&pan) && gain==first[1]);
  /* Verify the original title->selection->story path still accepts settings. */
  for(unsigned i=0;i<600 && flow(r)==1;++i)CHECK(tick(r,point(1084,53,i%20==19),e));
  CHECK(wait_flow(r,0x38,600,e));unsigned speech_checks=0;
  for(unsigned i=0;i<600 && flow(r)==0x38;++i) {
    /* Open the character profile, then its voice-replay icon. Merely
     * selecting the already selected character does not play speech. */
    BkInput in=i<130?point(112,903,i==100):i<180?point(357,639,i==140):point(832,908,i%20==19);
    CHECK(tick(r,in,e));
    if(bk_audio_get_gain(r->audio,61,&gain,&pan)) {CHECK(gain==first[0]);++speech_checks;}
  }
  CHECK(speech_checks && wait_flow(r,8,600,e));
  for(unsigned i=0;i<10000 && flow(r)==8;++i)CHECK(tick(r,(BkInput){.pressed=i%6==5?BK_BUTTON_CONFIRM:0},e));
  CHECK(wait_flow(r,2,600,e));
  for(unsigned i=0;i<3600 && bk_play_session_state(r->scene)->camera.phase!=1;++i)CHECK(tick(r,(BkInput){.pressed=i%30==29?BK_BUTTON_CONFIRM:0},e));
  CHECK(bk_play_session_state(r->scene)->camera.phase==1 && idle(r,60,e));
  CHECK(tick(r,(BkInput){.pressed=BK_BUTTON_PAUSE},e) && flow(r)==4);
  BkGameFrameState frozen=*bk_play_session_state(r->scene);
  CHECK(open_volume(r,4,e) && adjust(r,second,e) && save_return(r,volume,second,4,e));
  BkGameFrameState held=*bk_play_session_state(r->scene);frozen.background.music_volume=held.background.music_volume;
  CHECK(!memcmp(&held,&frozen,sizeof(held)) && redraw(r,e));
  /* Resume using the authored pause row, then ensure actual game updates. */
  for(unsigned i=0;i<180 && flow(r)==4;++i)CHECK(tick(r,point(640,480,i%20==19),e));
  CHECK(flow(r)==2 && idle(r,60,e));
  CHECK(bk_audio_get_gain(r->audio,8,&gain,&pan) && gain==second[1]);
  CHECK(bk_audio_get_gain(r->audio,48,&gain,&pan) && gain==second[2]);
  printf("volume-flow categories speech_checks=%u resumed_music=%d effect=%d\n",speech_checks,second[1],second[2]);
success:
  CHECK(run.nonzero && run.frames && (!reject || flow(r)==0x30));
  bk_scene_destroy(r->scene);r->scene=NULL;
  CHECK(bk_renderer_stats(r->r).live_allocations==baseline);
  printf("volume-flow PASS mode=%s frames=%u redraws=%u volumes=%d,%d,%d PCM=%" PRIu64 " nonzero=%" PRIu64 " hash=%016" PRIx64 "\n",argv[3],r->frames,r->redraws,bk_volume_file_values(volume)[0],bk_volume_file_values(volume)[1],bk_volume_file_values(volume)[2],r->samples,r->nonzero,r->hash);
  result=0;
done:
  if(result)fprintf(stderr,"volume-flow FAIL: %s flow%02x frame%u\n",e,r->scene?flow(r):0,r->frames);
  bk_scene_destroy(r->scene);bk_audio_destroy(r->audio);bk_renderer_destroy(r->r);bk_resources_destroy(store);
  bk_volume_file_destroy(volume);bk_record_file_destroy(records);bk_unlock_file_destroy(unlocks);bk_checkpoint_files_destroy(saves);bk_capture_files_destroy(captures);return result;
}
