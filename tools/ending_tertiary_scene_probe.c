#include "scene/ending_tertiary_controller.h"
#include "scene/ending_tertiary_presentation.h"
#include "scene/ending_target.h"
#include "scene/ending_ui_hints.h"
#include "game/ending_normal.h"
#include "core/input.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Real Japanese assets/PCM and input-driven476720->47811c->479137 frames.
 * Initial loader scalars are an explicit fixture. No forced clip, source,
 * audio completion, menu result or progress change after entry. CPU forest
 * draw publishes actual caches; GPU/video/outer loader are NOT exercised. */
#define CHECK(x) do { if (!(x)) { \
  fprintf(stderr,"tertiary scene line %d: %s: %s\n",__LINE__,#x,e); goto done; \
} } while (0)
typedef struct { uint64_t submitted, consumed, hash; } Sink;
typedef struct { uint32_t now, calls; BkInput input; } Input;
static uint64_t digest(uint64_t h, const void *data, size_t size) {
  const uint8_t *p = data;
  for (size_t i=0;i<size;++i) h=(h^p[i])*UINT64_C(1099511628211);
  return h;
}
static int submit(void *p,const int16_t *pcm,size_t frames,char e[256]) {
  (void)e; Sink *s=p;
  s->hash=digest(s->hash,pcm,frames*2*sizeof(*pcm)); s->submitted+=frames;
  return 1;
}
static int poll(void *p,uint64_t *frames,char e[256]) {
  (void)e; *frames=((Sink *)p)->consumed; return 1;
}
static int clock_read(void *p,uint32_t *ms,char e[256]) {
  (void)e; *ms=((Input *)p)->now; return 1;
}
static int key(void *p,unsigned code,unsigned mode,uint32_t *out,char e[256]) {
  Input *i=p;
  if (mode>3) { snprintf(e,256,"unexpected key mode %u",mode); return 0; }
  uint32_t mask=code==0?BK_BUTTON_CONFIRM:code==1?BK_BUTTON_BACK:0;
  *out=mode==0?!((i->input.held|i->input.pressed|i->input.released)&mask):
       mode==1?!!(i->input.pressed&mask):mode==2?!!(i->input.held&mask):
                                                    !!(i->input.released&mask);
  ++i->calls; return 1;
}
static int mount_archives(BkResourceStore *store,const char *data,char e[256]) {
  DIR *directory=opendir(data);
  if (!directory) { snprintf(e,256,"cannot open supplied Data directory"); return 0; }
  struct dirent *entry; int ok=1; unsigned count=0;
  while ((entry=readdir(directory))) {
    size_t n=strlen(entry->d_name);
    if (n<4||strcmp(entry->d_name+n-3,".pp")) continue;
    char name[128],path[2048];
    if (n-3>=sizeof(name)||snprintf(path,sizeof(path),"%s/%s",data,entry->d_name)>=(int)sizeof(path)) {
      snprintf(e,256,"archive name/path too long"); ok=0; break;
    }
    memcpy(name,entry->d_name,n-3); name[n-3]=0;
    if (!bk_resources_mount(store,name,path,e)) { ok=0; break; }
    ++count;
  }
  closedir(directory);
  return ok&&count;
}
static uint64_t pose_hash(BkActorPose *pose) {
  uint64_t h=UINT64_C(14695981039346656037);
  const BkModel *model=bk_actor_pose_model(pose);
  if (!model) return 0;
  for (uint32_t i=0;i<model->frame_count;++i) {
    h=digest(h,bk_actor_pose_local(pose,i),64);
    h=digest(h,bk_actor_pose_frame(pose,i),64);
    h=digest(h,bk_actor_pose_parent_world(pose,i),64);
  }
  return h;
}
static int geometry(BkEndingTertiaryAssets *assets,const BkMenuCamera *camera,
                      unsigned width,unsigned height,float worlds[39][16],
                      uint8_t present[39],float projection[16],float viewport[16],
                      BkEndingUiPickBindings *out,char e[256]) {
  BkActorForest *forest=bk_ending_tertiary_assets_forest(assets);
  if (!bk_camera_projection(projection,&(BkCameraLens){camera->fov,.75f,.5f,126384})) {
    snprintf(e,256,"invalid actual camera projection"); return 0;
  }
  memset(viewport,0,64);
  viewport[0]=viewport[12]=(float)width*.5f;
  viewport[5]=viewport[13]=(float)height*.5f;
  viewport[10]=viewport[15]=1;
  BkActorPose *primary=bk_ending_tertiary_assets_pose(assets,0);
  for (unsigned i=0;i<39;++i) {
    uint32_t node=bk_ending_tertiary_assets_node(assets,i);
    present[i]=node!=BK_MODEL_NONE;
    if (!present[i]) continue;
    const float *world=bk_actor_pose_frame(primary,node);
    if (!world) { snprintf(e,256,"missing held target node%u",i); return 0; }
    memcpy(worlds[i],world,64);
  }
  *out=(BkEndingUiPickBindings){worlds,present,39,camera->pose.position,
      bk_actor_forest_view(forest),projection,viewport,0};
  return 1;
}
/* Gesture planning only: private copies prevent hit search from mutating the
 * real controller, target table, selection or ring sprite. */
