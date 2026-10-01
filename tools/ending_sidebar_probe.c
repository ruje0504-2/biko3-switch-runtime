/* Real sidebar input and optional patched assets. Entry/inventory are explicit
 * fixtures; clicks, material changes, rendering and retirement are production. */
#include "ending_record_boundary_fixture.c"

typedef struct { uint64_t submitted, consumed; } SidebarSink;
static int sidebar_submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)pcm; (void)e; ((SidebarSink *)p)->submitted += frames; return 1;
}
static int sidebar_poll(void *p, uint64_t *out, char e[256]) {
  (void)e; SidebarSink *s = p;
  s->consumed = s->submitted < s->consumed + 800 ? s->submitted : s->consumed + 800;
  *out = s->consumed; return 1;
}
static int sidebar_present(BkScene *scene, BkRenderer *r, char e[256]) {
  return bk_renderer_begin(r, e) && bk_scene_draw(scene, &(BkSceneFrame){0}, e) &&
      bk_renderer_end(r, e) && bk_ending_normal_scene_after_present(scene, e);
}
static int sidebar_tick(BkScene *scene, BkRenderer *r, BkAudio *audio,
                         BkInput input, char e[256]) {
  return bk_audio_poll(audio, e) && bk_scene_step(scene, 1. / 60., &input, e) &&
      bk_audio_fill(audio, e) && sidebar_present(scene, r, e);
}
/* Manual-state fixtures check the native adapter against real clip clocks.
 * They do not advance story state or change the cursor/camera speed. */
