/* Actual gallery action8 -> flow48 -> gallery. Saved unlocks and injected
 * clocks are explicit fixtures. Real Japanese actors, camera, UI, PCM,
 * movie and capture files; no runtime cheat API or substituted loader. */
#include "../runtime/app/play_session.c"
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "special app line%d (%s): %s\n", \
    __LINE__, #x, error); goto done; } } while (0)
typedef struct {
  uint64_t submitted, consumed, hash, nonzero;
} Sink;
static unsigned frames, redraws, entries, returns, clock_calls[3], photos;
static uint32_t fixture_ms = 100000;
static int fail_movie_clock;
static int clock_fixture(void *p, int timer, uint32_t *out, char e[256]) {
  (void)p; (void)e; if (timer < 0 || timer > 2) return 0;
  ++clock_calls[timer];
  if (timer==2 && fail_movie_clock) { --fail_movie_clock; snprintf(e,256,"injected movie clock failure"); return 0; }
  *out = timer == 2 ? fixture_ms - 100000 : fixture_ms; return 1;
}
static int photo_clock_fixture(void *p, BkCaptureTime *out, char e[256]) {
  (void)p; (void)e; *out = (BkCaptureTime){2026,9,30,13,0,0,++photos}; return 1;
}
static uint64_t state_hash = UINT64_C(14695981039346656037);
static uint64_t hash_bytes(uint64_t h, const void *bytes, size_t n) {
  const uint8_t *p = bytes;
  for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int submit(void *p, const int16_t *pcm, size_t n, char e[256]) {
  (void)e;
  Sink *s = p;
  s->hash = hash_bytes(s->hash, pcm, n * 4);
  for (size_t i = 0; i < n * 2; ++i) s->nonzero += pcm[i] != 0;
  s->submitted += n;
  return 1;
}
static int poll(void *p, uint64_t *n, char e[256]) {
  (void)e;
  *n = ((Sink *)p)->consumed;
  return 1;
}
static int present(BkScene *scene, BkRenderer *r, BkAudio *a, char e[256]) {
  return bk_audio_fill(a, e) && bk_renderer_begin(r, e) &&
      bk_scene_draw(scene, &(BkSceneFrame){0}, e) && bk_renderer_end(r, e) &&
      bk_play_session_after_present(scene, e);
}
static int again(BkScene *scene, BkRenderer *r, BkAudio *a, char e[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  uint8_t before[200 * 120 * 4], after[sizeof(before)];
  uint32_t random = s->game_state.random;
  double clock = s->elapsed;
  BkEndingState state = s->ending_state;
  BkEndingProcess process = s->ending_process;
  BkSpecialProcess special = s->special_process;
  BkMenuCamera camera = s->menu_camera;
  unsigned calls[3]; memcpy(calls, clock_calls, sizeof calls);
  unsigned before_photos = photos;
  if (!bk_renderer_readback(r, before, sizeof(before), e) ||
      !present(scene, r, a, e) ||
      !bk_renderer_readback(r, after, sizeof(after), e)) return 0;
  if (memcmp(before, after, sizeof(before)) || s->elapsed != clock ||
      s->game_state.random != random || memcmp(&state, &s->ending_state, sizeof(state)) ||
      memcmp(&process, &s->ending_process, sizeof(process)) ||
      memcmp(&special, &s->special_process, sizeof special) ||
      memcmp(&camera, &s->menu_camera, sizeof camera) || photos != before_photos ||
      memcmp(calls, clock_calls, sizeof calls)) {
    snprintf(e, 256, "redraw changed pixels or live process state"); return 0;
  }
  ++redraws;
  return 1;
}
static int tick(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink,
                 BkInput in, char e[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  fixture_ms += 50;
  sink->consumed += 800;
  if (sink->consumed > sink->submitted) sink->consumed = sink->submitted;
  if (!bk_audio_poll(a, e) ||
      !bk_play_session_step_at(scene, 1. / 60., s->elapsed + .05, &in, e) ||
      !present(scene, r, a, e)) return 0;
  int32_t values[] = {s->flow.current, s->flow.previous, s->flow.target,
      s->ending_state.frame.phase, s->ending_state.frame.group,
      s->ending_state.frame.state_721ee0, s->ending_state.frame.transition_action,
      s->ending_state.control.variant, s->common.curtain.stage, s->common.blocked};
  state_hash = hash_bytes(state_hash, values, sizeof(values));
  state_hash = hash_bytes(state_hash, &s->special_process, sizeof(s->special_process));
  state_hash = hash_bytes(state_hash, &s->menu_camera, sizeof(s->menu_camera));
  ++frames;
  if ((s->front && (s->shown == 0x18 || s->shown == 0x48) && s->flow.current == 0x50) || s->retire_ending)
    return again(scene, r, a, e);
  return 1;
}
static BkInput pointer(float x, float y) {
  /* 200x120 output, centered160x120 content. Coordinates are authored1280x960. */
  return (BkInput){.pointer_active = 1, .pointer_x = 20 + x * .125f,
      .pointer_y = y * .125f, .pointer_motion_x = .1f};
}
static int wait_frames(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink,
                         unsigned n, BkInput in, char e[256]) {
  for (unsigned i = 0; i < n; ++i) if (!tick(scene, r, a, sink, in, e)) return 0;
  return 1;
}
static int click(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink,
                   float x, float y, char e[256]) {
  BkInput in = pointer(x, y);
  if (!wait_frames(scene, r, a, sink, 2, in, e)) return 0;
  in.pressed = in.held = BK_BUTTON_CONFIRM;
  return tick(scene, r, a, sink, in, e);
}
static int await_flow(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink,
                       unsigned flow, unsigned limit, char e[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  for (unsigned i = 0; i < limit; ++i) {
    if (s->flow.current == flow) return 1;
    if (!tick(scene, r, a, sink, pointer(4, 920), e)) return 0;
  }
  snprintf(e, 256, "flow%02x did not reach%02x phase%d state%d action%d", s->flow.current,
      flow, s->ending_state.frame.phase, s->ending_state.frame.state_721ee0,
      s->ending_state.frame.transition_action);
  return 0;
}
static int settle(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink, char e[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  for (unsigned i = 0; i < 400; ++i) {
    if (i && !s->common.blocked && !s->common.curtain.stage) return 1;
    if (!tick(scene, r, a, sink, pointer(4, 920), e)) return 0;
  }
  snprintf(e, 256, "menu curtain did not settle"); return 0;
}
static int stick_click(BkScene *scene, BkRenderer *r, BkAudio *a, Sink *sink,
                         float x, float y, char e[256]) {
  PlaySession *s = bk_scene_custom_context(scene);
  for (unsigned i = 0; i < 240; ++i) {
    double dx = x * .125 - s->cursor.sprite.rect[0];
    double dy = y * .125 - s->cursor.sprite.rect[1];
    double length = hypot(dx, dy);
    if (length < .01) {
      if (!tick(scene, r, a, sink, (BkInput){0}, e)) return 0;
      return tick(scene, r, a, sink,
          (BkInput){.pressed = BK_BUTTON_CONFIRM, .held = BK_BUTTON_CONFIRM}, e);
    }
    double magnitude = .18 + .82 * fmin(1, length / (80. / 60.));
    BkInput input = {.move_x = (float)(dx / length * magnitude),
                     .move_y = (float)(-dy / length * magnitude)};
    if (!tick(scene, r, a, sink, input, e)) return 0;
  }
  snprintf(e, 256, "gallery stick failed to reach actual menu button"); return 0;
}
int main(int argc, char **argv) {
  if (argc != 3) return 2;
  char error[256] = {0}, path[2048]; int result = 1;
  BkResourceStore *store = NULL; BkRenderer *renderer = NULL;
  BkAudio *audio = NULL; BkUnlockFile *unlocks = NULL;
  BkCaptureFiles *captures = NULL; BkScene *scene = NULL;
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  CHECK(!mkdir(argv[2], 0755) || errno == EEXIST);
  CHECK(store = bk_resources_create(error));
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_02", "bk3_03", "bk3_04",
      "bk3_06", "bk3_08", "bk3_09", "bk3_10", "bk3_11", "bk3_12", "bk3_13",
      "bk3_14", "bk3_15", "bk3_18", "fambom"};
  for (unsigned i=0; i<sizeof(packs)/sizeof(*packs); ++i) {
    CHECK(snprintf(path,sizeof path,"%s/%s.pp",argv[1],packs[i]) < (int)sizeof path);
    CHECK(bk_resources_mount(store,packs[i],path,error));
  }
  CHECK(bk_resources_mount_directory(store,"faces",argv[1],20480,error));
  CHECK(renderer = bk_renderer_create(200,120,stdout,error));
  BkRenderStats baseline = bk_renderer_stats(renderer);
  BkAudioSink output = {&sink,48000,480,1920,submit,poll};
  CHECK(audio = bk_audio_create(&output,error));
  snprintf(path,sizeof path,"%s/unlocks",argv[2]);
  CHECK(unlocks = bk_unlock_file_create(path,error));
  BkUnlockTable saved;
  for (unsigned g=0;g<5;++g) {
    uint8_t row[8]; memset(row,1,sizeof row);
    CHECK(bk_unlock_file_store(unlocks,g,row,&saved,error));
  }
  snprintf(path,sizeof path,"%s/captures",argv[2]);
  CHECK(captures = bk_capture_files_create(path,error));
  BkSceneServices services = {store,renderer,stdout,audio,captures};
  CHECK(scene = bk_play_session_create_with_storage(&services,NULL,unlocks,error));
  CHECK(present(scene,renderer,audio,error));
  PlaySession *s = bk_scene_custom_context(scene);
  s->special_clock_read = clock_fixture;
  s->capture_output.clock = (BkCaptureClock){NULL,photo_clock_fixture};
  s->game_state.random = UINT32_C(0x4e29b048);
  CHECK(wait_frames(scene,renderer,audio,&sink,64,pointer(4,920),error));
  CHECK(click(scene,renderer,audio,&sink,1084,262,error));
  CHECK(await_flow(scene,renderer,audio,&sink,0x18,200,error));
  CHECK(settle(scene,renderer,audio,&sink,error));
  for (unsigned visit=0;visit<10;++visit) {
    unsigned g=visit%5;
    CHECK(s->flow.current == 0x18);
    CHECK(stick_click(scene,renderer,audio,&sink,92,206+g*150,error));
    CHECK(click(scene,renderer,audio,&sink,840,288,error));
    CHECK(await_flow(scene,renderer,audio,&sink,0x48,250,error));
    CHECK(s->game_state.group == g && s->game_state.area == 0);
    CHECK(s->game_state.hotkeys.photo_count == (int)(visit/5));
    ++entries;
    /* First visits wait for actual opening completion; second visits use
     * the genuine confirm skip and retained process state. */
    if (visit>=5) {
      BkInput skip=pointer(4,920);skip.pressed=BK_BUTTON_CONFIRM;
      CHECK(tick(scene,renderer,audio,&sink,skip,error));
    }
    unsigned opening=0;
    while(s->special_process.phase != 2 && opening++<6000)
      CHECK(tick(scene,renderer,audio,&sink,pointer(4,920),error));
    CHECK(s->special_process.phase == 2);
    CHECK(again(scene,renderer,audio,error));
    CHECK(wait_frames(scene,renderer,audio,&sink,90,pointer(4,920),error));
    CHECK(wait_frames(scene,renderer,audio,&sink,80,pointer(1184,42),error));
    int32_t clip=s->ending_state.frame.camera_clip;
    CHECK(click(scene,renderer,audio,&sink,1184,90,error));
    CHECK(s->ending_state.frame.camera_clip==(clip+1)%3);
    CHECK(click(scene,renderer,audio,&sink,1184,174,error));
    uint8_t visible=s->ending_state.control.toggles[0];
    CHECK(click(scene,renderer,audio,&sink,1184,298,error));
    CHECK(s->ending_state.control.toggles[0] == !visible);
    CHECK(click(scene,renderer,audio,&sink,1184,298,error));
    CHECK(s->ending_state.control.toggles[0] == visible);
    CHECK(click(scene,renderer,audio,&sink,1184,210,error));
    CHECK(s->ending_state.frame.camera_mode==1);
    CHECK(wait_frames(scene,renderer,audio,&sink,16,pointer(1184,210),error));
    CHECK(click(scene,renderer,audio,&sink,1184,210,error));
    CHECK(wait_frames(scene,renderer,audio,&sink,100,pointer(4,920),error));
    CHECK(s->ending_state.frame.camera_mode==0);
    /* L/R controls outside the sidebar; A/B must not move this camera. */
    float yaw=s->menu_camera.yaw;
    BkInput orbit={.held=BK_BUTTON_CAMERA_ORBIT,.look_x=.5f};
    CHECK(tick(scene,renderer,audio,&sink,orbit,error));
    CHECK(s->menu_camera.yaw < yaw);
    float radius=s->menu_camera.radius;
    BkInput zoom={.held=BK_BUTTON_CAMERA_ADJUST,.look_x=.5f};
    CHECK(tick(scene,renderer,audio,&sink,zoom,error));
    CHECK(s->menu_camera.radius < radius);
    BkInput confirm={.held=BK_BUTTON_CONFIRM|BK_BUTTON_BACK,.look_x=.5f};
    BkMenuCamera before=s->menu_camera;
    CHECK(tick(scene,renderer,audio,&sink,confirm,error));
    CHECK(s->menu_camera.yaw == before.yaw && s->menu_camera.radius == before.radius);
    BkInput photo={.pressed=BK_BUTTON_PHOTO};
    unsigned written=photos;
    CHECK(tick(scene,renderer,audio,&sink,photo,error));
    CHECK(photos==written && s->game_state.hotkeys.photo_count==(int)(visit/5+1));
    CHECK(again(scene,renderer,audio,error));
    CHECK(tick(scene,renderer,audio,&sink,(BkInput){0},error));
    CHECK(photos==written+1);
    CHECK(again(scene,renderer,audio,error));
    /* Open the real sidebar before clicking its exit button. */
    CHECK(wait_frames(scene,renderer,audio,&sink,80,pointer(1184,42),error));
    CHECK(click(scene,renderer,audio,&sink,1184,382,error));
    CHECK(await_flow(scene,renderer,audio,&sink,0x18,300,error));
    CHECK(s->special_process.phase==0 && !s->special_process.ui.loaded);
    CHECK(settle(scene,renderer,audio,&sink,error));
    int32_t counts[5]; CHECK(bk_capture_files_count_photos(captures,counts,error));
    for(unsigned j=0;j<5;++j) CHECK(counts[j]==(int)(visit/5+(j<=g)));
    ++returns;
    if (visit==4) {
      /*Cross the real ending owner before returning to special: the same
       *camera/transitions must survive menu collection and late GPU draws.*/
      CHECK(click(scene,renderer,audio,&sink,612,240,error));
      CHECK(await_flow(scene,renderer,audio,&sink,0x10,250,error));
      CHECK(wait_frames(scene,renderer,audio,&sink,90,pointer(1200,730),error));
      CHECK(click(scene,renderer,audio,&sink,1200,730,error));
      CHECK(wait_frames(scene,renderer,audio,&sink,32,pointer(480,520),error));
      CHECK(click(scene,renderer,audio,&sink,480,520,error));
      CHECK(await_flow(scene,renderer,audio,&sink,1,250,error));
      CHECK(wait_frames(scene,renderer,audio,&sink,64,pointer(4,920),error));
      CHECK(click(scene,renderer,audio,&sink,1084,262,error));
      CHECK(await_flow(scene,renderer,audio,&sink,0x18,250,error));
      CHECK(settle(scene,renderer,audio,&sink,error));
    }
    printf("special-app group%u visit%u opening%u returned photos%u\n",g,visit,opening,photos);fflush(stdout);
  }
  CHECK(entries==10 && returns==10 && photos==10 && sink.nonzero);
  CHECK(clock_calls[0] && clock_calls[1] && clock_calls[2]);
  /*Loader failure after real UI/body creation must release all borrowers,
   *and leave the output photo count unchanged.*/
  CHECK(stick_click(scene,renderer,audio,&sink,92,356,error));
  CHECK(click(scene,renderer,audio,&sink,840,288,error));
  fail_movie_clock=1;
  CHECK(!await_flow(scene,renderer,audio,&sink,0x48,250,error));
  CHECK(strstr(error,"injected movie clock failure") && !fail_movie_clock && photos==10);
  error[0]=0;
  bk_scene_destroy(scene);scene=NULL;
  BkRenderStats after=bk_renderer_stats(renderer);
  CHECK(after.live_allocations==baseline.live_allocations && after.live_bytes==baseline.live_bytes);
  printf("PASS special-app entries%u returns%u photos%u frames%u redraws%u clocks%u/%u/%u state%016llx PCM%016llx samples%llu nonzero%llu\n",
      entries,returns,photos,frames,redraws,clock_calls[0],clock_calls[1],clock_calls[2],
      (unsigned long long)state_hash,(unsigned long long)sink.hash,
      (unsigned long long)sink.submitted*2,(unsigned long long)sink.nonzero);
  result=0;
done:
  bk_scene_destroy(scene);bk_capture_files_destroy(captures);
  bk_unlock_file_destroy(unlocks);bk_audio_destroy(audio);
  bk_renderer_destroy(renderer);bk_resources_destroy(store);return result;
}
