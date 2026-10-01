/*Actual session -> action4a -> original final-image UI -> wait/click.
 * The transition request is an explicit boundary fixture. Phase, old/new
 * ownership, image loading, fade/timer, input and drawing use production.
 * A separate explicit callback handoff checks retained background/light
 * values after the old 3D objects have died; it is not a natural exit path.
 * The real action7 currently reaches the unimplemented4D1025 and must fail.
 */
#include "../runtime/scene/ending_normal_session.c"
#include "ending_ui_render_oracle.h"
#include <unistd.h>

typedef struct { uint64_t submitted, consumed, hash; } ImageSink;
typedef struct {
  unsigned entries, frames, transitions, redraws, samples, max_error;
  unsigned clicks, timeouts, restored, rejected, terminal, pointer_frames;
  unsigned missing_images, corrupt_images;
  uint64_t initial_skin_dispatches;
  uint64_t state, pcm, pixels;
} ImageResult;
static uint64_t image_hash(uint64_t h, const void *data, size_t size) {
  const uint8_t *p = data;
  for (size_t i = 0; i < size; ++i) h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static int image_submit(void *context, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  ImageSink *s = context;
  s->hash = image_hash(s->hash, pcm, frames * 2 * sizeof(*pcm));
  s->submitted += frames;
  return 1;
}
static int image_poll(void *context, uint64_t *consumed, char e[256]) {
  (void)e;
  ImageSink *s = context;
  uint64_t next = s->consumed + 800;
  s->consumed = next < s->submitted ? next : s->submitted;
  *consumed = s->consumed;
  return 1;
}
static int image_present(BkScene *scene, BkRenderer *r, char e[256]) {
  return bk_renderer_begin(r, e) && bk_scene_draw(scene, &(BkSceneFrame){0}, e) &&
         bk_renderer_end(r, e) && bk_ending_normal_scene_after_present(scene, e);
}
static int image_tick(BkScene *scene, EndingNormalScene *s, const BkInput *input,
                        double wall, ImageResult *result, char e[256]) {
  if (!bk_audio_poll(s->services.audio, e) ||
      !bk_ending_normal_scene_step_at(scene, 1.0 / 60.0, wall, input, e) ||
      !bk_audio_fill(s->services.audio, e) ||
      !image_present(scene, s->services.renderer, e)) return 0;
  ++result->frames;
  return 1;
}
static BkEndingBackgroundAssets *image_background(EndingNormalScene *s) {
  return s->tertiary_assets ? bk_ending_tertiary_assets_background(s->tertiary_assets) :
         s->secondary_assets ? bk_ending_secondary_assets_background(s->secondary_assets) :
         bk_ending_normal_assets_background(s->assets);
}
static int image_redraw(BkScene *scene, EndingNormalScene *s, ImageResult *result,
                         uint8_t *pixels, uint8_t *again, size_t size, char e[256]) {
  if (!bk_renderer_readback(s->services.renderer, pixels, size, e)) return 0;
  BkEndingState state = *s->state;
  BkAudioStats audio = bk_audio_stats(s->services.audio);
  uint32_t rng = *s->random;
  BkEndingUiCompositeFrame frame = s->ui_frame;
  double wall = s->wall_seconds;
  BkRenderStats before = bk_renderer_stats(s->services.renderer);
  for (unsigned i = 0; i < 3; ++i) {
    if (!image_present(scene, s->services.renderer, e) ||
        !bk_renderer_readback(s->services.renderer, again, size, e) ||
        memcmp(pixels, again, size) || memcmp(&state, s->state, sizeof(state)) ||
        memcmp(&frame, &s->ui_frame, sizeof(frame)) || rng != *s->random ||
        wall != s->wall_seconds) return fail(e, "final image redraw changed the snapshot");
    BkAudioStats now = bk_audio_stats(s->services.audio);
    BkRenderStats render = bk_renderer_stats(s->services.renderer);
    /*A newly created 3D owner queues its initial ENVL work. The next begin
     * executes that work even while the previous pure-UI snapshot is shown.
     * Permit that one initial submission, then require every later redraw
     * to leave geometry counters unchanged. A real pure-UI interval has no
     * live renderer and must never submit any skin work. */
    int initial_upload = s->snapshot_ui_only && s->render && i == 0;
    if (now.submitted != audio.submitted || now.consumed != audio.consumed ||
        render.live_allocations != before.live_allocations ||
        render.live_bytes != before.live_bytes ||
        (s->snapshot_ui_only &&
            ((!initial_upload && render.skin_dispatches != before.skin_dispatches) ||
             render.mesh_updates != before.mesh_updates))) {
      snprintf(e, 256,
          "final image redraw %u: audio %llu/%llu -> %llu/%llu, allocations %llu -> %llu, "
          "bytes %llu -> %llu, skin %llu -> %llu, meshes %llu -> %llu (live3D=%d)", i,
          (unsigned long long)audio.submitted, (unsigned long long)audio.consumed,
          (unsigned long long)now.submitted, (unsigned long long)now.consumed,
          (unsigned long long)before.live_allocations, (unsigned long long)render.live_allocations,
          (unsigned long long)before.live_bytes, (unsigned long long)render.live_bytes,
          (unsigned long long)before.skin_dispatches, (unsigned long long)render.skin_dispatches,
          (unsigned long long)before.mesh_updates, (unsigned long long)render.mesh_updates,
          s->render != NULL);
      return 0;
    }
    if (initial_upload)
      result->initial_skin_dispatches += render.skin_dispatches - before.skin_dispatches;
    before = render;
    ++result->redraws;
  }
  return 1;
}
static int image_pixels(EndingNormalScene *s, ImageResult *result, uint8_t *pixels,
                         unsigned width, unsigned height, char e[256]) {
  if (!s->final_image || !s->snapshot_ui_only || s->snapshot_render ||
      s->ui_frame.curtain.curtain_alpha != 0 ||
      !s->ui_frame.sprites.count || s->ui_frame.sprites.draws[0].slot != 74)
    return fail(e, "image raster check requires a visible pure-UI frame");
  BkImage images[75] = {0};
  BkBlob raw = {0};
  int ok = 0;
  for (unsigned i = 0; i < s->ui_frame.sprites.count; ++i) {
    unsigned slot = s->ui_frame.sprites.draws[i].slot;
    if (images[slot].rgba) continue;
    const char *name = slot == 8 || slot >= 63
        ? bk_ending_stage_ui_image(&s->stage_ui, slot) : bk_ending_ui_image(slot);
    if (!name || bk_resources_read(s->services.resources, "bk3_00", name, &raw, e) != BK_RESOURCE_OK ||
        !bk_image_decode(raw.data, raw.size, &images[slot], e)) goto done;
    if (slot == 74 && (raw.size < 30 || memcmp(raw.data, "BM", 2) ||
        images[slot].width != 1024 || images[slot].height != 768 ||
        raw.data[28] != 24 || raw.data[29] != 0)) {
      fail(e, "final image is not the actual1024x768/24-bit BMP"); goto done;
    }
    bk_blob_free(&raw);
  }
  if (!bk_renderer_readback(s->services.renderer, pixels, (size_t)width * height * 4, e)) goto done;
  const BkViewport *v = &s->viewport;
  unsigned compared = 0;
  for (unsigned y = 0; y < height; ++y)
    for (unsigned x = 0; x < width; ++x) {
      const uint8_t *p = pixels + ((size_t)y * width + x) * 4;
      if (x < v->x || x >= v->x + v->width || y < v->y || y >= v->y + v->height) {
        if (p[0] || p[1] || p[2] || p[3] != 255) {
          fail(e, "final image escaped the centered4:3 viewport"); goto done;
        }
        continue;
      }
      if ((x - v->x) % 7 || (y - v->y) % 7) continue;
      int expected_rgb[3] = {0};
      if (!expected_over(images, &s->ui_frame.sprites, x - v->x, y - v->y, expected_rgb)) continue;
      for (unsigned c = 0; c < 3; ++c) {
        unsigned error = (unsigned)abs((int)p[c] - expected_rgb[c]);
        if (error > result->max_error) result->max_error = error;
        if (error > 1) {
          snprintf(e, 256, "final image raster at%u,%u channel%u: %u versus%d",
                   x, y, c, p[c], expected_rgb[c]); goto done;
        }
        ++compared;
      }
    }
  if (compared < 1000) { fail(e, "too few defined interior image samples"); goto done; }
  result->samples += compared;
  result->pixels = image_hash(result->pixels, pixels, (size_t)width * height * 4);
  ok = 1;
done:
  bk_blob_free(&raw);
  for (unsigned i = 0; i < 75; ++i) bk_image_free(&images[i]);
  return ok;
}
static int image_light_rejections(BkEndingNormalRender *renderer, const BkModel *model,
                                    ImageResult *result, char e[256]) {
  BkRetainedLightState saved, actual;
  if (!bk_ending_normal_render_save_lighting(renderer, model, &saved, e)) return 0;
  actual = saved;
  if (bk_ending_normal_render_save_lighting(NULL, model, &actual, e) ||
      memcmp(&actual, &saved, sizeof(saved)))
    return fail(e, "invalid light owner changed the saved output");
  ++result->rejected;
  for (unsigned i = 0; i < (saved.has_model ? 4u : 2u); ++i) {
    BkRetainedLightState invalid = saved;
    if (i == 0) invalid.has_model = 2;
    else if (i == 1) { invalid.has_model = 1; invalid.model = NULL; }
    else if (i == 2) ++invalid.device.light_count;
    else invalid.device.enabled = UINT32_MAX;
    if (bk_ending_normal_render_restore_lighting(renderer, &invalid, e) ||
        !bk_ending_normal_render_save_lighting(renderer, model, &actual, e) ||
        memcmp(&actual, &saved, sizeof(saved)))
      return fail(e, "invalid retained light state was accepted or changed the live device");
    ++result->rejected;
  }
  *e = 0;
  return 1;
}
#define VERIFY(condition) do { if (!(condition)) { \
  fprintf(stderr, "final image group%u source%u line%d (%s): %s\n", \
          group, source, __LINE__, #condition, e); goto done; } } while (0)
static int image_profile(BkRenderer *renderer, BkResourceStore *store, unsigned group,
                           unsigned source, ImageResult *out, char e[256]) {
  int ok = 0;
  BkScene *scene = NULL;
  BkAudio *audio = NULL;
  BkEndingRecords *records = calloc(1, sizeof(*records));
  ImageSink sink = {.hash = UINT64_C(14695981039346656037)};
  BkAudioSink output = {&sink, 48000, 480, 1920, image_submit, image_poll};
  unsigned width, height;
  bk_renderer_extent(renderer, &width, &height);
  size_t bytes = (size_t)width * height * 4;
  uint8_t *pixels = malloc(bytes), *redraw = malloc(bytes);
  float *background_pose = NULL;
  BkInput input = {0};
  VERIFY(records && pixels && redraw && (audio = bk_audio_create(&output, e)));
  BkSceneServices services = {store, renderer, stdout, audio, NULL, NULL};
  uint8_t unlocked[5][8] = {{0}};
  scene = bk_ending_normal_scene_create_story(
      &services, group, source == 3, records, unlocked, NULL, e);
  VERIFY(scene);
  EndingNormalScene *s = bk_scene_custom_context(scene);
  VERIFY(s && s->frame_active && s->state->frame.phase == (source == 3 ? 3 : 1));
  VERIFY(s->viewport.x == 160 && s->viewport.y == 0 &&
         s->viewport.width == 960 && s->viewport.height == 720);
  VERIFY(bk_audio_fill(audio, e) && image_present(scene, renderer, e));
  for (unsigned i = 0; i < 8; ++i)
    VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, out, e));
  if (source == 2) {
    /*Reach phase2 through the actual story reload. A gallery entry keeps
     * previous_flow18 and cannot stand in for a story final-image exit. */
    s->state->next_mode = 0;
    s->common->action = 6;
    s->common->blocked = 1;
    unsigned entered = 0;
    while (!s->secondary_assets && entered++ < 240)
      VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, out, e));
    VERIFY(s->secondary_assets);
    VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, out, e));
  }
  VERIFY(s->state->frame.phase == (int)source && s->previous_flow == 8);
  BkEndingNormalRender *old_render = s->render;
  BkEndingBackgroundAssets *background = image_background(s);
  VERIFY(background);
  BkEndingAudio *audio_owner = s->audio;
  BkEndingUiRender *base_ui = s->ui_render;
  BkEndingUiBatch *batch = s->ui_batch;
  /*Original phase7 request; actual UI curtain decides the release tick.*/
  s->common->action = 0x4a;
  s->common->blocked = 1;
  unsigned transition_frames = 0;
  while (!s->final_image && transition_frames++ < 240)
    VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, out, e));
  VERIFY(s->final_image && s->state->frame.phase == 7 && !s->render && !scene_forest(s));
  VERIFY(s->retired.render == old_render && s->snapshot_render == old_render &&
         !s->snapshot_ui_only && s->retained_background == background && s->has_retained_lights);
  VERIFY(s->audio == audio_owner && s->ui_render == base_ui && s->ui_batch == batch);
  VERIFY(s->stage_ui.loaded == (1u << 12) &&
         !memcmp(s->stage_ui.sprites[11].rect, (float[4]){0, 0, 960, 720}, 16));
  BkRetainedLightState saved = s->retained_lights, actual;
  const BkEndingBackgroundData *data = bk_ending_background_assets_data(background);
  VERIFY(data && bk_ending_normal_render_save_lighting(old_render, data->model, &actual, e));
  VERIFY(!memcmp(&saved, &actual, sizeof(saved)));
  BkClipState background_clip;
  VERIFY(bk_actor_pose_state(data->pose, &background_clip));
  size_t pose_floats = (size_t)data->model->frame_count * 16;
  VERIFY(background_pose = malloc(pose_floats * sizeof(float)));
  for (uint32_t i = 0; i < data->model->frame_count; ++i)
    memcpy(background_pose + (size_t)i * 16, bk_actor_pose_frame(data->pose, i), 64);
  VERIFY(image_redraw(scene, s, out, pixels, redraw, bytes, e));
  BkRenderStats before_retire = bk_renderer_stats(renderer);
  VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, out, e));
  VERIFY(!s->retired.render && !s->snapshot_render && s->snapshot_ui_only && s->final_image);
  BkRenderStats after_retire = bk_renderer_stats(renderer);
  VERIFY(after_retire.live_allocations < before_retire.live_allocations &&
         after_retire.live_bytes < before_retire.live_bytes);
  ++out->transitions;
  for (unsigned i = 0; i < 150; ++i) {
    /*Actual analog-pointer path during phase7, away from fading toolbar.*/
    input.move_x = i % 12 < 6 ? .3f : -.3f;
    input.move_y = 0;
    VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, out, e));
    ++out->pointer_frames;
  }
  input = (BkInput){0};
  VERIFY(s->stage_ui.sprites[11].transform.fade.stage == 3 &&
         s->common->curtain.stage == 0 && s->ui_frame.curtain.curtain_alpha == 0);
  VERIFY(s->retained_background == background && !memcmp(&saved, &s->retained_lights, sizeof(saved)));
  BkClipState held_clip;
  VERIFY(bk_actor_pose_state(data->pose, &held_clip));
  VERIFY(!memcmp(&background_clip, &held_clip, sizeof(held_clip)));
  for (uint32_t i = 0; i < data->model->frame_count; ++i)
    VERIFY(!memcmp(background_pose + (size_t)i * 16, bk_actor_pose_frame(data->pose, i), 64));
  BkRenderStats stable = bk_renderer_stats(renderer);
  VERIFY(stable.live_allocations == after_retire.live_allocations &&
         stable.live_bytes == after_retire.live_bytes &&
         stable.skin_dispatches == after_retire.skin_dispatches);
  VERIFY(image_pixels(s, out, pixels, width, height, e));
  VERIFY(image_redraw(scene, s, out, pixels, redraw, bytes, e));

  /*Explicit resource handoff fixture, after150 real pure-UI ticks. Verify
   * only held device values cross the gap; the final-image draw stays valid.*/
  VERIFY(bk_renderer_readback(renderer, pixels, bytes, e));
  VERIFY(ui_reload_final_image(s, 0, 0, e));
  BkEndingLoader restored_loader = source == 3 ? BK_ENDING_LOAD_4D2320 :
      source == 2 ? BK_ENDING_LOAD_4D00FA : BK_ENDING_LOAD_4CF318;
  VERIFY(ui_reload_load(s, restored_loader, -1, e));
  VERIFY(!s->retained_background && !s->has_retained_lights && !s->final_image && s->render);
  BkEndingBackgroundAssets *next_background = image_background(s);
  const BkEndingBackgroundData *next_data = bk_ending_background_assets_data(next_background);
  int same_background = group != 1 || source == 3;
  VERIFY(next_data && ((next_background == background) == same_background));
  VERIFY(bk_ending_normal_render_save_lighting(s->render, next_data->model, &actual, e));
  VERIFY(saved.ambient == actual.ambient);
  if (same_background) VERIFY(!memcmp(&saved, &actual, sizeof(saved)));
  VERIFY(image_light_rejections(s->render, next_data->model, out, e));
  VERIFY(image_light_rejections(s->render, bk_actor_pose_model(scene_primary(s)), out, e));
  if (source == 3) {
    BkMaterialPose *background_materials = bk_ending_tertiary_assets_materials(s->tertiary_assets, 5);
    VERIFY(s->special_material_count >= next_data->model->material_count);
    for (uint32_t i = 0; i < next_data->model->material_count; ++i)
      VERIFY(s->special_materials[i].pose == background_materials &&
             s->special_materials[i].index == i);
  }
  VERIFY(image_redraw(scene, s, out, pixels, redraw, bytes, e));
  VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, out, e));
  VERIFY(!s->snapshot_ui_only && s->snapshot_render == s->render);
  ++out->restored;

  /*A second actual transition, then original click/timer exit request.
   * Do not set finish_elapsed or substitute an available loader for4D1025.*/
  s->common->action = 0x4a;
  s->common->blocked = 1;
  transition_frames = 0;
  while (!s->final_image && transition_frames++ < 240)
    VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, out, e));
  VERIFY(s->final_image);
  for (unsigned i = 0; i < 150; ++i)
    VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, out, e));
  VERIFY(s->state->frame.previous_clock && s->common->action != 7);
  if ((group + source) % 2) {
    input.held = input.pressed = BK_BUTTON_CONFIRM;
    VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, out, e));
    ++out->clicks;
  } else {
    uint32_t previous = s->state->frame.previous_clock;
    uint32_t elapsed = s->state->frame.finish_elapsed;
    VERIFY(elapsed < 30000);
    double exact = ((double)previous + 30000 - elapsed + .25) / 1000;
    VERIFY(image_tick(scene, s, &input, exact, out, e));
    VERIFY(s->state->frame.finish_elapsed == 30000 && s->common->action != 7);
    VERIFY(image_tick(scene, s, &input, exact + .001, out, e));
    ++out->timeouts;
  }
  input = (BkInput){0};
  VERIFY(s->common->action == 7 && s->common->blocked == 1 &&
         !s->state->frame.finish_elapsed && !s->state->frame.previous_clock);
  VERIFY(image_redraw(scene, s, out, pixels, redraw, bytes, e));
  /*No further implementation is claimed: reach and inspect the real missing
   * loader failure after the curtain, rather than silently staying on image.*/
  unsigned exit_frames = 0;
  int accepted = 1;
  while (accepted && exit_frames++ < 240)
    accepted = image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, out, e);
  VERIFY(!accepted && s->failed && strstr(e, "does not own this ending loader"));
  ++out->terminal;
  *e = 0;
  out->state = image_hash(out->state, &s->state->frame, sizeof(s->state->frame));
  out->pcm = image_hash(out->pcm, &sink.hash, sizeof(sink.hash));
  ++out->entries;
  ok = 1;