static int sidebar_motion_checks(EndingNormalScene *s, unsigned *count, char e[256]) {
  const BkInput cases[] = {{.move_x=1}, {.move_y=-1}, {.move_x=.1f},
      {.pointer_active=1,.move_x=1}, {.move_x=1,.held=BK_BUTTON_CAMERA_ADJUST}};
  const unsigned gains[] = {6,6,1,1,1};
  for (unsigned rate = 30; rate <= 120; rate *= 2)
    for (unsigned trial = 0; trial < 5; ++trial) {
      s->input = cases[trial];
      if (stick_action_gain(s) != gains[trial]) return fail(e,"stick gain selection failed");
      s->active_seconds = 1.f/rate;
      float pointer[2]; memcpy(pointer,s->pointer.position,sizeof(pointer));
      if (s->selected_assets || s->tertiary_assets) {
        BkActorPose *pose = scene_primary(s);
        unsigned slot = s->selected_assets ? 6 : 4;
        BkClipTiming t; BkClipPrediction p; BkClipState after;
        if (!bk_actor_pose_select(pose,slot,1,e) ||
            !bk_actor_pose_timing(pose,slot,&t) || !bk_actor_pose_prediction(pose,slot,&p)) return 0;
        float start = (float)(((double)t.start+t.end)*.5);
        if (!bk_actor_pose_set_clock(pose,slot,0,start,e)) return 0;
        float motion[2] = {480.f/rate, -240.f/rate};
        float expected;
        if (s->selected_assets) {
          BkEndingSelectedMotionClip clip = {p.duration,t.start,t.end,start,p.rate,0};
          float amplified[2] = {motion[0]*gains[trial],motion[1]*gains[trial]};
          s->selected_plain_scheduled = 1;
          s->state->retained.auxiliary.word_6ea314 = 0;
          float ignored;
          if (!bk_ending_selected_motion_drag(&clip,1,0,amplified,e) ||
              !selected_drag(s,motion,&ignored,e)) return 0;
          expected = clip.source;
        } else {
          s->state->stage3_state = 3;
          s->state->retained.stage2.word_6a3c20 = 1;
          s->tertiary_controller->action.replay_after = 100000;
          s->tertiary_controller->action.replay_elapsed = 0;
          s->tertiary_controller->motion.direction[0] = 1;
          BkEndingTertiaryMotionState state = s->tertiary_controller->motion;
          BkEndingTertiaryMotionClip clip = {p.duration,t.start,t.end,start,p.rate*gains[trial],0};
          int32_t delta[2] = {(int32_t)motion[0],(int32_t)motion[1]};
          if (!bk_ending_tertiary_motion_drag(&state,s->group,0,s->active_seconds,delta,&clip,e)) return 0;
          BkEndingCall call = {.operation=BK_ENDING_STAGE_476720};
          memcpy(call.input.words+6,delta,sizeof(delta));
          s->input.held |= BK_BUTTON_CONFIRM;
          uint32_t result = 0;
          if (!frame_invoke(s,&call,&result,e)) return 0;
          expected = clip.source;
        }
        if (!bk_actor_pose_state(pose,&after) || after.source != expected)
          return fail(e,"stick drag did not update the actual clip clock at the requested gain");
      } else if (s->assets) {
        BkBomAssets *bom = bk_ending_normal_assets_bom(s->assets);
        const BkBomAssetBinding *b = bk_bom_assets_binding(bom,0);
        if (b && b->reference != BK_MODEL_NONE && b->parent != BK_MODEL_NONE) {
          BkBomManual expected = {.offset={.02f,-.03f}};
          BkNodeReference node;
          BkActorPose *pose = bk_bom_assets_actor(bom,0);
          int32_t delta[2] = {(int32_t)(480.f/rate),(int32_t)(-240.f/rate)};
          if (!bk_actor_pose_node_reference(pose,b->reference,&node,e) ||
              !bk_bom_manual_step(&expected,BK_BOM_MANUAL_FIRST,&node,bk_actor_pose_frame(pose,b->parent),
                  delta[0]*(int32_t)gains[trial],delta[1]*(int32_t)gains[trial],.09f,7.5f,s->normal_controller->flip,e)) return 0;
          s->presentation->manual[0] = (BkBomManual){.offset={.02f,-.03f}};
          s->state->normal_ready = 1;
          s->state->frame.state_721ee0 = 3; s->state->frame.camera_cached = 11;
          BkEndingCall call = {.operation=BK_ENDING_STAGE_4DF6C0};
          memcpy(call.input.words+6,delta,sizeof(delta));
          uint32_t result = 0;
          if (!frame_invoke(s,&call,&result,e) ||
              memcmp(&expected,&s->presentation->manual[0],sizeof(expected)))
            return fail(e,"normal manual drag gain mismatch");
        }
      }
      if (memcmp(pointer,s->pointer.position,sizeof(pointer))) return fail(e,"manual gain moved the menu cursor");
      ++*count;
    }
  return 1;
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"sidebar group%u mode%s variant%u line%d: %s\n",group,mode,variant,__LINE__,e); goto done; } } while (0)
int main(int argc, char **argv) {
  if (argc != 5 && argc != 6) return 2;
  unsigned group = (unsigned)strtoul(argv[2], NULL, 10);
  const char *mode = argv[3];
  unsigned variant = (unsigned)strtoul(argv[4], NULL, 10);
  if (group >= 5 || variant > 1) return 2;
  char e[256] = {0}, path[2048]; int result = 1;
  BkRenderer *r = NULL; BkResourceStore *store = NULL; BkAudio *audio = NULL;
  BkScene *scene = NULL; BkEndingRecords *records = NULL;
  SidebarSink sink = {0}; unsigned clicks = 0, pose_changes = 0, motion_checks = 0;
  CHECK(store = bk_resources_create(e));
  const char *packs[] = {"bk3_00","bk3_01","bk3_02","bk3_03","bk3_04","bk3_05",
      "bk3_06","bk3_07","bk3_08","bk3_09","bk3_10","bk3_11","bk3_12","bk3_13",
      "bk3_14","bk3_15","bk3_16","bk3_18","bk3_19","bk3_20","fambom"};
  for (unsigned i = 0; i < sizeof(packs)/sizeof(*packs); ++i) {
    snprintf(path,sizeof(path),"%s/%s.pp",argv[1],packs[i]);
    CHECK(bk_resources_mount(store,packs[i],path,e));
  }
  CHECK(bk_resources_mount_directory(store,"faces",argv[1],20480,e));
  CHECK(bk_resources_mount_directory(store,"fonts",argv[1],16*1024*1024,e));
  if (argc == 6) CHECK(bk_resources_load_patch(store,argv[5],e));
  CHECK(r = bk_renderer_create(160,120,stdout,e));
  BkRenderStats baseline = bk_renderer_stats(r);
  BkAudioSink output = {&sink,48000,480,1920,sidebar_submit,sidebar_poll};
  CHECK(audio = bk_audio_create(&output,e));
  BkSceneServices services = {store,r,stdout,audio,NULL,NULL};
  uint8_t unlocked[5][8] = {{0}};
  if (!strcmp(mode,"normal")) {
    CHECK(records = calloc(1,sizeof(*records)));
    scene = bk_ending_normal_scene_create_story(&services,group,0,records,unlocked,NULL,e);
  } else if (!strcmp(mode,"third")) {
    CHECK(records = calloc(1,sizeof(*records)));
    scene = bk_ending_normal_scene_create_story(&services,group,1,records,unlocked,NULL,e);
  } else if (!strcmp(mode,"secondary")) {
    scene = bk_ending_secondary_scene_create_gallery(&services,group,variant,unlocked,NULL,e);
  } else if (!strcmp(mode,"selected")) {
    scene = bk_ending_selected_scene_create_gallery(&services,group,variant,variant ? 5 : 2,unlocked,NULL,e);
  } else if (!strcmp(mode,"auxiliary")) {
    CHECK(records = calloc(1,sizeof(*records)));
    scene = create_entry(&services,group,variant,0x18,4,records,unlocked,NULL,e);
  } else goto done;
  CHECK(scene);
  EndingNormalScene *s = bk_scene_custom_context(scene);
  CHECK(s && s->state);
  s->inventory[1] = s->inventory[2] = 1;
  s->state->control.toggles[4] = s->state->control.toggles[6] = 1;
  CHECK(bk_audio_fill(audio,e) && sidebar_present(scene,r,e));
  if (s->selected_assets)
    for (unsigned frame = 0; s->state->auxiliary.gate == 4 && frame < 1800; ++frame)
      CHECK(sidebar_tick(scene,r,audio,(BkInput){0},e));
  CHECK(!s->selected_assets || s->state->auxiliary.gate == 1);
  BkInput open = {.pointer_active=1,
      .pointer_x=s->viewport.x+s->viewport.width-2,
      .pointer_y=s->viewport.y+s->viewport.height*.55f};
  for (unsigned frame = 0; frame < 160; ++frame)
    CHECK(sidebar_tick(scene,r,audio,open,e));
  CHECK(s->state->open);
  for (unsigned cycle = 0; cycle < 8; ++cycle)
    for (unsigned item = 0; item < 2; ++item) {
      BkEndingControlRect bounds[BK_ENDING_CONTROL_RECTS];
      CHECK(bk_ending_ui_control_rects(&s->ui,bounds));
      unsigned row = 6 + item, value = item ? 5 : 3;
      BkInput click = {.pointer_active=1,
          .pointer_x=s->viewport.x+bounds[row].x+bounds[row].width*.5f,
          .pointer_y=s->viewport.y+bounds[row].y+bounds[row].height*.5f};
      CHECK(sidebar_tick(scene,r,audio,click,e));
      uint8_t before = s->state->control.toggles[value];
      click.held = click.pressed = BK_BUTTON_CONFIRM;
      CHECK(sidebar_tick(scene,r,audio,click,e));
      if (s->state->control.toggles[value] == before)
        snprintf(e,sizeof(e),"item%u not toggled, phase%d enabled%u pointer %.2f %.2f rect %.2f %.2f %.2f %.2f",item,s->state->frame.phase,s->state->control.toggles[value+1],s->pointer.position[0],s->pointer.position[1],bounds[row].x,bounds[row].y,bounds[row].width,bounds[row].height);
      CHECK(s->state->control.toggles[value] != before);
      click.held = click.pressed = 0; click.released = BK_BUTTON_CONFIRM;
      CHECK(sidebar_tick(scene,r,audio,click,e));
      click.released = 0;
      for (unsigned frame = 0; frame < 3; ++frame)
        CHECK(sidebar_tick(scene,r,audio,click,e));
      ++clicks;
    }
  for (unsigned cycle = 0; cycle < 8; ++cycle)
    for (unsigned row = 0; row < 11; ++row) {
      BkEndingControlRect bounds[BK_ENDING_CONTROL_RECTS];
      CHECK(bk_ending_ui_control_rects(&s->ui,bounds));
      BkInput click = {.pointer_active=1,
          .pointer_x=s->viewport.x+bounds[row].x+bounds[row].width*.5f,
          .pointer_y=s->viewport.y+bounds[row].y+bounds[row].height*.5f};
      CHECK(sidebar_tick(scene,r,audio,click,e));
      click.held = click.pressed = BK_BUTTON_CONFIRM;
      int32_t old_mode = s->state->frame.auxiliary_mode;
      CHECK(sidebar_tick(scene,r,audio,click,e));
      if (row == 2 && old_mode != s->state->frame.auxiliary_mode) ++pose_changes;
      click.held = click.pressed = 0; click.released = BK_BUTTON_CONFIRM;
      CHECK(sidebar_tick(scene,r,audio,click,e));
      click.released = 0;
      for (unsigned frame = 0; frame < 30; ++frame)
        CHECK(sidebar_tick(scene,r,audio,click,e));
      ++clicks;
    }
  if (s->selected_assets) {
    for (unsigned change = 0; change < 2; ++change) {
      BkInput input;
      /*Enable both actual item materials before dragging to the next pose.*/
      BkInput edge = {.pointer_active=1,.pointer_x=s->viewport.x+s->viewport.width-2,
          .pointer_y=s->viewport.y+s->viewport.height*.55f};
      for (unsigned frame = 0; frame < 128; ++frame)
        CHECK(sidebar_tick(scene,r,audio,edge,e));
      for (unsigned item = 0; item < 2; ++item) {
        unsigned value = item ? 5 : 3;
        if (!s->state->control.toggles[value]) continue;
        BkEndingControlRect bounds[BK_ENDING_CONTROL_RECTS];
        CHECK(bk_ending_ui_control_rects(&s->ui,bounds));
        unsigned row = 6+item;
        BkInput enable = {.pointer_active=1,
            .pointer_x=s->viewport.x+bounds[row].x+bounds[row].width*.5f,
            .pointer_y=s->viewport.y+bounds[row].y+bounds[row].height*.5f};
        CHECK(sidebar_tick(scene,r,audio,enable,e));
        enable.held = enable.pressed = BK_BUTTON_CONFIRM;
        CHECK(sidebar_tick(scene,r,audio,enable,e));
        CHECK(!s->state->control.toggles[value]);
        enable.held = enable.pressed = 0; enable.released = BK_BUTTON_CONFIRM;
        CHECK(sidebar_tick(scene,r,audio,enable,e));
        ++clicks;
      }
      BkInput away = {.pointer_active=1,.pointer_x=s->viewport.x+4,
          .pointer_y=s->viewport.y+s->viewport.height-4};
      for (unsigned frame = 0; frame < 128; ++frame)
        CHECK(sidebar_tick(scene,r,audio,away,e));
      for (unsigned frame = 0; (s->state->auxiliary.gate != 1 || s->common->blocked) && frame < 2400; ++frame)
        CHECK(sidebar_tick(scene,r,audio,(BkInput){0},e));
      CHECK(s->state->auxiliary.gate == 1 && !s->common->blocked);
      CHECK(record_probe_point(scene,&input,e));
      for (unsigned frame = 0; frame < 48; ++frame)
        CHECK(sidebar_tick(scene,r,audio,input,e));
      input.held = input.pressed = BK_BUTTON_CONFIRM;
      CHECK(sidebar_tick(scene,r,audio,input,e));
      if (s->state->auxiliary.gate != 3)
        snprintf(e,sizeof(e),"pose menu gate%d mode%d open%u target%d view%d",s->state->auxiliary.gate,s->state->ui_controller.auxiliary.mode,s->state->open,s->state->frame.camera_cached,s->state->control.mode_721ec4);
      CHECK(s->state->auxiliary.gate == 3 && s->state->ui_controller.auxiliary.mode == 3);
      float from[2] = {input.pointer_x,input.pointer_y};
      float to[2] = {s->viewport.x+s->state->points[0][0],s->viewport.y+s->state->points[0][1]};
      input.pressed = 0;
      for (unsigned frame = 1; frame <= 48; ++frame) {
        input.pointer_x = from[0]+(to[0]-from[0])*frame/48.f;
        input.pointer_y = from[1]+(to[1]-from[1])*frame/48.f;
        CHECK(sidebar_tick(scene,r,audio,input,e));
      }
      input.held = input.pressed = 0; input.released = BK_BUTTON_CONFIRM;
      CHECK(sidebar_tick(scene,r,audio,input,e));
      for (unsigned frame = 0; (s->state->auxiliary.gate != 1 || s->common->blocked) && frame < 2400; ++frame)
        CHECK(sidebar_tick(scene,r,audio,(BkInput){0},e));
      CHECK(s->state->auxiliary.gate == 1 && !s->common->blocked);
      ++pose_changes;
    }
    uint32_t held = 0;
    for (unsigned frame = 0; frame < 1200; ++frame) {
      BkInput input = {0};
      int32_t gate = s->state->auxiliary.gate, action_mode = s->state->ui_controller.auxiliary.mode;
      if (gate == 1) {
        CHECK(record_probe_alternate(scene,&input,e));
        if (frame%15 == 1) input.held = BK_BUTTON_CONFIRM;
      } else if (gate == 3 && action_mode == 0) {
        input = (BkInput){.pointer_active=1,
            .pointer_x=s->viewport.x+s->state->points[0][0],
            .pointer_y=s->viewport.y+s->state->points[0][1]};
      } else if (gate == 3) {
        input.held = BK_BUTTON_CONFIRM;
        input.move_x = frame%32 < 16 ? 1 : -1;
      }
      input.pressed = input.held & ~held; input.released = held & ~input.held; held = input.held;
      CHECK(sidebar_tick(scene,r,audio,input,e));
    }
  }
  CHECK(sidebar_motion_checks(s,&motion_checks,e));
  CHECK(bk_ending_normal_scene_stop(scene,e));
  bk_scene_destroy(scene); scene = NULL;
  BkRenderStats final = bk_renderer_stats(r);
  CHECK(final.live_allocations == baseline.live_allocations && final.live_bytes == baseline.live_bytes);
  printf("PASS sidebar group%u mode%s variant%u patched%d clicks%u poses%u motion%u GPU baseline\n",group,mode,variant,argc==6,clicks,pose_changes,motion_checks);
  result = 0;
done:
  bk_scene_destroy(scene); free(records); bk_audio_destroy(audio);
  bk_renderer_destroy(r); bk_resources_destroy(store); return result;
}
