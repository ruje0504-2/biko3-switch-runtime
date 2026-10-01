#include "app/pause_preview.h"
#include "app/pause_session.h"
#include "scene/system_audio.h"
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkRenderer *renderer;
  BkViewport viewport;
  BkResourceStore *resources;
  BkCaptureFiles *files;
  BkPauseSession *session;
  BkCommonHudState common;
  BkFlowTransition flow;
  BkMenuCursor cursor;
  BkPauseState local_state, *state;
  BkPauseBindings bindings;
  BkFlowTransitionOps external_release;
  uint8_t overlay, hover_latched;
  BkSystemAudio *sounds[8];
  BkAudio *audio;
  BkAudioSink sink;
  float pointer[2], motion[2];
  uint32_t now_ms;
  double elapsed;
  int prepared, finished;
  uint64_t submitted;
  int owns_audio;
} PausePreview;
static int submit(void *context, const int16_t *samples, size_t frames,
                  char error[256]) {
  (void)samples;
  (void)error;
  ((PausePreview *)context)->submitted += frames;
  return 1;
}
static int poll(void *context, uint64_t *consumed, char error[256]) {
  (void)error;
  *consumed = ((PausePreview *)context)->submitted;
  return 1;
}
static int warp(void *context, float x, float y, char error[256]) {
  (void)error;
  PausePreview *p = context;
  p->pointer[0] = x;
  p->pointer[1] = y;
  return 1;
}
static int pointer(void *context, float point[2], float motion[2],
                   char error[256]) {
  (void)error;
  PausePreview *p = context;
  memcpy(point, p->pointer, sizeof(p->pointer));
  memcpy(motion, p->motion, sizeof(p->motion));
  return 1;
}
static int release_other(void *context, uint8_t flow, char error[256]) {
  PausePreview *p = context;
  if (p->external_release.release)
    return p->external_release.release(p->external_release.context, flow,
                                       error);
  snprintf(error, 256, "pause preview: flow%u release is not bound", flow);
  return 0;
}
void bk_pause_preview_clock(BkScene *scene, double elapsed) {
  PausePreview *p = bk_scene_custom_context(scene);
  if (p)
    p->elapsed = elapsed;
}
static int step(void *context, double seconds, const BkInput *input,
                char error[256]) {
  PausePreview *p = context;
  if (!p->session || p->finished)
    return 1;
  if (seconds <= 0 || seconds > 1) {
    snprintf(error, 256, "pause preview: invalid fixed step");
    return 0;
  }
  if (input->pointer_active) {
    p->pointer[0] = input->pointer_x - p->viewport.x;
    p->pointer[1] = input->pointer_y - p->viewport.y;
  }
  p->motion[0] = input->pointer_motion_x;
  p->motion[1] = input->pointer_motion_y;
  p->elapsed += seconds;
  p->now_ms = (uint32_t)(uint64_t)(p->elapsed * 1000);
  BkPauseInput in = {
      .buttons = ((input->pressed & BK_BUTTON_CONFIRM) ? BK_PAUSE_CONFIRM : 0) |
                 ((input->pressed & BK_BUTTON_UP) ? BK_PAUSE_UP : 0) |
                 ((input->pressed & BK_BUTTON_DOWN) ? BK_PAUSE_DOWN : 0) |
                 ((input->pressed & BK_BUTTON_LEFT) ? BK_PAUSE_LEFT : 0) |
                 ((input->pressed & BK_BUTTON_RIGHT) ? BK_PAUSE_RIGHT : 0),
      .now_ms = p->now_ms,
      .seconds = (float)seconds,
      .scale = p->viewport.width / 1280.f,
      .special = 0};
  if (p->owns_audio && !bk_audio_poll(p->audio, error))
    return 0;
  BkPauseFrame snapshot;
  if (!bk_pause_session_step(p->session, &in, &snapshot, error))
    return 0;
  if (p->owns_audio && !bk_audio_fill(p->audio, error))
    return 0;
  p->prepared = 1;
  if (p->bindings.flow->current != 4)
    p->finished = 1;
  return 1;
}
static int draw(void *context, const BkSceneFrame *frame, char error[256]) {
  (void)frame;
  PausePreview *p = context;
  if (!p->prepared) {
    snprintf(error, 256, "pause preview: draw before prepare");
    return 0;
  }
  if (!bk_renderer_viewport(p->renderer, &p->viewport, error) ||
      !bk_pause_session_draw(p->session, error) ||
      !bk_renderer_viewport(p->renderer, NULL, error))
    return 0;
  return 1;
}
static void destroy(void *context) {
  PausePreview *p = context;
  bk_pause_session_destroy(p->session);
  for (unsigned i = 0; i < 8; ++i)
    bk_system_audio_destroy(p->sounds[i]);
  if (p->owns_audio)
    bk_audio_destroy(p->audio);
  free(p);
}
BkScene *bk_pause_preview_create_shared(const BkSceneServices *services,
                                        BkPauseState *state,
                                        const BkPauseBindings *bindings,
                                        const BkFlowTransitionOps *release,
                                        double elapsed, char error[256]) {
  if (!services || !services->resources || !services->renderer ||
      !services->capture_files) {
    snprintf(error, 256, "pause preview: capture output root is required");
    return NULL;
  }
  PausePreview *p = calloc(1, sizeof(*p));
  if (!p) {
    snprintf(error, 256, "pause preview: allocation failed");
    return NULL;
  }
  p->renderer = services->renderer;
  unsigned width, height;
  bk_renderer_extent(p->renderer, &width, &height);
  if (!bk_camera_fit(&p->viewport, width, height, 4, 3)) {
    snprintf(error, 256, "pause preview: invalid renderer extent");
    free(p);
    return NULL;
  }
  p->resources = services->resources;
  p->files = services->capture_files;
  p->flow = (BkFlowTransition){4, 2, 0, 0};
  p->state = state ? state : &p->local_state;
  p->bindings = bindings ? *bindings
                         : (BkPauseBindings){&p->common, &p->flow, &p->cursor,
                                             &p->overlay, &p->hover_latched};
  if (release)
    p->external_release = *release;
  p->elapsed = elapsed;
  p->sink = (BkAudioSink){p, 48000, 240, 960, submit, poll};
  p->audio = services->audio;
  if (!p->audio) {
    p->audio = bk_audio_create(&p->sink, error);
    p->owns_audio = 1;
    if (!p->audio)
      goto fail;
  }
  const unsigned slots[] = {0, 1, 2, 3, 5};
  for (unsigned i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
    unsigned slot = slots[i];
    p->sounds[slot] = bk_system_audio_create_slot(services->resources, p->audio,
                                                  48 + slot, slot, bk_volume_get(services->audio_volumes, BK_VOLUME_EFFECT, -600), error);
    if (!p->sounds[slot])
      goto fail;
  }
  BkPauseSessionOps ops = {p, warp, pointer, release_other};
  if ((!bindings && !bk_menu_cursor_initialize(&p->cursor, p->viewport.width,
                                               p->viewport.height)) ||
      !(p->session = bk_pause_session_create(
            services->renderer, services->resources, services->capture_files,
            16 * 1024 * 1024, p->state, &p->bindings, p->sounds, &ops,
            p->viewport.width, p->viewport.height, error)))
    goto fail;
  BkInput initial = {.pointer_x = 240, .pointer_y = 180};
  if (!bindings && !step(p, 1.0 / 60, &initial, error))
    goto fail;
  BkScene *scene =
      bk_scene_custom_create(p, (BkSceneCustomOps){step, draw, destroy}, error);
  if (!scene)
    destroy(p);
  return scene;
fail:
  destroy(p);
  return NULL;
}
int bk_pause_preview_finished_context(void *context) {
  return context && ((PausePreview *)context)->finished;
}

int bk_pause_preview_resume_context(void *context) {
  PausePreview *p = context;
  return p && p->finished && p->bindings.flow->current == 2;
}

BkScene *bk_pause_preview_create(const BkSceneServices *services,
                                 char error[256]) {
  return bk_pause_preview_create_shared(services, NULL, NULL, NULL, 0, error);
}
