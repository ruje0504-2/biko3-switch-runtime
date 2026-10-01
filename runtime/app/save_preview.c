#include "app/save_preview.h"
#include "core/random.h"
#include "scene/save_menu_labels.h"
#include "scene/system_audio.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {
  BkRenderer *renderer;
  FILE *log;
  BkSaveMenuRender *render;
  BkCheckpointFiles *files;
  BkSavePreviewState *state;
  BkPauseBindings bindings;
  BkSavePreviewGame game;
  BkSavePreviewOps owner;
  BkCheckpointBank banks[5];
  BkSaveMenuRecord records[5][10];
  uint8_t labels[5][512];
  size_t lengths[5];
  BkSystemAudio *sounds[6];
  BkViewport viewport;
  BkVirtualPointer pointer;
  double elapsed;
  unsigned mode, text_group;
  int prepared, retired;
} SavePreview;
static int refresh(void *p, unsigned group, char e[256]) {
  SavePreview *s = p;
  BkCheckpointBank bank = {0};
  BkResourceResult result = bk_checkpoint_file_read(s->files, group, &bank, e);
  if (result == BK_RESOURCE_ERROR)
    return 0;
  BkSaveMenuLabel labels[10] = {0};
  for (unsigned i = 0; i < 10; ++i) {
    const BkCheckpoint *r = &bank.slots[i];
    labels[i].area = r->area;
    memcpy(labels[i].stamp, r->stamp, 32);
    /* Empty native slots have no meaningful area; sanitize the unused index
     * before the view's always-present detail lookup. Keep file bytes intact.
     */
    s->records[group][i] = (BkSaveMenuRecord){.area = r->stamp[0] ? r->area : 0,
                                              .occupied = r->stamp[0] != 0};
    memcpy(s->records[group][i].inventory, r->inventory, 5);
  }
  if (!bk_save_menu_labels(labels, s->labels[group], &s->lengths[group], e))
    return 0;
  s->banks[group] = bank;
  return 1;
}
static int load(void *p, unsigned group, unsigned slot, char e[256]) {
  SavePreview *s = p;
  const BkCheckpoint *r = &s->banks[group].slots[slot];
  if (!r->stamp[0]) {
    snprintf(e, 256, "save preview: cannot load empty checkpoint");
    return 0;
  }
  *s->game.group = group;
  *s->game.area = r->area;
  memcpy(s->game.inventory, r->inventory, 5);
  memcpy(s->game.inventory_tail, r->inventory + 5, 3);
  return 1;
}
static int store(void *p, unsigned group, unsigned slot, char e[256]) {
  SavePreview *s = p;
  if (s->log) {
    fprintf(s->log, "Checkpoint write begin group%u slot%u area%u\n", group,
            slot, *s->game.area);
    fflush(s->log);
  }
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  if (!t || group != *s->game.group) {
    snprintf(e, 256, "save preview: invalid clock/current group");
    return 0;
  }
  BkCheckpointTime stamp = {
      (unsigned)t->tm_year + 1900, (unsigned)t->tm_mon + 1,
      (unsigned)t->tm_mday,        (unsigned)t->tm_hour,
      (unsigned)t->tm_min,         (unsigned)t->tm_sec};
  uint8_t inventory[8];
  memcpy(inventory, s->game.inventory, 5);
  memcpy(inventory + 5, s->game.inventory_tail, 3);
  int ok = bk_checkpoint_file_store(
      s->files, group, slot, *s->game.area, inventory, &stamp,
      bk_random_next(s->game.random), &s->banks[group], e);
  if (s->log) {
    fprintf(s->log, "Checkpoint write %s%s%s\n", ok ? "complete" : "FAILED",
            ok ? "" : ": ", ok ? "" : e);
    fflush(s->log);
  }
  return ok;
}
static int warp(void *p, float x, float y, char e[256]) {
  (void)e;
  SavePreview *s = p;
  s->pointer.position[0] = x;
  s->pointer.position[1] = y;
  return 1;
}
static int pointer(void *p, float position[2], float motion[2], char e[256]) {
  (void)e;
  SavePreview *s = p;
  memcpy(position, s->pointer.position, sizeof(s->pointer.position));
  memcpy(motion, s->pointer.motion, sizeof(s->pointer.motion));
  return 1;
}
static int motion(void *p, float out[2], char e[256]) {
  (void)e;
  memcpy(out, ((SavePreview *)p)->pointer.motion, sizeof(float) * 2);
  return 1;
}
static int sound(void *p, unsigned slot, char e[256]) {
  SavePreview *s = p;
  if (slot >= 6 || !s->sounds[slot]) {
    snprintf(e, 256, "save preview: unbound sound%u", slot);
    return 0;
  }
  return bk_system_audio_restart(s->sounds[slot], e);
}
static int release(void *p, uint8_t flow, char e[256]) {
  SavePreview *s = p;
  if (!s->owner.release(s->owner.context, flow, e))
    return 0;
  if (flow == 0x28) {
    s->state->view.loaded = 0;
    s->retired = 1;
  }
  return 1;
}
static int clear_details(void *p, char e[256]) {
  (void)e;
  ((SavePreview *)p)->state->view.loaded &= (UINT64_C(1) << 42) - 1;
  return 1;
}
static int details(void *p, unsigned group, char e[256]) {
  SavePreview *s = p;
  return bk_save_menu_render_details(s->render, group, e) &&
         bk_save_menu_view_details(&s->state->view, group, s->viewport.width);
}
static int reset_text(void *p, char e[256]) {
  (void)e;
  SavePreview *s = p;
  s->state->text.enabled = 1;
  s->state->text.started = 0;
  return 1;
}
static int resume(void *p, char e[256]) {
  SavePreview *s = p;
  return s->owner.resume_pause(s->owner.context, e);
}
static int text(void *p, unsigned group, float seconds, char e[256]) {
  (void)seconds;
  (void)e;
  ((SavePreview *)p)->text_group = group;
  return 1;
}
void bk_save_preview_clock(BkScene *scene, double elapsed) {
  SavePreview *s = bk_scene_custom_context(scene);
  if (s)
    s->elapsed = elapsed;
}
static int step(void *p, double seconds, const BkInput *input, char e[256]) {
  SavePreview *s = p;
  if (!input || s->retired || !isfinite(seconds) || seconds <= 0 ||
      seconds > 1) {
    snprintf(e, 256, "save preview: invalid step/retired scene");
    return 0;
  }
  BkInput pointer_input = *input;
  /* D-pad retains slot/tab navigation; the left stick moves freely. */
  pointer_input.held &= ~(BK_BUTTON_UP | BK_BUTTON_DOWN |
                          BK_BUTTON_LEFT | BK_BUTTON_RIGHT);
  if (!bk_virtual_pointer_step(&s->pointer, &s->viewport, &pointer_input,
                                seconds, e))
    return 0;
  BkMenuCursor *cursor = s->bindings.cursor;
  cursor->wanted = 1;
  cursor->idle.armed = 0;
  cursor->sprite.fade.alpha = 1;
  cursor->sprite.fade.stage = 3;
  s->elapsed += seconds;
  BkSaveMenuInput in = {
      .ui = {.buttons =
                 ((input->pressed & BK_BUTTON_CONFIRM) ? BK_PAUSE_CONFIRM : 0) |
                 ((input->pressed & BK_BUTTON_UP) ? BK_PAUSE_UP : 0) |
                 ((input->pressed & BK_BUTTON_DOWN) ? BK_PAUSE_DOWN : 0) |
                 ((input->pressed & BK_BUTTON_LEFT) ? BK_PAUSE_LEFT : 0) |
                 ((input->pressed & BK_BUTTON_RIGHT) ? BK_PAUSE_RIGHT : 0),
             .seconds = (float)seconds,
             .now_ms = (uint32_t)(uint64_t)(s->elapsed * 1000),
             .scale = s->viewport.width / 1280.f},
      .current_group = *s->game.group};
  if ((input->pressed & BK_BUTTON_BACK) && !s->bindings.common->blocked &&
      s->state->control.page <= 1) {
    const float *q = s->state->control.page ? s->state->control.no
                                           : s->state->control.back;
    s->pointer.position[0] = q[0]+q[2]/2;
    s->pointer.position[1] = q[1]+q[3]/2;
    memset(s->pointer.motion,0,sizeof(s->pointer.motion));
    in.ui.buttons = BK_PAUSE_CONFIRM;
  }
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned k = 0; k < 10; ++k)
      in.occupied[g][k] = s->records[g][k].occupied;
  BkSaveMenuOps ops = {.menu = {.context = s,
                                .sound = sound,
                                .warp = warp,
                                .pointer = pointer,
                                .release = release},
                       .load = load,
                       .store = store,
                       .refresh_bank = refresh,
                       .clear_details = clear_details,
                       .load_details = details,
                       .reset_text = reset_text,
                       .resume_pause = resume};
  BkSaveMenuViewOps view_ops = {s, text, motion};
  BkSaveMenuFrame frame;
  s->prepared = 0;
  if (!bk_save_menu_control(&s->state->control, &s->bindings, &in, &ops, e) ||
      !bk_save_menu_view_step(&s->state->view, &s->state->control, &s->bindings,
                              &in.ui, s->mode, s->records, &view_ops, &frame,
                              e) ||
      !bk_save_menu_render_prepare(s->render, &frame, s->labels[s->text_group],
                                   s->lengths[s->text_group], &s->state->text,
                                   (float)seconds, s->viewport.width,
                                   s->viewport.height, e))
    return 0;
  s->prepared = 1;
  return 1;
}
static int draw(void *p, const BkSceneFrame *frame, char e[256]) {
  (void)frame;
  SavePreview *s = p;
  if (!s->prepared) {
    snprintf(e, 256, "save preview: missing snapshot");
    return 0;
  }
  return bk_renderer_viewport(s->renderer, &s->viewport, e) &&
         bk_save_menu_render_draw(s->render, e) &&
         bk_renderer_viewport(s->renderer, NULL, e);
}
static void destroy(void *p) {
  SavePreview *s = p;
  bk_save_menu_render_destroy(s->render);
  for (unsigned i = 0; i < 6; ++i)
    bk_system_audio_destroy(s->sounds[i]);
  free(s);
}
BkScene *bk_save_preview_create(const BkSceneServices *services,
                                BkCheckpointFiles *files,
                                BkSavePreviewState *state,
                                const BkPauseBindings *b,
                                const BkSavePreviewGame *game,
                                const BkSavePreviewOps *ops, double elapsed,
                                char e[256]) {
  if (!services || !services->renderer || !services->resources ||
      !services->audio || !files || !state || !b || !b->flow || !b->common ||
      !b->cursor || !b->hover_latched || !game || !game->group || !game->area ||
      !game->random || !game->inventory || !game->inventory_tail || !ops ||
      !ops->release || !ops->resume_pause || !isfinite(elapsed) ||
      elapsed < 0 || state->control.tab < 3 || state->control.tab > 7 ||
      (b->flow->previous != 1 && b->flow->previous != 4 &&
       b->flow->previous != 0x20)) {
    snprintf(e, 256, "save preview: invalid services/process state");
    return NULL;
  }
  SavePreview *s = calloc(1, sizeof(*s));
  if (!s) {
    snprintf(e, 256, "save preview: allocation failed");
    return NULL;
  }
  s->renderer = services->renderer;
  s->log = services->log;
  s->files = files;
  s->state = state;
  s->bindings = *b;
  s->game = *game;
  s->owner = *ops;
  s->elapsed = elapsed;
  s->mode = b->flow->previous == 0x20;
  unsigned group = (unsigned)(state->control.tab - 3), width, height;
  bk_renderer_extent(s->renderer, &width, &height);
  if (!bk_camera_fit(&s->viewport, width, height, 4, 3))
    goto bad;
  s->render = bk_save_menu_render_create(s->renderer, services->resources,
                                         s->mode, group, e);
  if (!s->render || !bk_save_menu_view_initialize(&state->view, s->mode, group,
                                                  s->viewport.width))
    goto bad;
  memcpy(state->control.back, state->view.sprites[1].rect, 16);
  memcpy(state->control.yes, state->view.sprites[10].rect, 16);
  memcpy(state->control.no, state->view.sprites[12].rect, 16);
  for (unsigned g = 0; g < 5; ++g)
    if (!refresh(s, g, e))
      goto bad;
  reset_text(s, e);
  const unsigned slots[] = {0, 2, 3, 5};
  for (unsigned i = 0; i < 4; ++i) {
    unsigned slot = slots[i];
    s->sounds[slot] = bk_system_audio_create_slot(
        services->resources, services->audio, 48 + slot, slot, bk_volume_get(services->audio_volumes, BK_VOLUME_EFFECT, -600), e);
    if (!s->sounds[slot])
      goto bad;
  }
  /* The native constructor does not warp the shared pointer. */
  memcpy(s->pointer.position, state->control.cursor, sizeof(s->pointer.position));
  BkScene *scene =
      bk_scene_custom_create(s, (BkSceneCustomOps){step, draw, destroy}, e);
  if (scene)
    return scene;
bad:
  destroy(s);
  return NULL;
}
