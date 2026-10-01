#include "app/application.h"
#include "app/pause_preview.h"
#include "app/play_session.h"
#include "build_info.h"
#include "core/game_clock.h"
#include "platform/audio_output.h"
#include "platform/platform.h"
#include "save/capture_file.h"
#include "scene/fps_overlay.h"
#include "scene/control_help.h"
#include "scene/ending_normal_session.h"
#include "scene/scene.h"
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static BkScene *load_scene(BkSceneKind kind, const BkSceneServices *services,
                           BkCheckpointFiles *save_files,
                           BkUnlockFile *unlock_file, BkRecordFile *record_file, BkVolumeFile *volume_file, const char *root,
                           unsigned *mounted, char error[256]) {
  unsigned required = kind == BK_SCENE_TITLE_PREVIEW   ? 1u
                      : kind == BK_SCENE_CAMERA_TRACK  ? 6u
                      : kind == BK_SCENE_ACTOR_PREVIEW ? 12u
                      : kind == BK_SCENE_PAUSE_PREVIEW ? 1u
                      : kind == BK_SCENE_GAME          ? 15u
                      : kind == BK_SCENE_ENDING_PREVIEW ? 0u
                                                       : 2u;
  const char *packs[] = {"bk3_00", "bk3_03", "bk3_04", "bk3_01"};
  for (unsigned i = 0; i < 4; i++)
    if ((required & (1u << i)) && !(*mounted & (1u << i))) {
      char path[1024];
      if (snprintf(path, sizeof(path), "%s/Data/%s.pp", root, packs[i]) >=
          (int)sizeof(path)) {
        snprintf(error, 256, "game path too long");
        return NULL;
      }
      if (!bk_resources_mount(services->resources, packs[i], path, error))
        return NULL;
      *mounted |= 1u << i;
    }
  if (kind == BK_SCENE_PAUSE_PREVIEW && !(*mounted & (1u << 6))) {
    char path[1024];
    if (snprintf(path, sizeof(path), "%s/Data/bk3_02.pp", root) >=
            (int)sizeof(path) ||
        !bk_resources_mount(services->resources, "bk3_02", path, error))
      return NULL;
    *mounted |= 1u << 6;
  }
  if (kind == BK_SCENE_GAME) {
    const char *extra[] = {"bk3_02", "bk3_05", "bk3_07", "bk3_16",
                           "bk3_15", "bk3_06", "bk3_18", "bk3_20"};
    const unsigned bits[] = {6, 8, 9, 10, 11, 7, 14, 15};
    for (unsigned i = 0; i < sizeof(extra) / sizeof(extra[0]); ++i) {
      if (*mounted & (1u << bits[i]))
        continue;
      char path[1024];
      if (snprintf(path, sizeof(path), "%s/Data/%s.pp", root, extra[i]) >=
              (int)sizeof(path) ||
          !bk_resources_mount(services->resources, extra[i], path, error))
        return NULL;
      *mounted |= 1u << bits[i];
    }
    /* Flow16 is reached by the real dialogue session without rebuilding the
     * application resource store. Mount all normal-ending dependencies while
     * the persistent game session is created, using disjoint mount bits. */
    const char *ending[] = {"bk3_08", "bk3_09", "bk3_10", "bk3_11",
                            "bk3_12", "bk3_13", "fambom", "bk3_14"};
    const unsigned ending_bits[] = {16, 17, 18, 19, 20, 21, 22, 23};
    for (unsigned i = 0; i < sizeof(ending) / sizeof(*ending); ++i) {
      if (*mounted & (1u << ending_bits[i]))
        continue;
      char path[1024];
      if (snprintf(path, sizeof(path), "%s/Data/%s.pp", root, ending[i]) >=
              (int)sizeof(path) ||
          !bk_resources_mount(services->resources, ending[i], path, error))
        return NULL;
      *mounted |= 1u << ending_bits[i];
    }
    const char *loose[] = {"routes", "faces", "collision", "fonts"};
    const unsigned loose_bits[] = {4, 5, 12, 13};
    const size_t limits[] = {20480, 20480, 16 * 1024 * 1024, 16 * 1024 * 1024};
    char loose_root[1024];
    if (snprintf(loose_root, sizeof(loose_root), "%s/Data", root) >=
        (int)sizeof(loose_root))
      return NULL;
    for (unsigned i = 0; i < 4; ++i) {
      if (*mounted & (1u << loose_bits[i]))
        continue;
      if (!bk_resources_mount_directory(services->resources, loose[i],
                                        loose_root, limits[i], error))
        return NULL;
      *mounted |= 1u << loose_bits[i];
    }
  }
  if (kind == BK_SCENE_ENDING_PREVIEW) {
    const char *ending[] = {"bk3_02", "bk3_03", "bk3_04", "bk3_08",
                            "bk3_09", "bk3_10", "bk3_11", "bk3_12",
                            "bk3_13", "bk3_18", "fambom"};
    const unsigned bits[] = {6, 1, 2, 16, 17, 18, 19, 20, 21, 14, 22};
    for (unsigned i = 0; i < sizeof(ending) / sizeof(*ending); ++i) {
      if (*mounted & (1u << bits[i]))
        continue;
      char path[1024];
      if (snprintf(path, sizeof(path), "%s/Data/%s.pp", root, ending[i]) >=
              (int)sizeof(path) ||
          !bk_resources_mount(services->resources, ending[i], path, error))
        return NULL;
      *mounted |= 1u << bits[i];
    }
    return bk_ending_normal_scene_create(services, 0, 0, error);
  }
  if (kind == BK_SCENE_ACTOR_PREVIEW) {
    const char *audio_packs[] = {"bk3_02", "bk3_06"};
    for (unsigned i = 0; services->audio && i < 2; i++)
      if (!(*mounted & (1u << (i + 6)))) {
        char path[1024];
        if (snprintf(path, sizeof(path), "%s/Data/%s.pp", root,
                     audio_packs[i]) >= (int)sizeof(path)) {
          snprintf(error, 256, "audio path too long");
          return NULL;
        }
        if (!bk_resources_mount(services->resources, audio_packs[i], path,
                                error))
          return NULL;
        *mounted |= 1u << (i + 6);
      }
    const char *loose[] = {"routes", "faces"};
    for (unsigned i = 0; i < 2; i++)
      if (!(*mounted & (1u << (i + 4)))) {
        char path[1024];
        if (snprintf(path, sizeof(path), "%s/Data", root) >=
            (int)sizeof(path)) {
          snprintf(error, 256, "game path too long");
          return NULL;
        }
        if (!bk_resources_mount_directory(services->resources, loose[i], path,
                                          20480, error))
          return NULL;
        *mounted |= 1u << (i + 4);
      }
  }
  if (kind == BK_SCENE_PAUSE_PREVIEW)
    return bk_pause_preview_create(services, error);
  if (kind == BK_SCENE_GAME) {
    const char *development = getenv("BK_DEVELOPMENT_ENTRY");
    if (development && !strcmp(development, "1"))
      return bk_play_session_create_development(services, save_files, error);
    return bk_play_session_create_with_progress(services, save_files,
                                               unlock_file, record_file, volume_file, error);
  }
  return bk_scene_create(kind, services, error);
}
static int capture(BkRenderer *renderer, const char *path, char error[256]) {
  size_t size = 1280 * 720 * 4;
  uint8_t *pixels = malloc(size);
  if (!pixels) {
    snprintf(error, 256, "capture allocation failed");
    return 0;
  }
  if (!bk_renderer_readback(renderer, pixels, size, error)) {
    free(pixels);
    return 0;
  }
  FILE *file = fopen(path, "wb");
  if (!file) {
    free(pixels);
    snprintf(error, 256, "cannot open capture output");
    return 0;
  }
  int written = fwrite(pixels, 1, size, file) == size;
  int closed = fclose(file) == 0;
  free(pixels);
  if (!written || !closed)
    snprintf(error, 256, "capture write failed");
  return written && closed;
}
/* Explicit offscreen host sink. Native Switch always uses audout. */
static int offline_submit(void *context, const int16_t *samples, size_t frames,
                          char error[256]) {
  (void)samples;
  (void)error;
  *(uint64_t *)context += frames;
  return 1;
}
static int offline_poll(void *context, uint64_t *consumed, char error[256]) {
  (void)error;
  *consumed = *(uint64_t *)context;
  return 1;
}
static BkControlHelpPage help_page(BkSceneKind kind, BkScene *scene) {
  if (kind == BK_SCENE_ENDING_PREVIEW) return BK_HELP_ENDING;
  if (kind == BK_SCENE_PAUSE_PREVIEW) return BK_HELP_PAUSE;
  if (kind == BK_SCENE_TITLE_PREVIEW) return BK_HELP_TITLE;
  if (kind != BK_SCENE_GAME) return BK_HELP_INSPECTION;
  switch (bk_play_session_displayed_flow(scene)) {
  case 1: return BK_HELP_TITLE;
  case 2: return BK_HELP_GAME;
  case 4: return BK_HELP_PAUSE;
  case 8: return BK_HELP_DIALOGUE;
  case 0x10: return BK_HELP_ENDING;
  case 0x18: return BK_HELP_GALLERY;
  case 0x20: case 0x68: return BK_HELP_CHOICE;
  case 0x28: return BK_HELP_SAVE;
  case 0x30: return BK_HELP_VOLUME;
  case 0x38: return BK_HELP_SELECTION;
  case 0x40: return BK_HELP_FAILURE;
  case 0x48: return BK_HELP_SPECIAL;
  case 0x50: return BK_HELP_LOADING;
  default: return BK_HELP_NONE;
  }
}
int bk_application_run(int argc, char **argv) {
  char error[256] = {0};
  BkLaunchConfig config = {0};
  BkPlatform *platform = bk_platform_open(argc, argv, &config, error);
  BkResourceStore *resources = NULL;
  BkRenderer *renderer = NULL;
  BkScene *scene = NULL;
  BkAudio *audio = NULL;
  BkFpsOverlay *fps_overlay = NULL;
  BkControlHelp *control_help = NULL;
  BkSceneKind kind = BK_SCENE_TITLE_PREVIEW;
  uint64_t offline_submitted = 0;
  BkAudioOutput *audio_output = NULL;
  BkCaptureFiles *capture_files = NULL;
  BkCheckpointFiles *save_files = NULL;
  BkUnlockFile *unlock_file = NULL;
  BkRecordFile *record_file = NULL;
  BkVolumeFile *volume_file = NULL;
  int result = 1;
  if (!platform)
    goto done;
  FILE *log = bk_platform_log(platform);
  fprintf(log,
          "Biko3 development preview %s\nMesa commit %s\n"
          "Development build; full mission/save/media flow is incomplete.\n",
          BK_VERSION, BK_MESA_COMMIT);
  fflush(log);
  resources = bk_resources_create(error);
  if (!resources)
    goto done;
  renderer = bk_renderer_create(1280, 720, log, error);
  if (!renderer)
    goto done;
  BkAudioSink sink;
  if (config.audio_device) {
    audio_output = bk_audio_output_open(&sink, log, error);
    if (!audio_output)
      goto done;
  } else {
    sink = (BkAudioSink){&offline_submitted, 48000,       240, 960,
                         offline_submit,     offline_poll};
    fprintf(log, "Audio: explicit offline host sink\n");
  }
  audio = bk_audio_create(&sink, error);
  if (!audio)
    goto done;
  if (audio_output && !bk_audio_output_start(audio_output, audio, error))
    goto done;
  if (config.capture_root) {
    capture_files = bk_capture_files_create(config.capture_root, error);
    if (!capture_files)
      goto done;
    save_files = bk_checkpoint_files_create(config.capture_root, error);
    if (!save_files)
      goto done;
    unlock_file = bk_unlock_file_create(config.capture_root, error);
    if (!unlock_file)
      goto done;
    record_file = bk_record_file_create(config.capture_root, error);
    if (!record_file)
      goto done;
  }
  volume_file = bk_volume_file_create(config.capture_root,config.game_root,error);
  if (!volume_file)goto done;
  BkSceneServices services = {resources, renderer, log, audio, capture_files, bk_volume_file_values(volume_file)};
  kind = !strcmp(config.scene, "camera-track")
                         ? BK_SCENE_CAMERA_TRACK
                     : !strcmp(config.scene, "actor")  ? BK_SCENE_ACTOR_PREVIEW
                     : !strcmp(config.scene, "pause")  ? BK_SCENE_PAUSE_PREVIEW
                     : !strcmp(config.scene, "game")   ? BK_SCENE_GAME
                     : !strcmp(config.scene, "ending") ? BK_SCENE_ENDING_PREVIEW
                     : !strcmp(config.scene, "office") ? BK_SCENE_STATIC_WORLD
                                                       : BK_SCENE_TITLE_PREVIEW;
  unsigned mounted = 0;
  scene = load_scene(kind, &services, save_files, unlock_file, record_file, volume_file, config.game_root,
                     &mounted, error);
  if (!scene)
    goto done;
  control_help = bk_control_help_create(renderer, error);
  if (!control_help)
    goto done;
  if (config.show_fps) {
    fps_overlay = bk_fps_overlay_create(renderer, error);
    if (!fps_overlay)
      goto done;
    fprintf(log, "Original FPS counter enabled (portable bitmap glyphs).\n");
  }
  BkGameClock game_clock = {0};
  double clock_origin = bk_platform_seconds(platform);
  /* The session constructor prepares its first snapshot and advances its
   * clock. Anchor subsequent wall time after that snapshot; a fast initial
   * presentation may otherwise put the first live tick before it. */
  double session_wall_base =
      kind == BK_SCENE_GAME ? bk_play_session_wall_seconds(scene) : 1;
  BkClock clock;
  bk_clock_init(&clock);
  BkInput sample, pending = {0};
  uint64_t frames = 0, window_frames = 0;
  double profile_start = bk_platform_performance_seconds(platform);
  double update_sum = 0, draw_sum = 0, end_sum = 0, tail_sum = 0;
  double max_frame = 0, simulated = 0;
  BkRenderStats previous_render = bk_renderer_stats(renderer);
  BkAudioStats previous_audio = bk_audio_stats(audio);
  fprintf(
      log,
      "Timing: original4adc16 sample/hold;220ms game-step cap; "
      "independent wall timers and audio. Rate fix2. GPU skin3. "
      "Save/car diagnostics4. Original front end5. Weather integration6.\n");
  if (audio_output)
    fprintf(log, "Audio pump: independent thread, priority0x2b,2ms poll; "
                 "cache-flushed PCM.\n");
  while (bk_platform_poll(platform, &sample)) {
    if ((sample.pressed & BK_BUTTON_PAUSE) && kind != BK_SCENE_GAME &&
        kind != BK_SCENE_PAUSE_PREVIEW)
      break;
    if ((audio_output && !bk_audio_output_check(audio_output, error)) ||
        (!audio_output && audio && !bk_audio_poll(audio, error)))
      goto done;
    double frame_start = bk_platform_performance_seconds(platform);
    bk_input_latch(&pending, &sample);
    double wall = bk_platform_seconds(platform) - clock_origin;
    if (!isfinite(wall) || wall < 0 || wall > 1e12) {
      snprintf(error, 256, "invalid application monotonic clock");
      goto done;
    }
    uint32_t now_ms = (uint32_t)(uint64_t)(wall * 1000);
    bk_game_clock_poll(&game_clock, now_ms, now_ms);
    BkSceneFrame frame = {
        .clock = kind == BK_SCENE_GAME
                     ? (BkClockFrame){game_clock.seconds > 0 ? 1u : 0u,
                                      game_clock.seconds, 0}
                     : bk_clock_advance(&clock, bk_platform_seconds(platform)),
        .input = pending};
    for (unsigned i = 0; i < frame.clock.steps; i++) {
      BkInput tick_input = bk_input_consume(&pending);
      BkSceneKind requested = kind;
      if (kind == BK_SCENE_TITLE_PREVIEW &&
          (tick_input.pressed & BK_BUTTON_CONFIRM))
        requested = BK_SCENE_GAME;
      if (kind != BK_SCENE_TITLE_PREVIEW && kind != BK_SCENE_GAME &&
          kind != BK_SCENE_PAUSE_PREVIEW &&
          (tick_input.pressed & BK_BUTTON_BACK))
        requested = BK_SCENE_TITLE_PREVIEW;
      if (kind != BK_SCENE_TITLE_PREVIEW && kind != BK_SCENE_GAME &&
          kind != BK_SCENE_PAUSE_PREVIEW &&
          !(tick_input.pressed & BK_BUTTON_BACK) &&
          (tick_input.pressed & BK_BUTTON_CAMERA_TRACK))
        requested = kind == BK_SCENE_CAMERA_TRACK ? BK_SCENE_STATIC_WORLD
                                                  : BK_SCENE_CAMERA_TRACK;
      if (kind != BK_SCENE_TITLE_PREVIEW && kind != BK_SCENE_GAME &&
          kind != BK_SCENE_PAUSE_PREVIEW &&
          !(tick_input.pressed & (BK_BUTTON_BACK | BK_BUTTON_CAMERA_TRACK)) &&
          (tick_input.pressed & BK_BUTTON_ACTOR_PREVIEW))
        requested = kind == BK_SCENE_ACTOR_PREVIEW ? BK_SCENE_STATIC_WORLD
                                                   : BK_SCENE_ACTOR_PREVIEW;
      if (requested != kind) {
        if (audio)
          for (unsigned voice = 0; voice < BK_AUDIO_VOICES; voice++)
            if (!bk_audio_clear(audio, voice, error)) {
              goto done;
            }
        BkScene *next =
            load_scene(requested, &services, save_files, unlock_file, record_file, volume_file,
                       config.game_root, &mounted, error);
        if (!next)
          goto done;
        bk_scene_destroy(scene);
        scene = next;
        kind = requested;
        if (kind == BK_SCENE_GAME)
          session_wall_base = bk_play_session_wall_seconds(scene);
        pending = (BkInput){0};
        bk_clock_init(&clock);
        fprintf(log, "Scene switched: %s\n",
                kind == BK_SCENE_STATIC_WORLD    ? "office"
                : kind == BK_SCENE_CAMERA_TRACK  ? "camera-track"
                : kind == BK_SCENE_ACTOR_PREVIEW ? "actor"
                : kind == BK_SCENE_GAME          ? "game"
                : kind == BK_SCENE_ENDING_PREVIEW ? "ending"
                                                 : "title");
        fflush(log);
        break;
      }
      simulated += frame.clock.step_seconds;
      int stepped = kind == BK_SCENE_GAME
                        ? bk_play_session_step_at(
                              scene, frame.clock.step_seconds,
                              wall + session_wall_base, &tick_input, error)
                        : bk_scene_step(scene, frame.clock.step_seconds,
                                        &tick_input, error);
      if (!stepped)
        goto done;
      if (kind == BK_SCENE_GAME)
        ++clock.ticks;
      /* One native game step per presentation, with independently supplied
       * wall time. Never add catch-up presentations. */
      if (kind == BK_SCENE_PAUSE_PREVIEW &&
          bk_pause_preview_finished_context(bk_scene_custom_context(scene)))
        break;
    }
    double after_update = bk_platform_performance_seconds(platform);
    if ((!audio_output && audio && !bk_audio_fill(audio, error)) ||
        !bk_renderer_begin(renderer, error) ||
        !bk_scene_draw(scene, &frame, error) ||
        !bk_control_help_draw(control_help, help_page(kind, scene), error) ||
        (fps_overlay &&
         !bk_fps_overlay_draw(fps_overlay, game_clock.fps,
                              kind != BK_SCENE_GAME ||
                                  bk_play_session_flow(scene)->current == 1,
                              error)))
      goto done;
    double after_draw = bk_platform_performance_seconds(platform);
    if (!bk_renderer_end(renderer, error))
      goto done;
    double after_end = bk_platform_performance_seconds(platform);
    frames++;
    if (kind == BK_SCENE_GAME && !bk_play_session_after_present(scene, error))
      goto done;
    if (kind == BK_SCENE_ENDING_PREVIEW &&
        !bk_ending_normal_scene_after_present(scene, error))
      goto done;
    double frame_end = bk_platform_performance_seconds(platform);
    update_sum += after_update - frame_start;
    draw_sum += after_draw - after_update;
    end_sum += after_end - after_draw;
    tail_sum += frame_end - after_end;
    if (frame_end - frame_start > max_frame)
      max_frame = frame_end - frame_start;
    window_frames++;
    if (frame_end - profile_start >= 2) {
      BkRenderStats render = bk_renderer_stats(renderer);
      BkAudioStats sound = bk_audio_stats(audio);
      double ms = 1000.0 / window_frames;
      fprintf(
          log,
          "PERF %.1ffps game/wall=%.3f update=%.2f draw=%.2f end=%.2f "
          "tail=%.2f "
          "max=%.2fms "
          "fence=%.2f acquire=%.2f present=%.2fms draws=%.1f uploads=%.2fMB "
          "skin=%.1fdispatch/%.0fvertices cpu_work=%.2fms "
          "audio_queue=%.1fms drains=%" PRIu64 " clamp_total=%.3fs flow=%02x\n",
          window_frames / (frame_end - profile_start),
          simulated / (frame_end - profile_start), update_sum * ms,
          draw_sum * ms, end_sum * ms, tail_sum * ms, max_frame * 1000,
          (render.fence_seconds - previous_render.fence_seconds) * ms,
          (render.acquire_seconds - previous_render.acquire_seconds) * ms,
          (render.present_seconds - previous_render.present_seconds) * ms,
          (double)(render.draws - previous_render.draws) / window_frames,
          (render.uploaded_bytes - previous_render.uploaded_bytes) /
              (1048576.0 * window_frames),
          (double)(render.skin_dispatches - previous_render.skin_dispatches) /
              window_frames,
          (double)(render.skin_vertices - previous_render.skin_vertices) /
              window_frames,
          (update_sum + draw_sum + end_sum + tail_sum -
           (render.fence_seconds - previous_render.fence_seconds) -
           (render.acquire_seconds - previous_render.acquire_seconds) -
           (render.present_seconds - previous_render.present_seconds)) *
              ms,
          (sound.submitted - sound.consumed) / 48.0,
          sound.queue_drains - previous_audio.queue_drains,
          kind == BK_SCENE_GAME ? game_clock.clamped_seconds
                                : clock.dropped_seconds,
          kind == BK_SCENE_GAME ? bk_play_session_flow(scene)->current : 0xff);
      if (kind == BK_SCENE_GAME)
        bk_play_session_log_state(scene, log);
      fflush(log);
      profile_start = frame_end;
      window_frames = 0;
      simulated = 0;
      update_sum = draw_sum = end_sum = tail_sum = max_frame = 0;
      previous_render = render;
      previous_audio = sound;
    }
    if (kind == BK_SCENE_GAME && bk_play_session_finished(scene))
      break;
    if (kind == BK_SCENE_PAUSE_PREVIEW &&
        bk_pause_preview_finished_context(bk_scene_custom_context(scene)))
      break; /* Standalone diagnostic has no enclosing session. */
  }
  if (config.capture_path && !capture(renderer, config.capture_path, error))
    goto done;
  if (kind == BK_SCENE_GAME && scene) {
    const BkGameFrameState *state = bk_play_session_state(scene);
    fprintf(log, "Game state: group%u area%u phase%u photos%d menu%u\n",
            state->group, state->area, state->camera.phase,
            state->hotkeys.photo_count, state->hotkeys.menu_request);
    fprintf(log, "Active flow: 0x%02x\n", bk_play_session_flow(scene)->current);
  }
  if (audio) {
    BkAudioStats stats = bk_audio_stats(audio);
    fprintf(log,
            "Audio frames submitted: %" PRIu64
            "; consumed lower bound: %" PRIu64
            "; observed queue drains: %" PRIu64 "\n",
            stats.submitted, stats.consumed, stats.queue_drains);
  }
  fprintf(log,
          "Frames: %" PRIu64 "; clock ticks: %" PRIu64
          "; dropped seconds: %.6f\n",
          frames, clock.ticks, clock.dropped_seconds);
  fflush(log);
  result = 0;
done:
  /* Flush the original error before cleanup: a driver/device failure during
   * teardown must not erase the useful cause from the SD log. */
  if (result && platform) {
    fprintf(bk_platform_log(platform), "FAILED before cleanup: %s\n", error);
    if (kind == BK_SCENE_GAME && scene)
      bk_play_session_log_state(scene, bk_platform_log(platform));
    fflush(bk_platform_log(platform));
  }
  /* Reverse lifetime order also handles partially constructed sessions. */
  bk_audio_output_stop(audio_output);
  bk_fps_overlay_destroy(fps_overlay);
  bk_control_help_destroy(control_help);
  bk_scene_destroy(scene);
  bk_audio_destroy(audio);
  bk_audio_output_close(audio_output);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(resources);
  bk_capture_files_destroy(capture_files);
  bk_checkpoint_files_destroy(save_files);
  bk_unlock_file_destroy(unlock_file);
  bk_record_file_destroy(record_file);
  bk_volume_file_destroy(volume_file);
  if (result)
    bk_platform_report_error(platform, error);
  bk_platform_close(platform);
  return result;
}
