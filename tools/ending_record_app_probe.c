/* Actual recorder -> flow10 save -> closing dialogue/unlock -> new-process
 * load -> menu-selected replay. Story/selected-stage handoffs are explicit
 * boundary fixtures. Actions, terminal21, clocks, PCM, returns and persistence
 * all run production code; no recorded lane or unlock flag is prepopulated. */
#include "app/front_end.h"
static double record_wall_step=1./60.;
#define BK_APP_PROBE_WALL_STEP record_wall_step
static unsigned record_group, record_variant;
static int record_result(const BkFrontEnd *f,BkDialogueResult *r,char e[256]) {
  if(!bk_front_end_result(f,r,e))return 0;
  r->group=(int32_t)record_group;r->kind=(int32_t)record_variant;return 1;
}
#define bk_front_end_result record_result
#define main gallery_regression_main
#include "ending_gallery_app_probe.c"
#undef main
#undef bk_front_end_result
#undef CHECK
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"record app line%d (%s): %s\n",__LINE__,#x,error); goto done; } } while(0)
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
int record_probe_point(BkScene *,BkInput *,char[256]);
int record_probe_alternate(BkScene *,BkInput *,char[256]);
int record_probe_active(BkScene *,int32_t *,char[256]);
static int expected_file(const char *root,PlaySession *s,int write,char e[256]) {
  char path[2048];snprintf(path,sizeof(path),"%s/expected.bkr",root);
  uint8_t *bytes=malloc(BK_RECORD_FILE_BYTES),*other=malloc(BK_RECORD_FILE_BYTES);
  if(!bytes||!other){free(bytes);free(other);return 0;}
  BkRecordView v[5];record_views(s,v);
  int ok=bk_record_encode(v,bytes,e);
  FILE *f=ok?fopen(path,write?"wb":"rb"):NULL;
  if(!f)ok=0;
  else {
    if(write)ok=fwrite(bytes,1,BK_RECORD_FILE_BYTES,f)==BK_RECORD_FILE_BYTES;
    else ok=fread(other,1,BK_RECORD_FILE_BYTES,f)==BK_RECORD_FILE_BYTES && fgetc(f)==EOF && !memcmp(bytes,other,BK_RECORD_FILE_BYTES);
    if(fclose(f))ok=0;
  }
  free(bytes);free(other);return ok;
}
int main(int argc,char **argv) {
  if(argc!=6&&argc!=7){fprintf(stderr,"usage: ending-record-app-probe DATA OUTPUT produce|replay GROUP VARIANT [mixed-clock]\n");return 2;}
  int mixed_clock=argc==7;
  if(mixed_clock&&strcmp(argv[6],"mixed-clock"))return 2;
  record_wall_step=mixed_clock?.05:1./60.;
  const unsigned input_limit=mixed_clock?22000:66000;
  const unsigned replay_limit=mixed_clock?14000:42000;
  int produce=!strcmp(argv[3],"produce"),result=1;
  if(!produce && strcmp(argv[3],"replay"))return 2;
  record_group=(unsigned)strtoul(argv[4],NULL,10);record_variant=(unsigned)strtoul(argv[5],NULL,10);
  if(record_group>=5||record_variant>1)return 2;
  char error[256]={0},path[2048];
  BkResourceStore *store=NULL;BkRenderer *renderer=NULL;BkAudio *audio=NULL;
  BkScene *scene=NULL;BkUnlockFile *unlocks=NULL;BkRecordFile *records=NULL;
  Sink sink={.hash=UINT64_C(14695981039346656037)};
  CHECK(store=bk_resources_create(error));
  const char *packs[]={"bk3_00","bk3_01","bk3_02","bk3_03","bk3_04","bk3_05","bk3_16","bk3_06","bk3_08","bk3_09","bk3_10","bk3_11","bk3_12","bk3_13","bk3_18","fambom"};
  for(unsigned i=0;i<sizeof(packs)/sizeof(*packs);++i){snprintf(path,sizeof(path),"%s/%s.pp",argv[1],packs[i]);CHECK(bk_resources_mount(store,packs[i],path,error));}
  CHECK(bk_resources_mount_directory(store,"faces",argv[1],20480,error));
  CHECK(bk_resources_mount_directory(store,"fonts",argv[1],16*1024*1024,error));
  CHECK(renderer=bk_renderer_create(80,48,stdout,error));BkRenderStats baseline=bk_renderer_stats(renderer);
  BkAudioSink output={&sink,48000,480,1920,submit,poll};CHECK(audio=bk_audio_create(&output,error));
  CHECK(unlocks=bk_unlock_file_create(argv[2],error));CHECK(records=bk_record_file_create(argv[2],error));
  BkSceneServices services={store,renderer,stdout,audio,NULL};
  CHECK(scene=bk_play_session_create_with_progress(&services,NULL,unlocks,records,error));
  PlaySession *s=bk_scene_custom_context(scene);s->game_state.random=0x50caa2;
  CHECK(present(scene,renderer,audio,error));
  if(produce) {
    BkUnlockTable saved={0};CHECK(bk_unlock_file_read(unlocks,&saved,error)==BK_RESOURCE_MISSING);
    for(unsigned g=0;g<5;++g)CHECK(!s->ending_records.groups[g].count);
    /* Explicit negative migration case: missing Gray data must fail before
     * loading replay resources. This creates no record or unlock fixture. */
    BkRenderStats before_missing=bk_renderer_stats(renderer);
    BkScene *missing=bk_ending_replay_scene_create(&services,record_group,
        record_variant,&s->ending_records,saved.flags,NULL,error);
    int rejected=missing==NULL;bk_scene_destroy(missing);
    CHECK(rejected && strstr(error,"missing or incomplete"));error[0]=0;
    BkRenderStats after_missing=bk_renderer_stats(renderer);
    CHECK(before_missing.live_allocations==after_missing.live_allocations &&
        before_missing.live_bytes==after_missing.live_bytes);
    CHECK(wait_frames(scene,renderer,audio,&sink,64,pointer(4,920),error));
    s->game_state.group=record_group;s->game_state.area=0;
    CHECK(schedule(s,0x10,0,error));s->flow.previous=8;
    s->common.curtain=(BkFadeSprite){1,2,3};s->common.action=s->common.blocked=0;
    CHECK(await_flow(scene,renderer,audio,&sink,0x10,200,error));
    CHECK(tick(scene,renderer,audio,&sink,pointer(4,920),error));
    CHECK(settle(scene,renderer,audio,&sink,error));
    /* A failed disk write must precede logical stop/retirement. */
    int32_t volume,pan;
    CHECK(bk_audio_get_gain(audio,47,&volume,&pan));
    BkRecordFile *owner=s->record_file;s->record_file=NULL;
    CHECK(!release(s,0x10,error));s->record_file=owner;error[0]=0;
    CHECK(!s->retire_ending&&!s->ending_unlock_valid);
    snprintf(path,sizeof(path),"%s/save/records.bkr.part",argv[2]);CHECK(!mkdir(path,0777));
    CHECK(!release(s,0x10,error));error[0]=0;CHECK(!rmdir(path));
    int32_t after_volume,after_pan;
    CHECK(!s->retire_ending&&!s->ending_unlock_valid&&
        bk_audio_get_gain(audio,47,&after_volume,&after_pan)&&volume==after_volume&&pan==after_pan);
    /* Supply only the selected-stage handoff, not record contents. */
    s->ending_state.auxiliary.selection=0;s->common.action=7;s->common.blocked=1;
    unsigned n=0;
    while((s->ending_state.frame.phase!=(int32_t)(5+record_variant)||s->ending_state.auxiliary.gate!=1)&&n++<2400)
      CHECK(tick(scene,renderer,audio,&sink,pointer(4,920),error));
    CHECK(n<2400);CHECK(wait_frames(scene,renderer,audio,&sink,90,pointer(4,920),error));
    BkInput point;CHECK(record_probe_point(s->ending,&point,error));
    CHECK(tick(scene,renderer,audio,&sink,point,error));point.held=point.pressed=BK_BUTTON_CONFIRM;
    CHECK(tick(scene,renderer,audio,&sink,point,error));
    CHECK(s->ending_state.auxiliary.gate==3&&s->ending_state.ui_controller.auxiliary.mode==3);
    int32_t choice=s->ending_state.choices[0];
    point.pointer_x=s->viewport.x+(float)s->ending_state.points[0][0];
    point.pointer_y=s->viewport.y+(float)s->ending_state.points[0][1];
    point.pressed=point.held=0;point.released=BK_BUTTON_CONFIRM;
    CHECK(tick(scene,renderer,audio,&sink,point,error));
    BkEndingRecord *r=&s->ending_records.groups[record_group];
    CHECK(r->count==1&&r->actions[0]==choice+12);
    n=0;while((s->common.blocked||s->ending_state.auxiliary.gate!=1)&&n++<2400)
      CHECK(tick(scene,renderer,audio,&sink,pointer(4,920),error));
    printf("record-app menu recorded group%u variant%u\n",record_group,record_variant);fflush(stdout);
    CHECK(n<2400);
    /* Enter through the actual anchor, drag/release the original prompt,
     * then keep real input held through modes1/2/6/4 and the finish wait. */
    unsigned modes=0, clicks=0;int32_t last_mode=-1,last_gate=-1;
    uint32_t previous_buttons=0;
    n=0;while(s->flow.current==0x10&&n++<input_limit) {
      int32_t mode=s->ending_state.ui_controller.auxiliary.mode;
      int32_t gate=s->ending_state.auxiliary.gate;
      if(mode!=last_mode||gate!=last_gate) {
        int32_t active;CHECK(record_probe_active(s->ending,&active,error));
        printf("record-input frame%u gate%d mode%d clip%d progress%.6f count%d\n",n,gate,mode,active,s->ending_state.auxiliary.progress,r->count);fflush(stdout);
        last_mode=mode;last_gate=gate;
      }
      BkInput drag={0};
      if(gate==1) {
        CHECK(record_probe_alternate(s->ending,&drag,error));
        if(n%15==1) { drag.held=BK_BUTTON_CONFIRM;++clicks; }
      } else if(gate==3 && mode==0) {
        drag=(BkInput){.pointer_active=1,
            .pointer_x=s->viewport.x+(float)s->ending_state.points[0][0],
            .pointer_y=s->viewport.y+(float)s->ending_state.points[0][1]};
      } else if(gate==3) {
        drag=(BkInput){.held=BK_BUTTON_CONFIRM,
            .pointer_motion_x=(n%32<16)?1:-1};
      }
      drag.pressed=drag.held&~previous_buttons;
      drag.released=previous_buttons&~drag.held;previous_buttons=drag.held;
      if(gate==3 && mode>=0 && mode<8) modes|=1u<<(unsigned)mode;
      CHECK(tick(scene,renderer,audio,&sink,drag,error));
    }
    if(n>=input_limit)snprintf(error,sizeof(error),"natural input timeout: gate%d mode%d progress%.6f clicks%u modes%x",s->ending_state.auxiliary.gate,s->ending_state.ui_controller.auxiliary.mode,s->ending_state.auxiliary.progress,clicks,modes);
    CHECK(n<input_limit&&s->flow.current==0x50&&s->flow.target==8);
    CHECK((modes&0x57u)==0x57u); /*0,1,2,4,6 all entered without writes.*/
    printf("PASS record-input modes%x clicks%u input_frames%u missing_lane_rejected1\n",modes,clicks,n);
    CHECK(r->count>=2&&r->actions[r->count-1]==21&& !memcmp(r->actions,r->retained[record_variant],40000));
    CHECK(s->ending_unlock_flags[6+record_variant]==1);
    CHECK(expected_file(argv[2],s,1,error));
    CHECK(await_flow(scene,renderer,audio,&sink,8,200,error));
    n=0;while(s->flow.current!=1&&n++<14000) {
      BkInput advance={.pressed=(n%12==0)?BK_BUTTON_CONFIRM:0};
      CHECK(tick(scene,renderer,audio,&sink,advance,error));
    }
    CHECK(n<14000&&bk_unlock_file_read(unlocks,&saved,error)==BK_RESOURCE_OK);
    CHECK(saved.flags[record_group][6+record_variant]==1);
    CHECK(expected_file(argv[2],s,0,error));
  } else {
    CHECK(expected_file(argv[2],s,0,error));
    CHECK(wait_frames(scene,renderer,audio,&sink,64,pointer(4,920),error));
    CHECK(click(scene,renderer,audio,&sink,1084,262,error));
    CHECK(await_flow(scene,renderer,audio,&sink,0x18,200,error));
    CHECK(wait_frames(scene,renderer,audio,&sink,64,pointer(4,920),error));
    CHECK(click(scene,renderer,audio,&sink,92,206+record_group*150,error));
    CHECK(click(scene,renderer,audio,&sink,612,record_variant?738:432,error));
    CHECK(await_flow(scene,renderer,audio,&sink,0x10,200,error));
    CHECK(s->ending_state.frame.phase==8);
    CHECK(await_flow(scene,renderer,audio,&sink,0x18,replay_limit,error));
    CHECK(wait_frames(scene,renderer,audio,&sink,64,pointer(4,920),error));
    CHECK(s->ending_records.groups[record_group].retained[record_variant][s->ending_records.groups[record_group].count-1]==21);
  }
  bk_scene_destroy(scene);scene=NULL;
  BkRenderStats after=bk_renderer_stats(renderer);CHECK(after.live_allocations==baseline.live_allocations&&after.live_bytes==baseline.live_bytes);
  printf("PASS record-app %s group%u variant%u frames%u redraws%u state%016llx PCM%016llx samples%llu nonzero%llu\n",argv[3],record_group,record_variant,frames,redraws,(unsigned long long)state_hash,(unsigned long long)sink.hash,(unsigned long long)sink.submitted*2,(unsigned long long)sink.nonzero);
  result=0;
done:
  bk_scene_destroy(scene);bk_record_file_destroy(records);bk_unlock_file_destroy(unlocks);
  bk_audio_destroy(audio);bk_renderer_destroy(renderer);bk_resources_destroy(store);return result;
}