done:
  bk_scene_destroy(scene);
  bk_audio_destroy(audio);
  free(records);
  free(background_pose);
  free(pixels);
  free(redraw);
  return ok;
}
static int image_resource_failure(BkRenderer *renderer, BkResourceStore *store,
                                    unsigned group, unsigned source, int corrupt,
                                    ImageResult *result, char e[256]) {
  int ok = 0, has_directory = 0;
  BkScene *scene = NULL;
  BkAudio *audio = NULL;
  BkResourceStore *fault = NULL;
  BkEndingRecords *records = calloc(1, sizeof(*records));
  FILE *file = NULL;
  char directory[] = "local/ending-image-fault-XXXXXX", path[1024] = {0};
  ImageSink sink = {.hash = UINT64_C(14695981039346656037)};
  BkAudioSink output = {&sink, 48000, 480, 1920, image_submit, image_poll};
  BkInput input = {0};
  uint8_t unlocked[5][8] = {{0}};
  VERIFY(records && (audio = bk_audio_create(&output, e)) && (fault = bk_resources_create(e)));
  if (corrupt) {
    VERIFY(mkdtemp(directory));
    has_directory = 1;
    VERIFY(snprintf(path, sizeof(path), "%s/%s", directory, bk_ending_final_image(group)) < (int)sizeof(path));
    VERIFY(file = fopen(path, "wb"));
    const uint8_t truncated_bmp[14] = {'B', 'M'};
    VERIFY(fwrite(truncated_bmp, 1, sizeof(truncated_bmp), file) == sizeof(truncated_bmp));
    int closed = fclose(file);
    file = NULL;
    VERIFY(closed == 0);
    VERIFY(bk_resources_mount_directory(fault, "bk3_00", directory, 1024, e));
  }
  BkSceneServices services = {store, renderer, stdout, audio, NULL, NULL};
  scene = bk_ending_normal_scene_create_story(&services, group, source == 3,
                                              records, unlocked, NULL, e);
  VERIFY(scene);
  EndingNormalScene *s = bk_scene_custom_context(scene);
  VERIFY(s && bk_audio_fill(audio, e) && image_present(scene, renderer, e));
  for (unsigned i = 0; i < 8; ++i)
    VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, result, e));
  if (source == 2) {
    s->state->next_mode = 0;
    s->common->action = 6;
    s->common->blocked = 1;
    unsigned entered = 0;
    while (!s->secondary_assets && entered++ < 240)
      VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, result, e));
    VERIFY(s->secondary_assets);
    VERIFY(image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, result, e));
  }
  VERIFY(s->state->frame.phase == (int)source);
  BkEndingNormalRender *old_render = s->render;
  /*Replace only the image-loading service in this isolated fixture. All
   * existing actor/audio/UI owners still borrow the real read-only store.
   * The actual transition must terminate when the required image is absent
   * or corrupt, and destruction must retire its entire executed prefix. */
  s->services.resources = fault;
  s->common->action = 0x4a;
  s->common->blocked = 1;
  unsigned attempts = 0;
  int accepted = 1;
  while (accepted && attempts++ < 240)
    accepted = image_tick(scene, s, &input, s->wall_seconds + 1.0 / 60, result, e);
  VERIFY(!accepted && s->failed && *e && s->state->frame.phase == 7 &&
         !s->final_image && !s->render && s->retired.render == old_render &&
         !s->ui_stage_render && !s->retained_background && !s->has_retained_lights);
  if (corrupt) ++result->corrupt_images;
  else ++result->missing_images;
  *e = 0;
  ok = 1;
