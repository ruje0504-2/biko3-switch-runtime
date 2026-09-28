/* Actual45 entry/background/prop/item/voice compositions. Supplied explicit
 * retained session/input fixtures, real actor resets and follow collision;
 * screen fields are fixtures, not recovered gameplay initialization/rendering.
 */
#include "scene/entry_forest.h"
#include "scene/game_frame.h"
#include "scene/opening_session.h"
#include "scene/player_hud_session.h"
#include "ui/text_canvas.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  uint64_t submitted, samples, nonzero, hash;
} Sink;
static int submit(void *context, const int16_t *pcm, size_t frames,
                  char *error) {
  (void)error;
  Sink *s = context;
  s->submitted += frames;
  for (size_t i = 0; i < frames * 2; ++i) {
    s->nonzero += pcm[i] != 0;
    s->samples++;
    uint16_t bits = (uint16_t)pcm[i];
    for (unsigned b = 0; b < 2; ++b) {
      s->hash ^= (bits >> (b * 8)) & 255;
      s->hash *= UINT64_C(1099511628211);
    }
  }
  return 1;
}
static int poll(void *context, uint64_t *consumed, char *error) {
  (void)error;
  *consumed = ((Sink *)context)->submitted;
  return 1;
}
typedef struct {
  BkResourceStore *store;
  BkFont *font;
  BkTextCanvas *canvas;
  BkTextStyle style;
  BkMessage empty;
  const BkMessage *message;
  unsigned uploads;
} CpuText;
static int text_clear(void *context, char error[256]) {
  (void)error;
  CpuText *t = context;
  bk_text_canvas_destroy(t->canvas);
  t->canvas = NULL;
  bk_font_destroy(t->font);
  t->font = NULL;
  return 1;
}
static int text_recreate(void *context, BkNoticeTextKind kind,
                         char error[256]) {
  CpuText *t = context;
  text_clear(t, error);
  BkBlob blob = {0};
  if (!bk_notice_text_style(kind, &t->style) ||
      bk_resources_read(t->store, "fonts", "Type_S.FTT", &blob, error) !=
          BK_RESOURCE_OK)
    return 0;
  t->font = bk_font_decode(blob.data, blob.size, error);
  bk_blob_free(&blob);
  if (!t->font)
    return 0;
  t->canvas = bk_text_canvas_create(t->font, (uint32_t)t->style.width,
                                    (uint32_t)t->style.height, error);
  t->message = &t->empty;
  return t->canvas != NULL;
}
static int text_bind(void *context, const BkMessage *message, char error[256]) {
  (void)error;
  ((CpuText *)context)->message = message;
  return 1;
}
static int text_prepare(CpuText *t, BkTextFlow *flow, float seconds,
                        char error[256]) {
  BkTextDraw draw;
  int upload;
  if (!t->canvas || !t->message ||
      !bk_text_canvas_prepare(t->canvas, &t->style, t->message->bytes,
                              t->message->length, seconds, 640, flow, &draw,
                              &upload, error))
    return 0;
  t->uploads += upload;
  return 1;
}
static int check_actor_entries(BkResourceStore *store, char *error) {
  unsigned count = 0, rejected = 0;
  const uint8_t flows[] = {8, 0x38, 1, 0x48};
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned a = 0; a < 9; ++a)
      for (unsigned f = 0; f < sizeof(flows); ++f) {
        BkEntryRequest request = {g, a, 0, flows[f]};
        BkEntrySelection selection;
        assert(bk_game_entry_select(&selection, &request));
        BkBlob data = {0};
        if (bk_resources_read(store, "routes", selection.route_file, &data,
                              error) != BK_RESOURCE_OK)
          return 0;
        BkRoute *route = bk_route_decode(data.data, data.size, error);
        bk_blob_free(&data);
        if (!route)
          return 0;
        int invalid = selection.route_cursor >= bk_route_count(route);
        uint32_t start = invalid ? 0 : selection.route_cursor;
        int8_t old_flag = (int8_t)bk_route_point(route, start)->flags;
        bk_route_destroy(route);
        BkEntryAssets *entry = bk_entry_assets_create(store, &request, error);
        if (invalid) {
          assert(!entry);
          rejected++;
          continue;
        }
        if (!entry)
          return 0;
        BkGameFrameState state = {
            .random = 17,
            .camera = {.stage = 9, .transition = 2},
            .player_hidden = 1,
            .player_sound_suppressed = 7,
            .player_events = {.loop_latched = 1, .noise = 13},
            .interaction = {.prompt = 7, .response = 9, .outcome = 4},
            .boundary = {3, 9, 8},
            .pickup = {.collected = {2, 3, 4, 5, 6}, .notice_visible = 9}};
        state.hotkeys = (BkPlayerHotkeys){
            .menu_request = 9, .photo_count = 73, .photos = {9, 8, 7, 6, 5}};
        state.player_actions[7] = 55;
        state.player_actions[15] = 66;
        state.npc_actions.walk[1] = 37;
        state.npc_actions.run[1] = 38;
        state.npc.ai.point.motion.hidden = 1;
        state.npc.ai.point.action_wait = (BkTimer){7, 111, 1};
        state.npc.path.run_remaining = 3;
        state.player.spatial.movement.turn[0] = 7;
        state.player.spatial.movement.move_latch = 4;
        state.player.interaction.script_phase = 3;
        state.player.return_yaw = 13;
        memset(state.player_latches, 7, sizeof(state.player_latches));
        memset(state.shared_latches, 8, sizeof(state.shared_latches));
        float player_head =
            bk_actor_pose_head(bk_entry_assets_player(entry))[1];
        float npc_head = bk_actor_pose_head(bk_entry_assets_actor(entry))[1];
        uint32_t clocks[4] = {1000, 1001, 1002, 1003};
        if (!bk_scene_game_frame_initialize_actors(entry, &state, start, clocks,
                                                   error)) {
          bk_entry_assets_destroy(entry);
          return 0;
        }
        assert(state.group == g && state.area == a &&
               state.camera.phase == selection.phase &&
               state.camera.stage == 9 && state.camera.transition == 2 &&
               state.npc.ai.point.motion.mode == selection.phase);
        assert(state.player.spatial.vertical_position ==
               (float)((double)player_head + 10));
        assert(state.npc_vertical ==
               (float)((double)npc_head + selection.player_position[1]));
        const BkActorPlacement *p =
            bk_actor_pose_placement(bk_entry_assets_player(entry));
        assert(
            !memcmp(p->position, state.player.spatial.movement.position, 12));
        assert(p->yaw_degrees == state.player.spatial.movement.yaw);
        p = bk_actor_pose_placement(bk_entry_assets_actor(entry));
        assert(!memcmp(p->position, state.npc.path.position, 12));
        assert(state.npc.ai.point.motion.route_flag == old_flag);
        assert(bk_route_point(bk_entry_assets_route(entry), start)->flags ==
               (flows[f] == 0x48 ? 1 : (uint8_t)old_flag));
        assert(state.npc_actions.walk[1] == 37 &&
               state.npc_actions.run[1] == 38);
        assert(state.player_actions[7] == 55 && state.player_actions[15] == 66);
        assert(state.npc.ai.point.motion.hidden == 1 &&
               state.npc.path.run_remaining == 3);
        assert(state.npc.ai.point.action_wait.duration == 2000 &&
               state.npc.ai.point.action_wait.deadline == 111 &&
               state.npc.ai.point.action_wait.armed == 1);
        assert(state.player.spatial.movement.turn[0] == 7 &&
               state.player.spatial.movement.move_latch == 4 &&
               state.player.interaction.script_phase == 3 &&
               state.player.return_yaw == 13);
        assert(state.interaction.prompt == 7 && !state.interaction.response &&
               !state.interaction.outcome && !state.player_hidden &&
               !state.player_sound_suppressed &&
               !state.boundary.npc_sound_played &&
               !state.boundary.player_sound_played &&
               state.boundary.ambient_gate == 3);
        for (unsigned j = 0; j < 5; ++j)
          assert(state.pickup.collected[j] == (j == 1 && g == 1 ? 1
                                               : f < 2          ? 0
                                                                : j + 2));
        assert(!state.hotkeys.menu_request && state.hotkeys.photo_count == 73);
        for (unsigned j = 0; j < 5; ++j)
          assert(state.hotkeys.photos[j] == 9 - (int)j);
        assert(state.pickup.notice_visible == 9 &&
               state.player_events.loop_latched == 1);
        for (unsigned j = 0; j < sizeof(state.player_latches); ++j)
          assert(state.player_latches[j] == 7);
        for (unsigned j = 0; j < sizeof(state.shared_latches); ++j)
          assert(state.shared_latches[j] == 8);
        BkGameFrameState saved = state;
        assert(!bk_scene_game_frame_initialize_actors(entry, &state, start,
                                                      clocks, error));
        assert(!memcmp(&state, &saved, sizeof(state)));
        bk_entry_assets_destroy(entry);
        count++;
      }
  assert(count == 166 && rejected == 14);
  printf("PASS actor entry resources=%u invalid-overrides=%u "
         "retained-state/heads/roots\n",
         count, rejected);
  return 1;
}
static int check_resolved_returns(BkResourceStore *store, char *error) {
  const unsigned areas[] = {4, 5, 5, 6, 5};
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned area = 0; area < 9; ++area) {
      BkGameFrameState s;
      BkEntryProgress progress;
      assert(bk_scene_game_frame_boot_state(&s, &progress, 12345));
      assert(s.random == 12345 && s.player_view.lean == 10 &&
             s.props.alpha == 0);
      assert(!s.npc_actions.walk[1] && !s.npc_actions.run[1]);
      BkEntryRequest request;
      uint32_t start;
      assert(bk_game_entry_resolve(&request, &start, &progress, g, area, 0x48));
      assert(request.area == areas[g] && start == 0);
      BkEntryAssets *entry = bk_entry_assets_create(store, &request, error);
      if (!entry)
        return 0;
      s.player_view.probe[0] = 17;
      s.player_view.blocked[0] = 3;
      s.player_view.lean = -10;
      s.player_view.selected_ray = 7;
      s.camera.stage = 9;
      uint32_t clocks[4] = {99, 100, 101, 102};
      if (!bk_scene_game_frame_initialize_entry(entry, &s, start, clocks,
                                                error)) {
        bk_entry_assets_destroy(entry);
        return 0;
      }
      assert(s.group == g && s.area == areas[g] && s.camera.phase == 3 &&
             s.camera.stage == 9);
      assert(s.npc.path.cursor == request.route_cursor);
      assert(s.player_view.yaw == 0 && s.player_view.pitch == 0 &&
             s.player_view.distance == 40 &&
             s.player_view.target_distance == 40 &&
             s.player.spatial.scene.wall.camera_distance == 40);
      assert(s.player_view.probe[0] == 17 && s.player_view.blocked[0] == 3 &&
             s.player_view.lean == -10 && s.player_view.selected_ray == 7);
      assert(!memcmp(&s.player_view.pose,
                     bk_follow_camera_pose(bk_entry_assets_camera(entry)),
                     sizeof(s.player_view.pose)));
      bk_entry_assets_destroy(entry);
    }
  puts("PASS resolved returns=45 cold-state and mode2 camera binding");
  return 1;
}
typedef struct {
  unsigned requests, group, photo;
} CaptureRequests;
static int capture_request(void *context, int photo, unsigned album,
                           char error[256]) {
  (void)error;
  CaptureRequests *c = context;
  ++c->requests;
  c->photo = (unsigned)photo;
  c->group = album;
  return 1;
}
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
int main(int argc, char **argv) {
  if (argc != 2 && (argc != 3 || (strcmp(argv[2], "--opening") &&
                                  strcmp(argv[2], "--hotkeys"))))
    return 2;
  int opening_mode = argc == 3;
  int hotkey_mode = argc == 3 && !strcmp(argv[2], "--hotkeys");
  CaptureRequests captures = {0};
  unsigned hotkey_requests = 0;
  BkSystemAudio *keys[3] = {0};
  int rc = 1;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkAudio *audio = NULL;
  BkSystemAudio *system = NULL, *click = NULL;
  BkDialogueAssets dialogue = {0};
  CpuText text = {0};
  unsigned handovers = 0;
  BkEntryForest *forest = NULL;
  BkGameFrameServices v = {0};
  BkGameFrameState s = {0};
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  unsigned profiles = 0, frames = 0;
  uint64_t appended = 0, visits = 0;
  CHECK(store);
  const char *packs[] = {"bk3_01", "bk3_02", "bk3_03", "bk3_04",
                         "bk3_05", "bk3_07", "bk3_16"};
  for (unsigned i = 0; i < 7; ++i) {
    CHECK(snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) <
          (int)sizeof(path));
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  CHECK(bk_resources_mount_directory(store, "routes", argv[1], 20480, error));
  CHECK(bk_resources_mount_directory(store, "collision", argv[1],
                                     BK_COLLISION_ATR_SIZE, error));
  CHECK(bk_resources_mount_directory(store, "fonts", argv[1], 16 * 1024 * 1024,
                                     error));
  text.store = store;
  if (!opening_mode) {
    CHECK(check_actor_entries(store, error));
    CHECK(check_resolved_returns(store, error));
  }
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned a = 0; a < 9; ++a) {
      BkEntryProgress progress;
      CHECK(bk_scene_game_frame_boot_state(&s, &progress, 1));
      BkEntryRequest request;
      uint32_t route_start;
      CHECK(bk_game_entry_resolve(&request, &route_start, &progress, g, a,
                                  opening_mode ? (a == 0   ? 8
                                                  : a == 1 ? 0x38
                                                           : 1)
                                               : 8));
      v.entry = bk_entry_assets_create(store, &request, error);
      CHECK(v.entry);
      uint32_t clocks[4] = {1000, 1000, 1000, 1000};
      CHECK(bk_scene_game_frame_initialize_entry(v.entry, &s, route_start,
                                                 clocks, error));
      assert(s.npc_actions.walk[1] == 0 && s.npc_actions.run[1] == 0);
      assert(s.npc.ai.point.action_wait.duration == 2000);
      assert(s.pickup.collected[1] == (g == 1));
      CHECK(bk_entry_assets_load_player_shadow(v.entry, store, error));
      CHECK(bk_entry_assets_load_mesh_shadow(v.entry, store, error));
      v.background = bk_background_assets_create(store, g, a, 1, 0, error);
      CHECK(v.background);
      const BkActorPose *bg = bk_background_assets_pose(v.background, 0);
      const BkModel *model = bk_actor_pose_model(bg);
      size_t floats = (size_t)model->frame_count * 16;
      float *world = malloc(floats * sizeof(float));
      CHECK(world);
      for (unsigned i = 0; i < model->frame_count; ++i)
        memcpy(world + 16 * i, bk_actor_pose_frame(bg, i), 64);
      v.props = bk_prop_assets_create(store, g, a, model, world, floats, error);
      free(world);
      CHECK(v.props);
      BkItemState retained[16] = {0};
      v.items = bk_item_assets_create(store, g, a, s.pickup.collected, retained,
                                      error);
      CHECK(v.items);
      forest = bk_entry_forest_create(v.entry, v.background, v.props, v.items,
                                      error);
      CHECK(forest);
      sink.submitted = 0;
      BkAudioSink output = {&sink, 48000, 240, 960, submit, poll};
      audio = bk_audio_create(&output, error);
      CHECK(audio);
      system = bk_system_audio_create(store, audio, 0, -600, error);
      CHECK(system);
      v.player_audio = bk_player_audio_create(store, audio, 1, error);
      CHECK(v.player_audio);
      v.npc_audio = bk_npc_audio_create(store, audio, 2, 3, error);
      CHECK(v.npc_audio);
      v.area_audio =
          bk_area_audio_create(store, audio, 4, v.player_audio, error);
      CHECK(v.area_audio);
      v.npc_events = bk_npc_event_audio_create(store, audio, 5, 6, -600,
                                               v.area_audio, system, error);
      CHECK(v.npc_events);
      v.item_feedback = bk_item_feedback_create(store, audio, 7, -600, error);
      CHECK(v.item_feedback);
      v.background_audio = bk_background_audio_create(
          store, audio, 8, bk_background_assets_config(v.background),
          &s.background, -600, error);
      CHECK(v.background_audio);
      v.prop_audio =
          bk_prop_audio_create(store, audio, 17, v.props, NULL, 0, -600, error);
      CHECK(v.prop_audio);
      if (hotkey_mode) {
        const unsigned slots[] = {0, 5, 7};
        for (unsigned i = 0; i < 3; ++i) {
          keys[i] = bk_system_audio_create_slot(store, audio, 32 + i, slots[i],
                                                -600, error);
          CHECK(keys[i]);
        }
        captures = (CaptureRequests){0};
        v.hotkeys = (BkPlayerHotkeyServices){keys[0], keys[1], keys[2],
                                             &captures, capture_request};
        s.hotkeys.photo_count = 98;
        for (unsigned i = 0; i < 5; ++i)
          s.hotkeys.photos[i] = (int)i + 20;
      }
      BkPlayerHudState player_hud = {0};
      CHECK(bk_player_hud_initialize(&player_hud, g, 0, 640));
      BkItemNoticeState notice = {0};
      BkPulseSprite prompt = {0};
      BkTextFlow flow = {0};
      BkOpeningServices opening = {
          store,     v.entry,
          &dialogue, v.item_feedback,
          NULL,      {&text, text_recreate, text_clear, text_bind}};
      if (opening_mode) {
        assert(17 + bk_prop_assets_count(v.props) <= 31);
        click = bk_system_audio_create_slot(store, audio, 31, 4, -600, error);
        CHECK(click);
        opening.click = click;
        CHECK(bk_item_notice_initialize(&notice, 0));
        bk_pulse_sprite_initialize(&prompt);
        CHECK(
            bk_opening_session_initialize(&opening, &s, &notice, &flow, error));
      }
      BkGameFrameInput input = {
          .seconds = 1.f / 60,
          .music_volume = -900,
          .effect_volume = -600,
          .interface_mode = 2,
          .player_scene = {.head_distance = 100,
                           .projected_depth = .5f,
                           .screen_scale = {.625f, .625f},
                           .screen_position = {320, 240}}};

      uint32_t static_meshes =
          bk_collision_count(bk_background_assets_collision(v.background));
      unsigned playable_frames = 0;
      if (opening_mode)
        input.seconds = .25f;
      for (unsigned step = 0; step < (opening_mode ? 4096u : 24u); ++step) {
        /* Explicit branch fixtures; no claim that missing51a190 performed
         * transitions. */
        if (!opening_mode) {
          s.camera.phase = step < 8 ? 0 : step < 12 ? 2 : step < 22 ? 1 : 3;
          if (step == 12)
            s.npc.ai.point.motion.behavior = 1;
        }
        input.now_ms = 1000 + step * (opening_mode ? 250 : 17);
        for (unsigned i = 0; i < 4; ++i)
          input.face_clocks[i] = input.now_ms;
        input.movement_buttons =
            (!opening_mode && step >= 12 && step < 20) ? BK_PLAYER_FORWARD : 0;
        const BkCameraFollowPose *camera =
            bk_follow_camera_pose(bk_entry_assets_camera(v.entry));
        memcpy(input.player_scene.wall.camera, camera->world + 12, 12);
        for (unsigned r = 0; r < 7; ++r)
          memcpy(input.player_scene.wall.rays[r], s.player_view.rays[r], 12);
        BkClipState before, after;
        CHECK(bk_actor_pose_state(bk_entry_assets_actor(v.entry), &before));
        CHECK(bk_audio_poll(audio, error));
        BkGameFrameResult result;
        if (hotkey_mode) {
          const unsigned buttons[] = {7, 2, 4, 4, 4, 0, 2, 2};
          input.hotkey_buttons =
              s.camera.phase == 1 ? buttons[playable_frames] : 7;
          input.special_mode = s.camera.phase == 1 && playable_frames == 3;
        }
        unsigned old_requests = captures.requests;
        uint8_t old_phase = s.camera.phase;
        CHECK(bk_scene_game_frame(&v, &s, &input, &result, error));
        if (hotkey_mode) {
          if (old_phase != 1)
            assert(captures.requests == old_requests);
          if (captures.requests != old_requests)
            assert(captures.group == g);
          assert(s.album_group == g);
        }
        assert((uint8_t)s.player.completion_requested == s.interaction.outcome);
        assert(result.count == (s.camera.phase == 1   ? 15
                                : s.camera.phase == 3 ? 2
                                                      : 11));
        assert(result.events[0] == BK_FRAME_COLLISION_BEGIN &&
               result.events[result.count - 1] == BK_FRAME_COLLISION_END);
        assert(result.static_meshes == static_meshes &&
               bk_collision_count(bk_background_assets_collision(
                   v.background)) == static_meshes);
        assert((uint8_t)s.npc.ai.point.motion.mode == s.camera.phase);
        CHECK(bk_actor_pose_state(bk_entry_assets_actor(v.entry), &after));
        if (s.camera.phase == 3)
          assert(!memcmp(&before, &after, sizeof(before)));
        appended += result.frame_meshes - static_meshes;
        const BkFrameVisit *walk;
        uint32_t count;
        CHECK(bk_entry_forest_draw(forest, 0, &walk, &count, error));
        visits += count;
        if (opening_mode) {
          uint8_t phase = s.camera.phase;
          BkItemNoticeFrame ui;
          if (phase == 1) {
            float view[16];
            const BkCameraLens lens = {1, .75f, .5f, 126384};
            const BkCameraFollowPose *active =
                bk_follow_camera_pose(bk_entry_assets_camera(v.entry));
            CHECK(bk_camera_view(view, active->world));
            BkPlayerHudFrame hud_frame;
            int unprojectable;
            CHECK(bk_player_hud_session_step(
                &player_hud, &s, input.special_mode, view, &lens, 640, 480,
                input.seconds, input.now_ms, &hud_frame, &unprojectable,
                error));
            assert(input.special_mode == 1
                       ? !hud_frame.capture
                       : (hud_frame.capture && hud_frame.capture_after == 4));
            CHECK(bk_item_notice_step(&notice, &s.pickup, &flow, input.seconds,
                                      input.now_ms, &ui));
            if (ui.bind_message)
              CHECK(text_bind(&text, bk_item_feedback_message(v.item_feedback),
                              error));
            playable_frames++;
          } else {
            CHECK(bk_opening_session_step(&opening, &s, &notice, &flow,
                                          step % 4 == 3, error));
            CHECK(bk_opening_notice_step(&notice, &prompt, phase,
                                         s.pickup.notice_visible, input.seconds,
                                         &ui));
            if (s.camera.phase == 1) {
              handovers++;
              assert(s.camera.stage == 0);
              if (phase == 0)
                assert(!dialogue.raw.data && !dialogue.state.text.length);
            }
          }
          if (ui.draw_text)
            CHECK(text_prepare(&text, &flow, input.seconds, error));
        }
        CHECK(bk_audio_fill(audio, error));
        frames++;
        if (opening_mode && playable_frames == 8)
          break;
      }
      if (opening_mode) {
        if (playable_frames != 8) {
          snprintf(error, 256, "opening stuck g%u a%u phase%u stage%d", g, a,
                   s.camera.phase, s.camera.stage);
          goto done;
        }
        printf("opening g%u a%u reached play; total frames%u\n", g, a, frames);
        fflush(stdout);
      }
      if (hotkey_mode) {
        assert(captures.requests == 3 && s.hotkeys.menu_request == 1 &&
               s.hotkeys.photo_count == 100);
        assert(s.hotkeys.photos[g] == 100 && s.camera.transition == 0);
        for (unsigned i = 0; i < 5; ++i)
          if (i != g)
            assert(s.hotkeys.photos[i] == (int)i + 20);
        hotkey_requests += captures.requests;
      }
      for (unsigned i = 0; i < 3; ++i) {
        bk_system_audio_destroy(keys[i]);
        keys[i] = NULL;
      }
      bk_system_audio_destroy(click);
      click = NULL;
      text_clear(&text, error);
      bk_dialogue_assets_close(&dialogue);
      /* A failed reached player request must still clean the dynamic suffix. */
      s.camera.phase = 0;
      s.player_actions[0] = 128;
      BkGameFrameResult failed;
      assert(!bk_scene_game_frame(&v, &s, &input, &failed, error));
      assert(bk_collision_count(bk_background_assets_collision(v.background)) ==
             static_meshes);
      bk_entry_forest_destroy(forest);
      forest = NULL;
      bk_npc_event_audio_destroy(v.npc_events);
      bk_system_audio_destroy(system);
      system = NULL;
      bk_area_audio_destroy(v.area_audio);
      bk_player_audio_destroy(v.player_audio);
      bk_npc_audio_destroy(v.npc_audio);
      bk_background_audio_destroy(v.background_audio);
      bk_prop_audio_destroy(v.prop_audio);
      bk_item_feedback_destroy(v.item_feedback);
      bk_audio_destroy(audio);
      audio = NULL;
      bk_item_assets_destroy(v.items);
      bk_prop_assets_destroy(v.props);
      bk_background_assets_destroy(v.background);
      bk_entry_assets_destroy(v.entry);
      v = (BkGameFrameServices){0};
      profiles++;
    }
  printf("PASS game frame profiles=%u frames=%u "
         "dynamic-meshes=%" PRIu64 " publication-visits=%" PRIu64
         " samples=%" PRIu64 " nonzero=%" PRIu64 " hash=%016" PRIx64 "\n",
         profiles, frames, appended, visits, sink.samples, sink.nonzero,
         sink.hash);
  if (opening_mode)
    printf("PASS opening handovers=%u text-uploads=%u\n", handovers,
           text.uploads);
  if (hotkey_mode)
    printf("PASS game hotkeys profiles=%u requests=%u (CPU capture boundary)\n",
           profiles, hotkey_requests);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL profile%u frame%u: %s\n", profiles, frames, error);
  for (unsigned i = 0; i < 3; ++i)
    bk_system_audio_destroy(keys[i]);
  bk_system_audio_destroy(click);
  text_clear(&text, error);
  bk_dialogue_assets_close(&dialogue);
  bk_entry_forest_destroy(forest);
  bk_npc_event_audio_destroy(v.npc_events);
  bk_system_audio_destroy(system);
  bk_area_audio_destroy(v.area_audio);
  bk_player_audio_destroy(v.player_audio);
  bk_npc_audio_destroy(v.npc_audio);
  bk_background_audio_destroy(v.background_audio);
  bk_prop_audio_destroy(v.prop_audio);
  bk_item_feedback_destroy(v.item_feedback);
  bk_audio_destroy(audio);
  bk_item_assets_destroy(v.items);
  bk_prop_assets_destroy(v.props);
  bk_background_assets_destroy(v.background);
  bk_entry_assets_destroy(v.entry);
  bk_resources_destroy(store);
  return rc;
}
