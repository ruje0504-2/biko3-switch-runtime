#include "app/retry_preview.h"
#include "scene/pause_render.h"
#include "scene/system_audio.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkRenderer *renderer;
  BkPauseRender *render;
  BkViewport viewport;
  BkRetryState *state;
  BkCheckpointPrompt *checkpoint;
  uint8_t *phase;
  float *reserve;
  BkPauseBindings bindings;
  BkFlowTransitionOps release;
  BkSystemAudio *sounds[4];
  float point[2], motion[2];
  double elapsed;
  int prepared;
} ChoicePreview;
static int warp(void *context, float x, float y, char error[256]) {
  (void)error;
  ChoicePreview *r = context;
  r->point[0] = x;
  r->point[1] = y;
  return 1;
}
static int pointer(void *context, float point[2], float motion[2],
                   char error[256]) {
  (void)error;
  ChoicePreview *r = context;
  memcpy(point, r->point, sizeof(r->point));
  memcpy(motion, r->motion, sizeof(r->motion));
  return 1;
}
static int sound(void *context, unsigned slot, char error[256]) {
  ChoicePreview *r = context;
  if (slot >= 4 || !r->sounds[slot]) {
    snprintf(error, 256, "retry preview: unbound sound%u", slot);
    return 0;
  }
  return bk_system_audio_restart(r->sounds[slot], error);
}
static int release(void *context, uint8_t flow, char error[256]) {
  ChoicePreview *r = context;
  return r->release.release(r->release.context, flow, error);
}
void bk_retry_preview_clock(BkScene *scene, double elapsed) {
  ChoicePreview *r = bk_scene_custom_context(scene);
  if (r)
    r->elapsed = elapsed;
}
static int step(void *context, double seconds, const BkInput *input,
                char error[256]) {
  ChoicePreview *r = context;
  if (!input || !isfinite(seconds) || seconds <= 0 || seconds > 1) {
    snprintf(error, 256, "retry preview: invalid time/input");
    return 0;
  }
  if (input->pointer_active) {
    r->point[0] = input->pointer_x - r->viewport.x;
    r->point[1] = input->pointer_y - r->viewport.y;
  }
  r->motion[0] = input->pointer_motion_x;
  r->motion[1] = input->pointer_motion_y;
  r->elapsed += seconds;
  BkPauseInput in = {
      .buttons = ((input->pressed & BK_BUTTON_CONFIRM) ? BK_PAUSE_CONFIRM : 0) |
                 ((input->pressed & BK_BUTTON_LEFT) ? BK_PAUSE_LEFT : 0) |
                 ((input->pressed & BK_BUTTON_RIGHT) ? BK_PAUSE_RIGHT : 0),
      .now_ms = (uint32_t)(uint64_t)(r->elapsed * 1000),
      .seconds = (float)seconds,
      .scale = r->viewport.width / 1280.f};
  BkPauseOps ops = {.context = r,
                    .sound = sound,
                    .warp = warp,
                    .pointer = pointer,
                    .release = release};
  BkPauseFrame frame;
  int ok =
      r->checkpoint
          ? bk_checkpoint_prompt_step(r->checkpoint, &r->bindings, r->phase,
                                      r->reserve, &in, &ops, &frame, error)
          : bk_retry_step(r->state, &r->bindings, &in, &ops, &frame, error);
  if (!ok || !bk_pause_render_prepare(r->render, &frame, r->viewport.width,
                                      r->viewport.height, error))
    return 0;
  r->prepared = 1;
  return 1;
}
static int draw(void *context, const BkSceneFrame *frame, char error[256]) {
  (void)frame;
  ChoicePreview *r = context;
  if (!r->prepared) {
    snprintf(error, 256, "retry preview: draw before prepare");
    return 0;
  }
  return bk_renderer_viewport(r->renderer, &r->viewport, error) &&
         bk_pause_render_draw(r->render, error) &&
         bk_renderer_viewport(r->renderer, NULL, error);
}
static void destroy(void *context) {
  ChoicePreview *r = context;
  bk_pause_render_destroy(r->render);
  for (unsigned i = 0; i < 4; ++i)
    bk_system_audio_destroy(r->sounds[i]);
  free(r);
}
static BkScene *create(const BkSceneServices *services, BkRetryState *state,
                       const BkPauseBindings *bindings,
                       const BkFlowTransitionOps *flow_ops, double elapsed,
                       BkCheckpointPrompt *checkpoint, uint8_t *phase,
                       float *reserve, char error[256]) {
  if (!services || !services->renderer || !services->resources ||
      !services->audio || (!state && !checkpoint) ||
      (checkpoint && (!phase || !reserve)) || !bindings || !bindings->common ||
      !bindings->flow || !bindings->cursor || !bindings->hover_latched ||
      !flow_ops || !flow_ops->release || !isfinite(elapsed) || elapsed < 0) {
    snprintf(error, 256, "retry preview: missing process bindings/services");
    return NULL;
  }
  ChoicePreview *r = calloc(1, sizeof(*r));
  if (!r) {
    snprintf(error, 256, "retry preview: allocation failed");
    return NULL;
  }
  r->renderer = services->renderer;
  r->state = state;
  r->checkpoint = checkpoint;
  r->phase = phase;
  r->reserve = reserve;
  r->bindings = *bindings;
  r->release = *flow_ops;
  r->elapsed = elapsed;
  unsigned width, height;
  bk_renderer_extent(r->renderer, &width, &height);
  if (!bk_camera_fit(&r->viewport, width, height, 4, 3))
    goto bad;
  const unsigned slots[] = {0, 2, 3};
  for (unsigned i = 0; i < 3; ++i) {
    unsigned slot = slots[i];
    r->sounds[slot] = bk_system_audio_create_slot(
        services->resources, services->audio, 48 + slot, slot, bk_volume_get(services->audio_volumes, BK_VOLUME_EFFECT, -600), error);
    if (!r->sounds[slot])
      goto bad;
  }
  r->render = checkpoint ? bk_pause_render_create_checkpoint(
                               r->renderer, services->resources, error)
                         : bk_pause_render_create_retry(
                               r->renderer, services->resources, error);
  BkPauseOps ops = {.context = r, .warp = warp};
  if (!r->render ||
      !(checkpoint
            ? bk_checkpoint_prompt_initialize(checkpoint, r->viewport.width,
                                              &ops, error)
            : bk_retry_initialize(state, r->viewport.width, &ops, error)))
    goto bad;
  BkScene *scene =
      bk_scene_custom_create(r, (BkSceneCustomOps){step, draw, destroy}, error);
  if (scene)
    return scene;
bad:
  destroy(r);
  return NULL;
}

BkScene *bk_retry_preview_create(const BkSceneServices *services,
                                 BkRetryState *state,
                                 const BkPauseBindings *bindings,
                                 const BkFlowTransitionOps *ops, double elapsed,
                                 char error[256]) {
  return create(services, state, bindings, ops, elapsed, NULL, NULL, NULL,
                error);
}
BkScene *bk_checkpoint_preview_create(const BkSceneServices *services,
                                      BkCheckpointPrompt *state,
                                      const BkPauseBindings *bindings,
                                      const BkFlowTransitionOps *ops,
                                      uint8_t *phase, float *reserve,
                                      double elapsed, char error[256]) {
  return create(services, NULL, bindings, ops, elapsed, state, phase, reserve,
                error);
}
