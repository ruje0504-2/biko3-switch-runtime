#include "app/pause_session.h"
#include <stdlib.h>
#include <string.h>
struct BkPauseSession {
  BkPauseState *state;
  BkPauseBindings bindings;
  BkPauseSessionOps ops;
  BkCaptureFiles *files;
  BkSystemAudio *sounds[8];
  BkPauseRender *render;
  unsigned width, height;
  int retired, failed;
};
static int sound(void *p, unsigned slot, char e[256]) {
  BkPauseSession *s = p;
  if (slot >= 8 || !s->sounds[slot]) {
    snprintf(e, 256, "pause session: missing system sound%u", slot);
    return 0;
  }
  return bk_system_audio_restart(s->sounds[slot], e);
}
static int warp(void *p, float x, float y, char e[256]) {
  BkPauseSession *s = p;
  return s->ops.warp(s->ops.context, x, y, e);
}
static int pointer(void *p, float position[2], float motion[2], char e[256]) {
  BkPauseSession *s = p;
  return s->ops.pointer(s->ops.context, position, motion, e);
}
static int release(void *p, uint8_t flow, char e[256]) {
  BkPauseSession *s = p;
  if (flow == 4) {
    /* Idempotent logical release: the backdrop was selected earlier. Keep its
     * GPU resources until the owner submits that snapshot and destroys us. */
    s->retired = 1;
    return 1;
  }
  return s->ops.release_other(s->ops.context, flow, e);
}
static int remove_capture(void *p, char e[256]) {
  return bk_pause_capture_remove(((BkPauseSession *)p)->files, e);
}
static BkPauseOps operations(BkPauseSession *s) {
  return (BkPauseOps){s, sound, warp, pointer, release, remove_capture};
}
BkPauseSession *bk_pause_session_create(
    BkRenderer *r, BkResourceStore *store, BkCaptureFiles *files, size_t limit,
    BkPauseState *state, const BkPauseBindings *b,
    BkSystemAudio *const sounds[8], const BkPauseSessionOps *ops,
    unsigned width, unsigned height, char e[256]) {
  if (!state || !b || !b->common || !b->flow || !b->cursor ||
      !b->pause_overlay || !b->hover_latched || !sounds || !sounds[0] ||
      !sounds[1] || !sounds[2] || !sounds[3] || !sounds[5] || !ops ||
      !ops->warp || !ops->pointer || !ops->release_other || !width || !height) {
    snprintf(e, 256, "pause session: incomplete services");
    return NULL;
  }
  BkPauseSession *s = calloc(1, sizeof(*s));
  if (!s) {
    snprintf(e, 256, "pause session: allocation failed");
    return NULL;
  }
  s->state = state;
  s->bindings = *b;
  s->ops = *ops;
  s->files = files;
  s->width = width;
  s->height = height;
  memcpy(s->sounds, sounds, sizeof(s->sounds));
  s->render = bk_pause_resources_load(r, store, files, limit, e);
  BkPauseOps o = operations(s);
  if (!s->render || !bk_pause_initialize(state, width, &o, e)) {
    bk_pause_session_destroy(s);
    return NULL;
  }
  return s;
}
int bk_pause_session_step(BkPauseSession *s, const BkPauseInput *input,
                          BkPauseFrame *snapshot, char e[256]) {
  if (!s || !snapshot || s->failed || s->retired) {
    snprintf(e, 256, "pause session: absent, retired or failed session");
    return 0;
  }
  BkPauseFrame backdrop, ui;
  BkPauseOps ops = operations(s);
  s->failed = 1;
  if (!bk_pause_backdrop(s->state, &backdrop, e) ||
      !bk_pause_step(s->state, &s->bindings, input, &ops, &ui, e))
    return 0;
  if (backdrop.count + ui.count > BK_PAUSE_DRAWS) {
    snprintf(e, 256, "pause session: snapshot overflow");
    return 0;
  }
  memcpy(backdrop.draws + backdrop.count, ui.draws,
         ui.count * sizeof(*ui.draws));
  backdrop.count += ui.count;
  if (!bk_pause_render_prepare(s->render, &backdrop, s->width, s->height, e))
    return 0;
  *snapshot = backdrop;
  s->failed = 0;
  return 1;
}
int bk_pause_session_draw(BkPauseSession *s, char e[256]) {
  if (!s || s->failed) {
    snprintf(e, 256, "pause session: absent or failed session");
    return 0;
  }
  if (!bk_pause_render_draw(s->render, e)) {
    s->failed = 1;
    return 0;
  }
  return 1;
}
void bk_pause_session_destroy(BkPauseSession *s) {
  if (s) {
    bk_pause_render_destroy(s->render);
    free(s);
  }
}
