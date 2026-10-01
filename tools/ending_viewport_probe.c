/* Application-owner integration probe, including its private CPU snapshots.
 * Like ending-exit-probe, this deliberately shares the real implementation;
 * no test-only setters are added to the production scene API. */
#include "render/renderer.h"
static int zero_origin;
/* Explicit raster diagnostic: preserve logical scene/input coordinates and
 * framebuffer extent, but place the single regular view at GPU origin zero.
 * This does not change production rendering or pretend to cover dual views. */
static int probe_viewport(BkRenderer *renderer, const BkViewport *viewport,
                           char error[256]) {
  if (zero_origin && viewport) {
    BkViewport moved = *viewport;
    moved.x = moved.y = 0;
    return bk_renderer_viewport(renderer, &moved, error);
  }
  return bk_renderer_viewport(renderer, viewport, error);
}
#define bk_renderer_viewport probe_viewport
#include "../runtime/scene/ending_normal_session.c"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line%d: %s\n", __LINE__, e); goto done; } } while (0)
typedef struct { uint64_t submitted; } Sink;
typedef struct {
  uint8_t *pixels, *interface_pixels;
  size_t size;
  unsigned width, height, frames, rectangles;
  unsigned draw_checks, modal_movie_changes, empty_draws;
  uint64_t states, pointer_states, frame_states, geometry_states;
} Capture;
static uint64_t digest(uint64_t h, const void *data, size_t size) {
  const uint8_t *p = data;
  for (size_t i = 0; i < size; ++i)
    h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int submit(void *ctx, const int16_t *pcm, size_t n, char e[256]) {
  (void)pcm; (void)e;
  ((Sink *)ctx)->submitted += n;
  return 1;
}
static int poll(void *ctx, uint64_t *n, char e[256]) {
  (void)e; *n = ((Sink *)ctx)->submitted; return 1;
}
static int run(const char *data, unsigned width, unsigned height,
               unsigned variant, int story, Capture *out) {
  char e[256] = {0}, path[1024];
  int ok = 0;
  BkRenderer *renderer = NULL;
  BkResourceStore *store = NULL;
  BkAudio *audio = NULL;
  BkScene *scene = NULL;
  BkEndingRecords *records = NULL;
  uint8_t *pixels = NULL, *clear = NULL;
  Sink sink = {0};
  size_t bytes = (size_t)width * height * 4;
  *out = (Capture){.states = UINT64_C(14695981039346656037),
                   .pointer_states = UINT64_C(14695981039346656037),
                   .frame_states = UINT64_C(14695981039346656037),
                   .geometry_states = UINT64_C(14695981039346656037)};
  CHECK(renderer = bk_renderer_create(width, height, stdout, e));
  CHECK(store = bk_resources_create(e));
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04",
                          "bk3_06", "bk3_08", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", data, packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  BkAudioSink output = {&sink, 48000, 240, 960, submit, poll};
  CHECK(audio = bk_audio_create(&output, e));
  BkSceneServices services = {store, renderer, NULL, audio, NULL, NULL};
  BkRenderStats baseline = bk_renderer_stats(renderer);
  CHECK(pixels = malloc(bytes));
  CHECK(clear = malloc(bytes));
  CHECK(bk_renderer_begin(renderer, e) && bk_renderer_end(renderer, e));
  CHECK(bk_renderer_readback(renderer, clear, bytes, e));
  if (story) {
    CHECK(records = calloc(1, sizeof(*records)));
    const uint8_t unlocked[5][8] = {{0}};
    scene = bk_ending_normal_scene_create_story(&services, 0, 0, records,
                                               unlocked, NULL, e);
  } else {
    scene = bk_ending_normal_scene_create(&services, 0, variant, e);
  }
  CHECK(scene);
  EndingNormalScene *s = bk_scene_custom_context(scene);
  /* Action-table variants are both normal phase1, not gallery selections. */
  CHECK(bk_ending_normal_render_pass_count(s->render) == 3u);
  ++out->draw_checks;
  BkViewport viewport;
  CHECK(bk_camera_fit(&viewport, width, height, 4, 3));
  CHECK(!memcmp(&s->viewport, &viewport, sizeof(viewport)));
  BkViewport raster = viewport;
  if (zero_origin) raster.x = raster.y = 0;
  float scale = viewport.width / 1280.f;
  CHECK(s->ui.sprites[52].rect[2] == viewport.width);
  CHECK(s->ui.sprites[52].rect[3] == viewport.height);
  CHECK(s->pointer.position[0] == viewport.width * .5f);
  CHECK(s->pointer.position[1] == viewport.height * .5f);
  /* A point above the camera center must project above the raster center.
   * Vulkan's viewport formula has positive height; the projection flips Y. */
  float clip_y = s->ui_projection[5] / 10.f;
  float raster_y = (clip_y + 1.f) * viewport.height * .5f;
  float pick_y = clip_y * s->ui_viewport[5] + s->ui_viewport[13];
  CHECK(fabsf(raster_y - pick_y) < .001f && pick_y < viewport.height * .5f);
  out->width = viewport.width;
  out->height = viewport.height;
  out->size = (size_t)viewport.width * viewport.height * 4 * 3;
  CHECK(out->pixels = malloc(out->size));
  CHECK(out->interface_pixels = malloc(out->size));
  unsigned captures = 0;
  BkSceneFrame frame = {{0, 0, 0}, {0}};
  CHECK(bk_renderer_begin(renderer, e) && bk_scene_draw(scene, &frame, e) &&
         bk_renderer_end(renderer, e) && bk_ending_normal_scene_after_present(scene, e));
  float stick_start[2];
  memcpy(stick_start, s->pointer.position, sizeof(stick_start));
  unsigned total = story ? 163 : 129;
  for (unsigned step = 0; step < total; ++step) {
    uint32_t previous_movie = bk_ending_normal_render_movie_frame(s->render);
    BkInput input = {0};
    if (step < 24)
      input.move_x = .8f;
    else if (step < 48)
      input.move_y = .6f;
    else if (step < 129) {
      input.pointer_active = 1;
      input.pointer_x = viewport.x + 1200 * scale;
      input.pointer_y = viewport.y + 730 * scale;
      if (story && step == 128)
        input.pressed = input.held = BK_BUTTON_CONFIRM;
    } else if (step == 161)
      input.pressed = BK_BUTTON_BACK;
    CHECK(bk_audio_poll(audio, e));
    CHECK(bk_scene_step(scene, 1.0 / 60, &input, e));
    CHECK(bk_audio_fill(audio, e));
    /*4d7ac4 consumes toolbar input before scene preparation: step128
     * already uses phase9's roots. Cancel at161 restores phase1 before
     * preparation as well. Check actual prepared GPU passes. */
    int modal_draw = story && step >= 128 && step <= 160;
    unsigned passes = bk_ending_normal_render_pass_count(s->render);
    unsigned expected_passes = modal_draw ? 2u : 3u;
    if (passes != expected_passes)
      snprintf(e, sizeof(e), "step%u story%d variant%u phase%d: GPU passes%u expected%u",
                 step, story, variant, s->state->frame.phase, passes, expected_passes);
    CHECK(passes == expected_passes);
    if (zero_origin)
      for (unsigned pass = 0; pass < passes; ++pass)
        CHECK(bk_ending_normal_render_pass_view(s->render, pass) == 0);
    ++out->draw_checks;
    if (modal_draw &&
        previous_movie != bk_ending_normal_render_movie_frame(s->render))
      ++out->modal_movie_changes;
    if (step == 23)
      CHECK(s->pointer.position[0] > stick_start[0] + 80 * scale);
    if (step == 47)
      CHECK(s->pointer.position[1] < stick_start[1] - 60 * scale);
    if (step == 127) {
      BkEndingControlRect rects[13];
      CHECK(bk_ending_ui_control_rects(&s->ui, rects));
      for (unsigned i = 0; i < 13; ++i) {
        const float *r = s->ui.sprites[(unsigned[]){12,29,38,21,17,19,23,25,27,36,34,45,47}[i]].rect;
        CHECK(r[0] >= 0 && r[1] >= 0 && r[0] + r[2] <= viewport.width &&
              r[1] + r[3] <= viewport.height);
        CHECK(!memcmp(r, &rects[i], sizeof(rects[i])));
        ++out->rectangles;
      }
    }
    if (story && step >= 128 && step <= 160)
      CHECK(s->state->frame.phase == 9);
    if (story && step == 161)
      CHECK(s->state->frame.phase == 1 && !s->state->control.pause_selection);
    frame = (BkSceneFrame){{1, 1.0 / 60, 0}, input};
    CHECK(bk_renderer_begin(renderer, e) && bk_scene_draw(scene, &frame, e) &&
           bk_renderer_end(renderer, e) && bk_ending_normal_scene_after_present(scene, e));
    out->states = digest(out->states, &s->pointer, sizeof(s->pointer));
    out->states = digest(out->states, &s->state->frame, sizeof(s->state->frame));
    out->states = digest(out->states, &s->ui_frame, sizeof(s->ui_frame));
    out->pointer_states = digest(out->pointer_states, &s->pointer, sizeof(s->pointer));
    out->frame_states = digest(out->frame_states, &s->state->frame, sizeof(s->state->frame));
    /* Separate CPU scene parity from final raster coverage. The camera,
     * every loaded actor's local/world/parent caches, projection and actual
     * GPU queue topology must match between identical content extents. */
    out->geometry_states = digest(out->geometry_states, &s->camera,
                                   sizeof(s->camera));
    out->geometry_states = digest(out->geometry_states, s->ui_view,
                                   sizeof(s->ui_view));
    out->geometry_states = digest(out->geometry_states, s->ui_projection,
                                   sizeof(s->ui_projection));
    for (unsigned actor = 0; actor < 5; ++actor) {
      BkActorPose *pose = bk_ending_normal_assets_pose(s->assets, actor);
      const BkModel *model = bk_actor_pose_model(pose);
      CHECK(pose && model);
      for (uint32_t node = 0; node < model->frame_count; ++node) {
        const float *matrices[] = {bk_actor_pose_local(pose, node),
                                   bk_actor_pose_frame(pose, node),
                                   bk_actor_pose_parent_world(pose, node)};
        for (unsigned matrix = 0; matrix < 3; ++matrix) {
          CHECK(matrices[matrix]);
          out->geometry_states = digest(out->geometry_states,
                                         matrices[matrix], 16 * sizeof(float));
        }
      }
    }
    for (unsigned pass = 0; pass < passes; ++pass) {
      uint32_t count = bk_ending_normal_render_queue_count(s->render, pass);
      out->geometry_states = digest(out->geometry_states, &count, sizeof(count));
      for (uint32_t item = 0; item < count; ++item) {
        uint32_t fields[3];
        CHECK(bk_ending_normal_render_queue_item(s->render, pass, item,
                                                  fields, fields + 1,
                                                  fields + 2));
        out->geometry_states = digest(out->geometry_states, fields,
                                       sizeof(fields));
      }
    }
    if (step == 47 || step == 127 || step == total - 1) {
      CHECK(captures < 3 && bk_renderer_readback(renderer, pixels, bytes, e));
      size_t target = (size_t)captures * viewport.width * viewport.height * 4;
      for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
          size_t i = ((size_t)y * width + x) * 4;
          if (x < raster.x || x >= raster.x + raster.width ||
              y < raster.y || y >= raster.y + raster.height)
            CHECK(!memcmp(pixels + i, clear + i, 4));
          else {
            memcpy(out->pixels + target, pixels + i, 4);
            target += 4;
          }
        }
      /* Isolate the exact production UI batch against the same clear color.
       * This is a pure redraw: no input, animation, UI update or time sample.
       * Full-scene readbacks above remain a separate optical diagnostic. */
      BkVirtualPointer held_pointer = s->pointer;
      BkEndingFrameState held_frame = s->state->frame;
      uint32_t held_movie = bk_ending_normal_render_movie_frame(s->render);
      CHECK(bk_renderer_begin(renderer, e));
      CHECK(bk_renderer_viewport(renderer, &viewport, e));
      CHECK(bk_ending_ui_batch_draw(s->ui_batch, e));
      CHECK(bk_renderer_viewport(renderer, NULL, e));
      CHECK(bk_renderer_end(renderer, e));
      CHECK(bk_renderer_readback(renderer, pixels, bytes, e));
      CHECK(!memcmp(&held_pointer, &s->pointer, sizeof(held_pointer)) &&
             !memcmp(&held_frame, &s->state->frame, sizeof(held_frame)) &&
             bk_ending_normal_render_movie_frame(s->render) == held_movie);
      target = (size_t)captures * viewport.width * viewport.height * 4;
      for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
          size_t index = ((size_t)y * width + x) * 4;
          if (x < raster.x || x >= raster.x + raster.width ||
              y < raster.y || y >= raster.y + raster.height)
            CHECK(!memcmp(pixels + index, clear + index, 4));
          else {
            memcpy(out->interface_pixels + target, pixels + index, 4);
            target += 4;
          }
        }
      ++captures;
    }
    ++out->frames;
  }
  CHECK(captures == 3 && out->rectangles == 13);
  CHECK(!story || out->modal_movie_changes > 0);
  {
    /* Isolate the recovered51c736 phase7 draw boundary. This explicitly
     * seeded fixture does not run or claim the unfinished phase7 controller.
     * Even with a later clock, it must keep video/actor timelines and draw
     * no scene geometry. The retained UI is deliberately excluded. */
    BkClipState clocks[5], clock_after;
    for (unsigned i = 0; i < 5; ++i)
      CHECK(bk_actor_pose_state(bk_ending_normal_assets_pose(s->assets, i),
                               clocks + i));
    uint32_t held_movie = bk_ending_normal_render_movie_frame(s->render);
    uint32_t held_random = *s->random;
    s->state->frame.phase = 7;
    s->now_ms += 1000u;
    CHECK(prepare_scene_draw(s, e));
    CHECK(!bk_ending_normal_render_pass_count(s->render) &&
          bk_ending_normal_render_movie_frame(s->render) == held_movie &&
          *s->random == held_random);
    for (unsigned i = 0; i < 5; ++i) {
      CHECK(bk_actor_pose_state(bk_ending_normal_assets_pose(s->assets, i),
                               &clock_after));
      CHECK(!memcmp(clocks + i, &clock_after, sizeof(clock_after)));
    }
    CHECK(bk_renderer_begin(renderer, e));
    CHECK(bk_ending_normal_render_draw(s->render, e));
    CHECK(bk_renderer_end(renderer, e));
    CHECK(bk_renderer_readback(renderer, pixels, bytes, e));
    CHECK(!memcmp(pixels, clear, bytes));
    ++out->empty_draws;
  }
  bk_scene_destroy(scene); scene = NULL;
  BkRenderStats final = bk_renderer_stats(renderer);
  CHECK(final.live_allocations == baseline.live_allocations && final.live_bytes == baseline.live_bytes);
  ok = 1;
