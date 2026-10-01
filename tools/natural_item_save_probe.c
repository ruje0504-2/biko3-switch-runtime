/* Group0 area1 -> item1 -> actual exit/save -> fresh-process menu load,
 * then area2 -> area3 -> area4 save/continue and fresh-process loads.
 * Only the incoming area1 loader and initial random seed are fixtures.
 * All later movement, inventory, outcomes and flow transitions belong to the
 * application. This is not a title-to-ending walkthrough. */
#include "../runtime/app/play_session.c"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
  fprintf(stderr, "natural-item line%d: %s (%s)\n", __LINE__, #x, e); \
  goto done; } } while (0)

typedef struct {
  BkScene *scene;
  BkRenderer *renderer;
  BkAudio *audio;
  unsigned frames;
} Run;

static int submit(void *context, const int16_t *pcm, size_t count, char e[256]) {
  (void)pcm; (void)e;
  *(uint64_t *)context += count;
  return 1;
}
static int poll(void *context, uint64_t *count, char e[256]) {
  (void)e;
  *count = *(uint64_t *)context;
  return 1;
}
static int present(Run *r, char e[256]) {
  return bk_audio_fill(r->audio, e) && bk_renderer_begin(r->renderer, e) &&
      bk_scene_draw(r->scene, &(BkSceneFrame){0}, e) &&
      bk_renderer_end(r->renderer, e) &&
      bk_play_session_after_present(r->scene, e);
}
static int tick(Run *r, BkInput in, char e[256]) {
  ++r->frames;
  return bk_audio_poll(r->audio, e) &&
      bk_scene_step(r->scene, 1. / 60, &in, e) && present(r, e);
}
static int wait_flow(Run *r, uint8_t target, char e[256]) {
  for (unsigned i = 0; i < 600; ++i) {
    if (bk_play_session_flow(r->scene)->current == target) return 1;
    if (!tick(r, (BkInput){0}, e)) return 0;
  }
  snprintf(e, 256, "flow%x did not reach%x",
      bk_play_session_flow(r->scene)->current, target);
  return 0;
}
static int ready(Run *r, char e[256]) {
  for (unsigned i = 0; i < 3600; ++i) {
    const BkGameFrameState *s = bk_play_session_state(r->scene);
    if (bk_play_session_flow(r->scene)->current == 2 &&
        s->camera.phase == 1 && !bk_play_session_common(r->scene)->curtain.alpha)
      return 1;
    if (!tick(r, (BkInput){.pressed = i % 30 == 29 ? BK_BUTTON_CONFIRM : 0}, e))
      return 0;
  }
  snprintf(e, 256, "game opening did not finish");
  return 0;
}
static int settle(Run *r, char e[256]) {
  for (unsigned i = 0; i < 100; ++i) {
    if (!tick(r, (BkInput){0}, e)) return 0;
    const BkCommonHudState *s = bk_play_session_common(r->scene);
    if (i > 20 && !s->blocked && !s->curtain.stage) return 1;
  }
  snprintf(e, 256, "menu transition did not settle");
  return 0;
}
static int click(Run *r, float x, float y, char e[256]) {
  unsigned width, height;
  BkViewport v;
  bk_renderer_extent(r->renderer, &width, &height);
  if (!bk_camera_fit(&v, width, height, 4, 3)) return 0;
  BkInput in = {.pointer_active = 1,
      .pointer_x = v.x + x * (v.width / 1280.f),
      .pointer_y = v.y + y * (v.width / 1280.f)};
  if (!tick(r, in, e)) return 0;
  in.pressed = BK_BUTTON_CONFIRM;
  return tick(r, in, e);
}
static int tracking(const BkGameFrameState *s, const BkFlowTransition *f,
                    char e[256]) {
  if (!s->interaction.outcome && f->current == 2) return 1;
  snprintf(e, 256, "tracking interrupted: flow%x outcome%u",
      f->current, s->interaction.outcome);
  return 0;
}
static void dump_state(const char *tag, const BkGameFrameState *s,
                       const BkFlowTransition *f) {
  fprintf(stderr,
      "%s flow%02x prev%02x area%u pos%.2f/%.2f yaw%.1f npc%.2f/%.2f "
      "cursor%u hidden%u visible%u outcome%u action%d mode%d wall=%s near%d\n", tag,
      f->current, f->previous,
      s->area, s->player.spatial.movement.position[0],
      s->player.spatial.movement.position[2], s->player.spatial.movement.yaw,
      s->npc.path.position[0], s->npc.path.position[2], s->npc.path.cursor,
      s->npc.ai.point.motion.hidden, s->npc.visible, s->interaction.outcome,
      s->player.spatial.movement.action, s->player.spatial.movement.interaction_mode,
      s->player.spatial.scene.wall_name,
      s->player.spatial.scene.wall.near_wall);
}
enum Destination { POINT, WALL_WAIT, PICKUP, EXIT };
static int move(Run *r, float x, float z, unsigned limit,
                enum Destination goal, char e[256]) {
  for (unsigned i = 0; i < limit; ++i) {
    const BkGameFrameState *s = bk_play_session_state(r->scene);
    const BkFlowTransition *f = bk_play_session_flow(r->scene);
    if (goal == EXIT && !s->interaction.outcome &&
        (f->current == 0x20 || (f->current == 0x50 && f->target == 0x20)))
      return 1;
    if (!tracking(s, f, e)) return 0;
    if (goal == PICKUP && s->pickup.collected[1] == 1) return 1;
    const BkPlayerMovement *p = &s->player.spatial.movement;
    double dx = (double)x - p->position[0], dz = (double)z - p->position[2];
    double distance = hypot(dx, dz);
    if (goal == POINT && distance <= 5) return 1;
    double delta = atan2(dx, dz) * 180. / 3.14159265358979323846 - p->yaw;
    while (delta > 180) delta -= 360;
    while (delta < -180) delta += 360;
    BkInput in = {0};
    if (fabs(delta) > 7) in.look_x = delta > 0 ? 1 : -1;
    else if (distance > 5) in.held = BK_BUTTON_UP;
    if (!tick(r, in, e)) return 0;
  }
  const BkGameFrameState *s = bk_play_session_state(r->scene);
  if (goal == WALL_WAIT && tracking(s, bk_play_session_flow(r->scene), e) &&
      s->player.spatial.scene.wall.near_wall &&
      !strcmp(s->player.spatial.scene.wall_name, "Mesh_Atari_Hantei@M00_11.X"))
    return 1;
  snprintf(e, 256, "destination %.1f/%.1f failed at %.3f/%.3f wall=%s", x, z,
      s->player.spatial.movement.position[0], s->player.spatial.movement.position[2],
      s->player.spatial.scene.wall_name);
  return 0;
}
static int idle_tracking(Run *r, unsigned frames, char e[256]) {
  for (unsigned i = 0; i < frames; ++i)
    if (!tracking(bk_play_session_state(r->scene),
                   bk_play_session_flow(r->scene), e) ||
        !tick(r, (BkInput){0}, e)) return 0;
  return tracking(bk_play_session_state(r->scene),
                   bk_play_session_flow(r->scene), e);
}
static int drive_exit(Run *r, char e[256]) {
  unsigned area = bk_play_session_state(r->scene)->area;
  /* Both routes allow the NPC to leave before the player follows. */
  for (unsigned i = 0; i < 18000; ++i) {
    const BkGameFrameState *s = bk_play_session_state(r->scene);
    if (s->npc.ai.point.motion.hidden) break;
    if (!idle_tracking(r, 1, e)) return 0;
  }
  if (!bk_play_session_state(r->scene)->npc.ai.point.motion.hidden) {
    snprintf(e, 256, "area%u NPC did not leave", area);
    return 0;
  }
  if (area == 2) {
    /* South exit, west of the small box at z85..97. */
    const float path[][2] = {{-103, 118}, {-103, 60}, {-92, 60}};
    for (unsigned i = 0; i < sizeof(path) / sizeof(*path); ++i)
      if (!move(r, path[i][0], path[i][1], 1800, POINT, e)) return 0;
    return move(r, -92, -15, 1800, EXIT, e);
  }
  /* Area3 west exit at x=-258.76, z=-317.18..-257.18. */
  return move(r, -197, -280, 1800, POINT, e) &&
      move(r, -220, -280, 1800, POINT, e) &&
      move(r, -280, -280, 1800, EXIT, e);
}