static int hit_at(const BkEndingTertiaryControllerScene *scene,
                     const BkEndingState *state,int wanted,int x,int y,char e[256]) {
  BkEndingFrameState frame=state->frame;
  BkEndingControlState control=state->control;
  BkEndingAuxiliaryState auxiliary=state->auxiliary;
  BkClipState clip; BkNodeReference camera;
  int32_t targets[39][2],kind=*scene->action_kind,column=*scene->action_column;
  memcpy(targets,state->targets,sizeof(targets));
  BkEndingUiSprite ring=scene->ui->sprites[50];
  if (!bk_actor_pose_state(bk_ending_tertiary_assets_pose(scene->assets,0),&clip)||
      !bk_actor_forest_anchor_reference(bk_ending_tertiary_assets_forest(scene->assets),1,&camera,e)) return -1;
  BkEndingTargetBindings b={&frame,&control,&auxiliary,&clip.slot,
      bk_ending_tertiary_assets_config(scene->assets)->actions,targets,&kind,&column,
      &ring,camera.local,scene->geometry};
  int hit;
  if (!bk_ending_target_step(&b,(float[2]){(float)x,(float)y},&hit,e)) return -1;
  return hit==1&&frame.camera_cached==wanted;
}
static int gesture(const BkEndingTertiaryControllerScene *scene,
                      const BkEndingState *state,int wanted,int32_t point[2],char e[256]) {
  int32_t center[2];
  if (wanted<0||wanted>=39||!bk_ending_ui_project_target(scene->geometry,(unsigned)wanted,center,e)) return -1;
  /*47cb41 rejects an off-screen CENTER even when4dac12 can hit the
   * visible edge of its circle. Continue the camera gesture in that case.*/
  if (center[0]<0||center[1]<0||(uint32_t)center[0]>scene->width||
      (uint32_t)center[1]>scene->height) goto missed;
  for (int radius=0;radius<=80;radius+=2)
    for (int dy=-radius;dy<=radius;dy+=2)
      for (int dx=-radius;dx<=radius;dx+=2) {
        if (abs(dx)!=radius&&abs(dy)!=radius) continue;
        int64_t x=(int64_t)center[0]+dx,y=(int64_t)center[1]+dy;
        if (x<0||y<0||x>=scene->width||y>=scene->height) continue;
        int hit=hit_at(scene,state,wanted,(int)x,(int)y,e);
        if (hit<0) return -1;
        if (hit) { point[0]=(int32_t)x;point[1]=(int32_t)y;return 1; }
      }
missed:
  snprintf(e,256,"no on-screen target%d at projected%d,%d in%ux%u",wanted,
      center[0],center[1],scene->width,scene->height);
  return 0;
}
int main(int argc,char **argv) {
  if (argc<2||argc>3) return 2;
  int first=argc==3?atoi(argv[2]):0,last=argc==3?first+1:5;
  if (first<0||last>5) return 2;
  char e[256]={0}; int rc=1;
  BkResourceStore *store=bk_resources_create(e);
  BkEndingTertiaryAssets *assets=NULL;
  BkEndingBackgroundAssets *background=NULL;
  BkAudio *mixer=NULL; BkEndingAudio *audio=NULL;
  BkEndingState *state=calloc(1,sizeof(*state));
  BkEndingRecords *records=calloc(1,sizeof(*records));
  uint64_t all_pcm=UINT64_C(14695981039346656037),all_pose=all_pcm;
  unsigned entries=0,total=0,menus=0,drags=0,states=0,retained_checks=0,frozen=0,zooms=0;
  BkEndingTertiaryControllerRetained retained;
  bk_ending_tertiary_controller_initialize(&retained);
  BkEndingVoiceEnvelope envelope={0};
  CHECK(store&&state&&records&&mount_archives(store,argv[1],e));
  for (int group=first;group<last;++group)
    for (unsigned profile=0;profile<2;++profile) {
      memset(state,0,sizeof(*state));
      uint32_t random=1001+(unsigned)group*2+profile,clocks[4]={100,100,100,100};
      BkMenuCamera camera; BkEndingCameraPresets presets;
      BkEndingCameraTransitions transitions={0};
      CHECK(bk_menu_camera_dialogue(&camera));
      BkEndingTertiaryControllerRetained held=retained;
      if (profile) {
        background=bk_ending_background_assets_create(store,bk_ending_normal_background((unsigned)group,0),e);
        CHECK(background);
        assets=bk_ending_tertiary_assets_create_reloaded(store,(unsigned)group,1,
            background,clocks,&random,&camera,&presets,e);
      } else assets=bk_ending_tertiary_assets_create(store,(unsigned)group,1,
                                                   clocks,&random,&camera,&presets,e);
      CHECK(assets&&bk_ending_tertiary_assets_load_background(assets,store,e));
      CHECK(!memcmp(&held,&retained,sizeof(held))); ++retained_checks;
      bk_ending_background_assets_destroy(background); background=NULL;
      const BkEndingTertiaryConfig *config=bk_ending_tertiary_assets_config(assets);
      BkActorForest *forest=bk_ending_tertiary_assets_forest(assets);
      BkActorPose *primary=bk_ending_tertiary_assets_pose(assets,0);
      unsigned width=profile?640:960,height=profile?480:720;
      float scale=(float)((double)width/1280.0);
      BkEndingUi ui={0}; BkEndingStageUi stage={0}; uint8_t flags[6]={0};
      CHECK(bk_ending_ui_initialize(&ui,width,flags,&state->gauge_y,e));
      CHECK(bk_ending_stage_ui_initialize(&ui,&stage,BK_ENDING_UI_THIRD,
                                         (unsigned)group,1,width,e));
      state->frame=(BkEndingFrameState){.phase=3,.group=(uint8_t)group,
          .camera_mode=5,.camera_cached=-1};
      state->stage3_state=4;
      state->control.variant=5;
      memcpy(state->frame.camera_table,config->camera_table,sizeof(config->camera_table));
      for (unsigned i=0;i<3;++i)
        memcpy(state->control.targets[i],bk_ending_tertiary_assets_target(assets,i),12);
      memcpy(state->frame.camera_values,state->control.targets[0],12);
      unsigned missing=bk_ending_tertiary_assets_missing_visible(assets);
      state->control.toggles[1]=missing==3;state->control.toggles[2]=missing!=3;
      state->control.toggles[3]=group!=1;state->control.toggles[4]=group==1;
      state->control.toggles[5]=state->control.toggles[7]=1;
      state->auxiliary.variant=1;
      state->auxiliary.expression_a=config->expression_a;
      state->auxiliary.expression_b=config->expression_b;
      state->selected=-1;
      state->retained.stage2.value_54ccd0=1;
      state->retained.stage2.word_54ccc8=-1;
      uint32_t follow=bk_actor_forest_node(forest,0,bk_ending_tertiary_assets_follow(assets));
      int32_t face_mode=0,eye_lower=0,kind=-1,column=-1,voice_volume=-900,effect_volume=-700;
      int8_t previous=8,finish_setting=0;
      Sink sink={.hash=UINT64_C(14695981039346656037)};
      BkAudioSink output={&sink,48000,480,1920,submit,poll};
      CHECK(mixer=bk_audio_create(&output,e));
      CHECK(audio=bk_ending_audio_create_entry(store,mixer,0,(unsigned)group,1,-1600,e));
      Input input={.now=1000};
      float worlds[39][16],projection[16],viewport[16];uint8_t present[39];
      BkEndingUiPickBindings geometry_state={0};
      BkEndingRetainedStage2 *r=&state->retained.stage2;
      BkEndingTertiaryActionBindings bindings={
          {&state->frame,&state->control,&state->auxiliary,&camera,records,
           &state->stage3_state,&face_mode,&r->word_6a3c20,r->words_6afcfc,
           &r->word_6afd08,state->unavailable,&state->ui_controller.hints.movement_ready,
           &r->byte_6afd18,&r->word_6a3c24,&r->word_54ccc8,&r->value_54ccd0,
           &state->open,&previous,&finish_setting,config->actions+5,state->targets,
           bk_ending_tertiary_initial_targets(),&voice_volume,&effect_volume,&random},
          &state->gauge_y,&scale,state->choices};
      BkEndingTertiaryControllerScene controller={assets,audio,&retained,&transitions,
          &presets,&ui,&geometry_state,width,height,state->speech_names,state->targets,
          state->points,&kind,&column,&follow,&state->selected,&state->next_mode,&input,key};
      int32_t disabled[5]={0};
      unsigned bom_count=bk_bom_dual_assets_count(bk_ending_tertiary_assets_bom(assets));
      BkEndingTertiaryPresentationScene display={assets,audio,&envelope,&random,
          disabled,bom_count,&input,clock_read};
      CHECK(bom_count==(group==2?5u:0u));
      CHECK(bk_actor_forest_camera_publish(forest,e));
      unsigned frame=0,operations=0,hold_pcm=0,drag_frames=0,zoom_frames=0;
      for (;frame<20000&&!state->frame.curtain_wanted;++frame) {
        input.now=1000+(uint32_t)((uint64_t)frame*1000/60);
        input.input=(BkInput){0};
        if (state->stage3_state==4&&r->byte_6afd18==2&&hold_pcm<8) {
          ++hold_pcm;++frozen;
        } else { uint64_t next=sink.consumed+800;sink.consumed=next<sink.submitted?next:sink.submitted; }
        CHECK(bk_audio_poll(mixer,e));
        CHECK(geometry(assets,&camera,width,height,worlds,present,projection,viewport,&geometry_state,e));
        geometry_state.ring_width=ui.sprites[50].rect[2];
        BkClipState before;
        CHECK(bk_actor_pose_state(primary,&before));
        if (state->stage3_state>=0&&state->stage3_state<6) states|=1u<<(unsigned)state->stage3_state;
        BkEndingFrameInput captured={0};int32_t point[2]={0,0},motion[2]={0,0};
        float camera_motion[2]={0,0};unsigned camera_buttons=0;
        int playing1;CHECK(bk_audio_playing(mixer,1,&playing1));
        if (frame==1) input.input.pressed=input.input.held=BK_BUTTON_CONFIRM;
        if (state->stage3_state==1&&!playing1&&(before.slot==1||before.slot==4)) {
          int wanted=before.slot==1?bk_ending_tertiary_initial_targets()[group]:
              operations<2?config->actions[5]:!state->unavailable[0]?config->actions[10]:
              !state->unavailable[1]?config->actions[15]:6;
          int found=gesture(&controller,state,wanted,point,e);
          CHECK(found>=0);
          if (found) input.input.pressed=input.input.held=BK_BUTTON_CONFIRM;
          else {
            /* Some authored close-ups exclude the initial knee target.
             * Use the original right-drag distance operation, including the
             * parent's physical-release state, before retrying the hit. */
            CHECK(state->frame.camera_mode==0&&camera.radius<120&&zoom_frames<120);
            camera_motion[0]=60;camera_buttons=2;
            input.input.held=BK_BUTTON_BACK;++zoom_frames;++zooms;
            e[0]=0;
          }
        } else if (state->stage3_state==3) {
          memcpy(point,state->points[0],sizeof(point));
          if (before.slot==7||before.slot==9) {
            motion[0]=60;motion[1]=-40;++drag_frames;++drags;
            input.input.held=BK_BUTTON_CONFIRM;
            if (state->ui_controller.hints.movement_ready) {
              input.input.held=0;input.input.released=BK_BUTTON_CONFIRM;++operations;++menus;
            }
          } else { input.input.released=BK_BUTTON_CONFIRM;++operations;++menus; }
        }
        memcpy(captured.words+9,point,sizeof(point));memcpy(captured.words+6,motion,sizeof(motion));
        CHECK(bk_ending_tertiary_controller_scene_step(&controller,&bindings,&captured,1.f/60,e));
        CHECK(bk_ending_tertiary_presentation_scene_step(&display,state,&face_mode,&eye_lower,1.f/60,e));
        uint32_t tracks[]={bk_ending_tertiary_assets_registry(assets,3),bk_ending_tertiary_assets_registry(assets,4)};
        float offset[3];memcpy(offset,state->frame.camera_values,12);
        if (state->frame.camera_mode==0||state->frame.camera_mode==4||state->frame.camera_mode==2) {
          BkEndingCameraKind mode=state->frame.camera_mode==4?BK_ENDING_CAMERA_FIXED:
                                 state->frame.camera_mode==2?BK_ENDING_CAMERA_AUTO:BK_ENDING_CAMERA_ORBIT;
          CHECK(bk_ending_camera_assets_step(bk_ending_tertiary_assets_cameras(assets),forest,
              tracks,&camera,mode,offset,camera_motion,camera_buttons,follow,1.f/60,e));
        } else CHECK(state->frame.camera_mode==5);
        const BkFrameVisit *walk;uint32_t count;
        CHECK(bk_actor_forest_draw(forest,bk_ending_tertiary_assets_root(assets,5),&walk,&count,e));
        BkClipState after;CHECK(bk_actor_pose_state(primary,&after));
        BkEndingUiHintsBindings hints={.frame=&state->frame,.control=&state->control,
            .auxiliary=&state->auxiliary,.stage3_state=&state->stage3_state,
            .active_clip=&after.slot,.targets=state->targets,.target_count=39,
            .choices=state->choices,.points=state->points,.alternate=state->alternate,
            .gauge_y=&state->gauge_y};
        BkEndingUiFrame draws={0};
        CHECK(bk_ending_ui_hints_start(&state->ui_controller.hints,scale,e));
        CHECK(bk_ending_ui_hints(&ui,&stage,&state->ui_controller.hints,&hints,
            (float[2]){(float)point[0],(float)point[1]},
            (float[2]){(float)motion[0],(float)motion[1]},scale,1.f/60,&draws,e));
        if (!(frame%32)) {
          uint64_t h=pose_hash(primary);all_pose=digest(all_pose,&h,sizeof(h));
          all_pose=digest(all_pose,disabled,bom_count*sizeof(*disabled));
          for (unsigned role=1;role<3&&group==2;++role) {
            h=pose_hash(bk_ending_tertiary_assets_pose(assets,role));
            all_pose=digest(all_pose,&h,sizeof(h));
          }
        }
        CHECK(bk_audio_fill(mixer,e));++total;
        if (frame&&frame%2000==0) {
          printf("progress group=%d profile=%u frame=%u state=%d sub=%u clip=%d progress=%g menus=%u drag=%u\n",
              group,profile,frame,state->stage3_state,r->byte_6afd18,after.slot,
              state->auxiliary.progress,operations,drag_frames);fflush(stdout);
        }
      }
      CHECK(frame<20000&&operations>=4&&state->frame.transition_action==7);
      CHECK(state->auxiliary.progress>=.39f&&drag_frames&&hold_pcm==8);
      CHECK(records->groups[group].count>=4);
      all_pcm=digest(all_pcm,&sink.hash,sizeof(sink.hash));++entries;
      printf("profile group=%d retained_background=%u frames=%u menus=%u drag=%u zoom=%u progress=%g records=%d\n",
          group,profile,frame,operations,drag_frames,zoom_frames,state->auxiliary.progress,records->groups[group].count);
      fflush(stdout);
      bk_ending_audio_destroy(audio);audio=NULL;bk_audio_destroy(mixer);mixer=NULL;
      bk_ending_tertiary_assets_destroy(assets);assets=NULL;
    }
  /* Idle input may pass through state2 (waiting for physical release).
   * Require opening/initial/active/action/finish, permitting that sixth state.*/
  e[0]=0;
  if (entries!=(unsigned)(last-first)*2||(states&59u)!=59u||(states&~63u)||retained_checks!=entries) {
    snprintf(e,sizeof(e),"entries=%u states=%u retained=%u",entries,states,retained_checks);
    CHECK(0);
  }
  printf("PASS tertiary scene entries=%u frames=%u menus=%u drag=%u states=%u retained=%u frozen_pcm=%u zoom=%u pose=%016llx pcm=%016llx\n",
      entries,total,menus,drags,states,retained_checks,frozen,zooms,
      (unsigned long long)all_pose,(unsigned long long)all_pcm);
  rc=0;
done:
  bk_ending_audio_destroy(audio);bk_audio_destroy(mixer);
  bk_ending_tertiary_assets_destroy(assets);bk_ending_background_assets_destroy(background);
  bk_resources_destroy(store);free(records);free(state);return rc;
}