done:
  bk_scene_destroy(scene);
  free(records); free(pixels); free(clear);
  bk_audio_destroy(audio);
  bk_resources_destroy(store);
  bk_renderer_destroy(renderer);
  if (!ok) {
    free(out->pixels); free(out->interface_pixels);
    out->pixels = out->interface_pixels = NULL;
  }
  return ok;
}
static unsigned pixel_error(const uint8_t *a, const uint8_t *b) {
  unsigned worst = 0;
  /* The actual swapchain requires COMPOSITE_ALPHA_OPAQUE. Compare displayed
   * RGB; offscreen destination alpha is not a displayed channel. */
  for (unsigned c = 0; c < 3; ++c) {
    unsigned difference = (unsigned)abs((int)a[c] - b[c]);
    if (difference > worst) worst = difference;
  }
  return worst;
}
static int neighbor(const uint8_t *image, const uint8_t *pixel,
                     unsigned width, unsigned height, unsigned x, unsigned y) {
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      int nx = (int)x + dx, ny = (int)y + dy;
      if (nx < 0 || ny < 0 || nx >= (int)width || ny >= (int)height) continue;
      if (pixel_error(image + ((size_t)ny * width + (unsigned)nx) * 4, pixel) <= 4)
        return 1;
    }
  return 0;
}
/* Translating a Vulkan viewport can change raster coverage at an edge even
 * with byte-identical matrices/vertices. Require exact CPU state, <=4/255
 * displayed RGB differences or a bidirectional one-pixel edge match, and fewer than
 * .05% changed pixels overall. A displaced/scaled layout fails these bounds.
 * This is an aspect/coordinate regression, not Windows screenshot equality. */