int main(int argc, char **argv) {
  if (argc != 4 || (strcmp(argv[3], "produce") && strcmp(argv[3], "reload") &&
                    strcmp(argv[3], "continue"))) {
    fprintf(stderr,
        "usage: natural-item-save-probe DATA OUTPUT produce|reload|continue\n");
    return 2;
  }
  int produce = !strcmp(argv[3], "produce");
  int continue_mode = !strcmp(argv[3], "continue"), result = 1;
  char e[256] = {0}, path[2048];
  uint64_t submitted = 0;
  BkResourceStore *resources = NULL;
  BkCaptureFiles *captures = NULL;
  BkCheckpointFiles *files = NULL;
  BkCheckpointBank bank = {0};
  const uint8_t expected[5] = {0, 1, 0, 0, 0};
  uint32_t expected_area = 2;
  BkRenderStats baseline = {0};
  Run r = {0};
  CHECK(resources = bk_resources_create(e));
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_02", "bk3_03", "bk3_04",
      "bk3_05", "bk3_06", "bk3_07", "bk3_15", "bk3_16", "bk3_20"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    CHECK(snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) < (int)sizeof(path));
    CHECK(bk_resources_mount(resources, packs[i], path, e));
  }
  const char *loose[] = {"routes", "faces", "collision", "fonts"};
  for (unsigned i = 0; i < sizeof(loose) / sizeof(*loose); ++i)
    CHECK(bk_resources_mount_directory(resources, loose[i], argv[1], 16 * 1024 * 1024, e));
  CHECK(captures = bk_capture_files_create(argv[2], e));
  CHECK(files = bk_checkpoint_files_create(argv[2], e));
  CHECK(bk_checkpoint_file_read(files, 0, &bank, e) ==
      (produce ? BK_RESOURCE_MISSING : BK_RESOURCE_OK));
  if (!produce) {
    expected_area = bank.slots[0].area;
    CHECK(bank.slots[0].stamp[0] &&
        (expected_area == 2 || expected_area == 3 ||
         (!continue_mode && expected_area == 4)) &&
        !memcmp(bank.slots[0].inventory, expected, sizeof(expected)));
  }
  CHECK(r.renderer = bk_renderer_create(80, 48, stderr, e));
  baseline = bk_renderer_stats(r.renderer);
  BkAudioSink sink = {&submitted, 48000, 480, 1920, submit, poll};
  CHECK(r.audio = bk_audio_create(&sink, e));
  BkSceneServices services = {resources, r.renderer, stderr, r.audio, captures};
  CHECK(r.scene = bk_play_session_create_development(&services, files, e));
  PlaySession *s = bk_scene_custom_context(r.scene);
  s->game_state.random = 0x50caa2; /* Reproducible initial seed, before input. */
  CHECK(present(&r, e) && ready(&r, e));
  CHECK(s->game_state.area == 0 && !s->game_state.pickup.collected[1]);
  if (produce) {
    /* Explicit incoming boundary; no inventory/outcome/action injection. */
    CHECK(bk_game_preview_suspend_area(s->game, e));
    CHECK(bk_game_preview_advance_area(s->game, s->flow.previous, e));
    CHECK(s->game_state.area == 1 && ready(&r, e));
    unsigned start = r.frames;
    const float approach[][2] = {{103, 110}, {20, 110}, {240, 110}, {240, 180}};
    for (unsigned i = 0; i < sizeof(approach) / sizeof(*approach); ++i)
      CHECK(move(&r, approach[i][0], approach[i][1], 3000, POINT, e));
    /* Remain at the actual wall while the NPC completes its patrol. */
    CHECK(move(&r, 260, 180, 3000, WALL_WAIT, e));
    CHECK(idle_tracking(&r, 3500, e));
    const float pickup[][2] = {{240, 150}, {295, 150}, {312, 175}, {310, 240}};
    for (unsigned i = 0; i < sizeof(pickup) / sizeof(*pickup); ++i)
      CHECK(move(&r, pickup[i][0], pickup[i][1], 3000, POINT, e));
    /* Pass through the item: native pickup tests the movement segment,
     * not a radius around a stationary endpoint. */
    CHECK(move(&r, 345, 236, 3000, PICKUP, e));
    CHECK(s->game_state.pickup.collected[1] == 1);
    printf("PICKUP group0 item1 frames%u\n", r.frames - start);
    CHECK(move(&r, 240, 150, 1800, POINT, e));
    for (unsigned i = 0; i < 3600 && !s->game_state.npc.ai.point.motion.hidden; ++i)
      CHECK(idle_tracking(&r, 1, e));
    CHECK(s->game_state.npc.ai.point.motion.hidden);
    CHECK(move(&r, 240, 110, 3000, POINT, e));
    CHECK(move(&r, 20, 110, 3000, POINT, e));
    CHECK(move(&r, -20, 110, 600, EXIT, e));
    CHECK(wait_flow(&r, 0x20, e) && settle(&r, e));
    CHECK(click(&r, 496, 548, e) && wait_flow(&r, 0x28, e) && settle(&r, e));
    CHECK(click(&r, 750, 250, e) && click(&r, 496, 548, e));
    CHECK(bk_checkpoint_file_read(files, 0, &bank, e) == BK_RESOURCE_OK);
    CHECK(click(&r, 1100, 908, e) && wait_flow(&r, 2, e) && ready(&r, e));
  } else {
    CHECK(tick(&r, (BkInput){.pressed = BK_BUTTON_PAUSE}, e));
    CHECK(wait_flow(&r, 4, e) && settle(&r, e));
    CHECK(click(&r, 640, 336, e) && wait_flow(&r, 0x28, e) && settle(&r, e));
    CHECK(click(&r, 750, 250, e) && click(&r, 496, 548, e));
    CHECK(wait_flow(&r, 2, e) && ready(&r, e));
  }
  CHECK(s->game_state.area == expected_area &&
      !memcmp(s->game_state.pickup.collected, expected, sizeof(expected)));
  if (continue_mode) {
    if (!drive_exit(&r, e)) {
      dump_state("Area continuation failed", &s->game_state, &s->flow);
      goto done;
    }
    CHECK(wait_flow(&r, 0x20, e) && settle(&r, e));
    CHECK(click(&r, 496, 548, e) && wait_flow(&r, 0x28, e) && settle(&r, e));
    CHECK(click(&r, 750, 250, e) && click(&r, 496, 548, e));
    CHECK(bk_checkpoint_file_read(files, 0, &bank, e) == BK_RESOURCE_OK);
    ++expected_area;
    CHECK(click(&r, 1100, 908, e) && wait_flow(&r, 2, e) && ready(&r, e));
  }
  CHECK(bank.slots[0].area == expected_area && bank.slots[0].stamp[0] &&
      !memcmp(bank.slots[0].inventory, expected, sizeof(expected)));
  CHECK(s->game_state.area == expected_area &&
      !memcmp(s->game_state.pickup.collected, expected, sizeof(expected)));
  CHECK(idle_tracking(&r, 120, e));
  BkCheckpointBank after = {0};
  CHECK(bk_checkpoint_file_read(files, 0, &after, e) == BK_RESOURCE_OK);
  CHECK(!memcmp(&bank, &after, sizeof(bank)));
  printf("PASS natural-item %s group0 area%u inventory01000 frames%u\n",
         argv[3], expected_area, r.frames);
  result = 0;
done:
  bk_scene_destroy(r.scene);
  if (!result) {
    BkRenderStats end = bk_renderer_stats(r.renderer);
    if (end.live_allocations != baseline.live_allocations || end.live_bytes != baseline.live_bytes)
      result = 1;
    else puts("PASS GPU allocations returned to renderer baseline");
  }
  bk_audio_destroy(r.audio);
  bk_renderer_destroy(r.renderer);
  bk_capture_files_destroy(captures);
  bk_checkpoint_files_destroy(files);
  bk_resources_destroy(resources);
  return result;
}