done:
  if (file) fclose(file);
  bk_scene_destroy(scene);
  bk_audio_destroy(audio);
  bk_resources_destroy(fault);
  free(records);
  if (*path) unlink(path);
  if (has_directory) rmdir(directory);
  return ok;
}
#undef VERIFY
int main(int argc, char **argv) {
  if (argc < 2 || argc > 4) return 2;
  unsigned first = 0, last = 5, source_first = 1, source_last = 4;
  if (argc >= 3) {
    if (strlen(argv[2]) != 1 || argv[2][0] < '0' || argv[2][0] > '4') return 2;
    first = (unsigned)(argv[2][0] - '0'); last = first + 1;
  }
  if (argc == 4) {
    if (strlen(argv[3]) != 1 || argv[3][0] < '1' || argv[3][0] > '3') return 2;
    source_first = (unsigned)(argv[3][0] - '0'); source_last = source_first + 1;
  }
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkRenderer *renderer = bk_renderer_create(1280, 720, stdout, e);
  BkResourceStore *store = bk_resources_create(e);
  if (!renderer || !store) goto done;
  const char *packs[] = {"bk3_00", "bk3_02", "bk3_03", "bk3_04", "bk3_06",
                        "bk3_08", "bk3_09", "bk3_11", "bk3_18", "fambom"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i)
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >= (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, e)) goto done;
  BkRenderStats baseline = bk_renderer_stats(renderer);
  ImageResult result = {.state = UINT64_C(14695981039346656037),
      .pcm = UINT64_C(14695981039346656037), .pixels = UINT64_C(14695981039346656037)};
  for (unsigned group = first; group < last; ++group)
    for (unsigned source = source_first; source < source_last; ++source) {
      if (!image_profile(renderer, store, group, source, &result, e)) goto done;
      BkRenderStats current = bk_renderer_stats(renderer);
      if (current.live_allocations != baseline.live_allocations || current.live_bytes != baseline.live_bytes) {
        fail(e, "final image scene retained GPU allocations after destruction"); goto done;
      }
      printf("PASS final image profile%u source%u\n", group, source); fflush(stdout);
    }
  for (unsigned source = source_first; source < source_last; ++source)
    for (int corrupt = 0; corrupt < 2; ++corrupt) {
      unsigned group = argc >= 3 ? first : source == 1 ? 4 : source == 2 ? 1 : 2;
      if (!image_resource_failure(renderer, store, group, source, corrupt, &result, e)) goto done;
      BkRenderStats current = bk_renderer_stats(renderer);
      if (current.live_allocations != baseline.live_allocations || current.live_bytes != baseline.live_bytes) {
        fail(e, "failed final image transition retained GPU allocations"); goto done;
      }
    }
  if (result.entries != (last - first) * (source_last - source_first) ||
      result.restored != result.entries || result.terminal != result.entries ||
      result.missing_images != source_last - source_first ||
      result.corrupt_images != source_last - source_first ||
      result.samples < result.entries * 1000u) {
    fail(e, "missing final-image lifecycle coverage"); goto done;
  }
  printf("PASS final image entries%u frames%u transition%u redraw%u samples%u max%u pointer%u click%u timeout%u restore%u initial-skin%llu rejects%u missing%u corrupt%u pending-loader%u state=%016llx pcm=%016llx pixels=%016llx\n",
      result.entries, result.frames, result.transitions, result.redraws, result.samples,
      result.max_error, result.pointer_frames, result.clicks, result.timeouts,
      result.restored, (unsigned long long)result.initial_skin_dispatches, result.rejected,
      result.missing_images, result.corrupt_images,
      result.terminal, (unsigned long long)result.state,
      (unsigned long long)result.pcm, (unsigned long long)result.pixels);
  rc = 0;
done:
  if (rc) fprintf(stderr, "%s\n", e);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(store);
  return rc;
}