static int compare(const Capture *a, const Capture *b, size_t *changed,
                    size_t *edges, unsigned *worst) {
  if (a->size != b->size || a->width != b->width || a->height != b->height)
    return 0;
  int states_equal = a->states == b->states &&
      a->pointer_states == b->pointer_states &&
      a->frame_states == b->frame_states &&
      a->geometry_states == b->geometry_states;
  size_t count = 0, edge_count = 0, unmatched = 0;
  size_t plane = (size_t)a->width * a->height * 4;
  for (size_t i = 0; i < a->size; i += 4) {
    unsigned difference = pixel_error(a->pixels + i, b->pixels + i);
    if (difference > *worst) *worst = difference;
    if (!difference) continue;
    ++count;
    if (difference <= 4) continue;
    unsigned x = (unsigned)((i % plane) / 4 % a->width);
    unsigned y = (unsigned)((i % plane) / 4 / a->width);
    size_t start = (i / plane) * plane;
    if (!neighbor(a->pixels + start, b->pixels + i, a->width, a->height, x, y) ||
        !neighbor(b->pixels + start, a->pixels + i, a->width, a->height, x, y)) {
      if (!unmatched)
        fprintf(stderr, "first unmatched raster pixel: capture%zu (%u,%u) error%u\n",
                i / plane, x, y, difference);
      ++unmatched;
      continue;
    }
    ++edge_count;
  }
  *changed += count;
  *edges += edge_count;
  if (unmatched)
    fprintf(stderr, "unmatched raster pixels: %zu/%zu; changed=%zu matched_edges=%zu\n",
            unmatched, a->size / 4, count, edge_count);
  if (count * 2000 > a->size / 4) {
    fprintf(stderr, "changed-pixel bound exceeded: %zu/%zu\n", count, a->size / 4);
    return 0;
  }
  return states_equal && !unmatched;
}
int main(int argc, char **argv) {
  int repeat = argc > 2 && !strcmp(argv[argc - 1], "--repeat");
  zero_origin = argc > 2 && !strcmp(argv[argc - 1], "--zero-origin");
  if (repeat || zero_origin) --argc;
  if (argc < 2 || argc > 4) return 2;
  int selected_mode = -1, selected_size = -1;
  if (argc > 2) {
    char *end;
    unsigned long mode = strtoul(argv[2], &end, 10);
    if (!*argv[2] || *end || mode >= 3) return 2;
    selected_mode = (int)mode;
  }
  if (argc > 3) {
    char *end;
    unsigned long size = strtoul(argv[3], &end, 10);
    if (!*argv[3] || *end || size >= 4) return 2;
    selected_size = (int)size;
  }
  const unsigned sizes[][2] = {{854, 480}, {1280, 720}, {1920, 1080}, {640, 960}};
  unsigned cases = 0, frames = 0, differences = 0, scene_differences = 0;
  unsigned geometry_differences = 0;
  unsigned draw_checks = 0, modal_movie_changes = 0, empty_draws = 0;
  size_t compared = 0, changed = 0, edges = 0;
  unsigned worst = 0;
  size_t ui_changed = 0, ui_edges = 0;
  unsigned ui_worst = 0;
  for (unsigned mode = 0; mode < 3; ++mode)
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(*sizes); ++i) {
      if ((selected_mode >= 0 && selected_mode != (int)mode) ||
          (selected_size >= 0 && selected_size != (int)i))
        continue;
      BkViewport v;
      if (!bk_camera_fit(&v, sizes[i][0], sizes[i][1], 4, 3)) return 1;
      Capture ref = {0}, actual = {0};
      if (!run(argv[1], v.width, v.height, mode == 1, mode == 2, &ref)) return 1;
      unsigned actual_width = repeat ? v.width : sizes[i][0];
      unsigned actual_height = repeat ? v.height : sizes[i][1];
      if (!run(argv[1], actual_width, actual_height, mode == 1, mode == 2, &actual)) {
        free(ref.pixels); free(ref.interface_pixels); return 1;
      }
      Capture ref_ui = ref, actual_ui = actual;
      ref_ui.pixels = ref.interface_pixels;
      actual_ui.pixels = actual.interface_pixels;
      int equal = compare(&ref_ui, &actual_ui, &ui_changed, &ui_edges, &ui_worst);
      int scene_equal = compare(&ref, &actual, &changed, &edges, &worst);
      scene_differences += !scene_equal;
      geometry_differences += ref.geometry_states != actual.geometry_states;
      if (ref.geometry_states != actual.geometry_states)
        fprintf(stderr, "scene geometry differs: %016llx/%016llx\n",
                (unsigned long long)ref.geometry_states,
                (unsigned long long)actual.geometry_states);
      if ((!equal || !scene_equal) && ref.size == actual.size) {
        size_t changed = 0, first = SIZE_MAX;
        unsigned max_error = 0;
        for (size_t j = 0; j < ref.size; ++j) {
          unsigned error = (unsigned)abs((int)ref.pixels[j] - actual.pixels[j]);
          if (error) {
            if (first == SIZE_MAX) first = j;
            ++changed;
            if (error > max_error) max_error = error;
          }
        }
        printf("comparison: state=%016llx/%016llx pointer=%016llx/%016llx "
               "frame=%016llx/%016llx changed=%zu max_error=%u first_byte=%zu\n",
               (unsigned long long)ref.states, (unsigned long long)actual.states,
               (unsigned long long)ref.pointer_states, (unsigned long long)actual.pointer_states,
               (unsigned long long)ref.frame_states, (unsigned long long)actual.frame_states,
               changed, max_error, first);
        unsigned shown = 0;
        size_t changed_pixels = 0, displaced = 0;
        for (size_t j = 0; j < ref.size; j += 4) {
          unsigned difference = 0;
          for (unsigned c = 0; c < 4; ++c) {
            unsigned d = (unsigned)abs((int)ref.pixels[j+c] - actual.pixels[j+c]);
            if (d > difference) difference = d;
          }
          if (!difference) continue;
          ++changed_pixels;
          if (difference <= 2) continue;
          size_t plane = (size_t)v.width * v.height * 4;
          unsigned x = (unsigned)((j % plane) / 4 % v.width);
          unsigned y = (unsigned)((j % plane) / 4 / v.width);
          int near = 0;
          for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
              if ((int)x + dx < 0 || (int)y + dy < 0 ||
                  (int)x + dx >= (int)v.width || (int)y + dy >= (int)v.height) continue;
              size_t k = (j / plane) * plane +
                          ((size_t)((int)y + dy) * v.width + (unsigned)((int)x + dx)) * 4;
              unsigned distance = 0;
              for (unsigned c = 0; c < 4; ++c) {
                unsigned d = (unsigned)abs((int)ref.pixels[k+c] - actual.pixels[j+c]);
                if (d > distance) distance = d;
              }
              if (distance <= 2) near = 1;
            }
          displaced += !near;
          if (shown++ < 12)
            printf("pixel plane%zu (%u,%u): %u,%u,%u,%u -> %u,%u,%u,%u near=%d\n",
                   j / plane, x, y, ref.pixels[j], ref.pixels[j+1], ref.pixels[j+2], ref.pixels[j+3],
                   actual.pixels[j], actual.pixels[j+1], actual.pixels[j+2], actual.pixels[j+3], near);
        }
        printf("pixel coverage: changed=%zu non_neighbor=%zu\n", changed_pixels, displaced);
      }
      printf("viewport %ux%u mode%u%s: content=%ux%u, UI=%s full_scene=%s\n",
             actual_width, actual_height, mode,
             repeat ? " same-origin repeat" :
             zero_origin ? " explicit zero-origin raster fixture" : "",
             v.width, v.height,
             equal ? "PASS" : "DIFFERENCE", scene_equal ? "PASS" : "DIFFERENCE");
      fflush(stdout);
      compared += ref.size;
      frames += ref.frames + actual.frames;
      draw_checks += ref.draw_checks + actual.draw_checks;
      modal_movie_changes += ref.modal_movie_changes + actual.modal_movie_changes;
      empty_draws += ref.empty_draws + actual.empty_draws;
      free(ref.pixels); free(actual.pixels);
      free(ref.interface_pixels); free(actual.interface_pixels);
      differences += !equal;
      ++cases;
    }
  printf("%s ending viewport UI: %u paired cases, %u actual scene frames, "
         "%zu UI readback RGBA bytes; changed_UI_pixels=%zu UI_edges=%zu "
         "max_UI_display_error=%u; exact CPU states, stick/touch/13 toolbar "
         "hitboxes/modal cancel/bars/projection/resources\n",
         differences ? "FAIL" : "PASS", cases, frames, compared, ui_changed, ui_edges, ui_worst);
  printf("Full-scene optical diagnostic: %u/%u within stated raster bounds, "
         "max_display_error=%u; not a Windows screenshot or full-ending acceptance\n",
         cases - scene_differences, cases, worst);
  printf("%s scene geometry pairing: %u/%u camera/projection, all actor "
         "local/world/parent matrices and submitted queue topology agree\n",
         geometry_differences ? "FAIL" : "PASS", cases - geometry_differences,
         cases);
  printf("Draw stage PASS: %u actual GPU pass checks, %u modal movie frame "
         "changes, %u explicit phase7 empty readbacks; redraw retains video\n",
         draw_checks, modal_movie_changes, empty_draws);
  return differences ? 1 : 0;
}
