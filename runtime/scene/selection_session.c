#include "scene/selection_session.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkSelectionSession {
  BkSelectionSessionConfig config;
  BkRenderer *renderer;
  BkResourceStore *store;
  BkSelectionAudio *audio;
  BkSelectionWorld *world;
  BkSelectionRender *active, *draw, *retired;
  BkSelectionActorAssets *retired_body;
  BkSelectionUiRender *ui_render;
  BkSelectionFrame frame;
  uint32_t reload_clocks[4];
  int32_t movie_clock_ms, movie_restart_clock_ms, reload_movie_clock_ms;
  int32_t voice_volume;
  int released, pending, drawn, ready;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "selection session: %s", why);
  return 0;
}
static int sound(void *p, unsigned slot, char e[256]) {
  BkSelectionSession *s = p;
  return slot < 8 && s->config.sounds[slot]
             ? bk_system_audio_restart(s->config.sounds[slot], e)
             : fail(e, "missing process system sound");
}
static int music(void *p, int32_t volume, char e[256]) {
  return bk_selection_audio_music_gain(((BkSelectionSession *)p)->audio, volume,
                                       e);
}
static int position(void *p, float out[2], char e[256]) {
  BkSelectionPointer *o = &((BkSelectionSession *)p)->config.pointer;
  return o->position(o->context, out, e);
}
static int motion(void *p, float out[2], char e[256]) {
  BkSelectionPointer *o = &((BkSelectionSession *)p)->config.pointer;
  return o->motion(o->context, out, e);
}
static int warp(void *p, float x, float y, char e[256]) {
  BkSelectionPointer *o = &((BkSelectionSession *)p)->config.pointer;
  return o->warp(o->context, x, y, e);
}
static int voice_stop(void *p, char e[256]) {
  return bk_selection_audio_voice_stop(((BkSelectionSession *)p)->audio, e);
}
static int voice_play(void *p, int32_t volume, char e[256]) {
  return bk_selection_audio_voice_play(((BkSelectionSession *)p)->audio, volume,
                                       e);
}
static int voice_status(void *p, int *playing, char e[256]) {
  return bk_selection_audio_voice_status(((BkSelectionSession *)p)->audio,
                                         playing, e);
}
static int voice_level(void *p, float seconds, float *out, char e[256]) {
  BkSelectionSession *s = p;
  return bk_selection_audio_voice_level(s->audio, s->config.envelope, seconds,
                                        out, e);
}
static int movie_step(void *p, char e[256]) {
  BkSelectionSession *s = p;
  return bk_selection_render_movie_step(s->active, s->movie_clock_ms,
                                        s->movie_restart_clock_ms, e);
}
static int replace(void *p, unsigned group, char e[256]) {
  BkSelectionSession *s = p;
  if (group >= 5 || s->retired || s->retired_body)
    return fail(e, "invalid replacement lifecycle");
  if (!bk_selection_audio_release_voice(s->audio, e))
    return 0;
  uint8_t alternate;
  if (!bk_selection_actor_variant(s->config.unlocked[group], s->config.random,
                                  &alternate))
    return fail(e, "invalid unlock state");
  if (!bk_selection_world_replace(s->world, group, alternate, s->reload_clocks,
                                  s->config.random, &s->retired_body, e))
    return 0;
  BkSelectionRender *next = bk_selection_render_create_at(
      s->renderer, s->store, s->world, s->reload_movie_clock_ms, e);
  if (!next)
    return 0;
  s->retired = s->active;
  s->active = next;
  return bk_selection_audio_bind_voice(
      s->audio,
      bk_selection_actor_assets_voice(bk_selection_world_body(s->world)),
      s->voice_volume, e);
}
static int release(void *p, uint8_t flow, char e[256]) {
  BkSelectionSession *s = p;
  if (flow != 0x38 || s->released)
    return fail(e, "cannot release unowned selection flow");
  if (!bk_selection_audio_stop(s->audio, e))
    return 0;
  s->released = 1;
  return 1;
}
static void collect(BkSelectionSession *s) {
  bk_selection_render_destroy(s->retired);
  s->retired = NULL;
  bk_selection_actor_assets_destroy(s->retired_body);
  s->retired_body = NULL;
}
void bk_selection_session_destroy(BkSelectionSession *s) {
  if (!s)
    return;
  if (s->audio) {
    char ignored[256];
    bk_selection_audio_stop(s->audio, ignored);
  }
  bk_selection_audio_destroy(s->audio);
  bk_selection_ui_render_destroy(s->ui_render);
  bk_selection_render_destroy(s->active);
  collect(s);
  bk_selection_world_destroy(s->world);
  free(s);
}
BkSelectionSession *
bk_selection_session_create(BkRenderer *renderer, BkResourceStore *store,
                            BkAudio *audio, const BkSelectionSessionConfig *c,
                            char e[256]) {
  if (!renderer || !store || !audio || !c || !c->ui || !c->camera ||
      !c->envelope || !c->random || !c->unlocked || !c->bindings.common ||
      !c->bindings.flow || !c->bindings.cursor || !c->bindings.hover_latched ||
      !c->bindings.photos || !c->bindings.group || !c->bindings.area ||
      !c->bindings.photo_count || *c->bindings.group < 0 ||
      *c->bindings.group >= 5 || !c->pointer.position || !c->pointer.motion ||
      !c->pointer.warp || !c->width || !c->height ||
      (c->bindings.flow->current != 0x38 &&
       !(c->bindings.flow->current == 0x50 &&
         c->bindings.flow->target == 0x38)) ||
      c->voice_volume < -10000 || c->voice_volume > 0 ||
      !bk_fog_validate(&c->fog, e)) {
    fail(e, "invalid retained state/services");
    return NULL;
  }
  for (unsigned i = 1; i <= 4; ++i)
    if (!c->sounds[i]) {
      fail(e, "missing persistent system sound");
      return NULL;
    }
  uint32_t random = *c->random;
  uint8_t alternate;
  if (!bk_selection_actor_variant(c->unlocked[0], &random, &alternate)) {
    fail(e, "invalid unlock state");
    return NULL;
  }
  BkSelectionSession *s = calloc(1, sizeof(*s));
  if (!s) {
    fail(e, "allocation failed");
    return NULL;
  }
  s->config = *c;
  s->renderer = renderer;
  s->store = store;
  s->voice_volume = c->voice_volume;
  BkSelectionUi ui = *c->ui;
  if (!bk_selection_ui_initialize(&ui, c->width, 0, c->music_volume, e))
    goto bad;
  s->ui_render = bk_selection_ui_render_create(renderer, store, 0, e);
  if (!s->ui_render)
    goto bad;
  s->audio = bk_selection_audio_create(store, audio, c->music_voice,
                                       c->speech_voice, c->music_volume, e);
  if (!s->audio)
    goto bad;
  s->world = bk_selection_world_create(store, (unsigned)*c->bindings.group,
                                       alternate, c->loading_seconds, c->camera,
                                       c->clocks, &random, e);
  if (!s->world)
    goto bad;
  s->active = bk_selection_render_create_at(renderer, store, s->world,
                                            c->movie_clock_ms, e);
  if (!s->active ||
      !bk_selection_audio_bind_voice(
          s->audio,
          bk_selection_actor_assets_voice(bk_selection_world_body(s->world)),
          c->voice_volume, e))
    goto bad;
  *c->ui = ui;
  *c->random = random;
  return s;
bad:
  bk_selection_session_destroy(s);
  return NULL;
}
int bk_selection_session_step(BkSelectionSession *s,
                              const BkSelectionSessionInput *in, char e[256]) {
  if (!s || !in || s->pending || s->released || in->ui.special ||
      !isfinite(in->ui.seconds) || in->ui.seconds < 0 ||
      in->ui.voice_present != 1)
    return fail(e, "invalid retail step or previous frame not presented");
  s->ready = 0;
  collect(s);
  BkSelectionUi *ui = s->config.ui;
  BkSelectionWorldInput world = {.selected = (unsigned)ui->selected,
                                 .camera_mode = (unsigned)ui->camera_mode,
                                 .buttons = in->camera_buttons,
                                 .voice_active = ui->voice_active,
                                 .seconds = in->ui.seconds,
                                 .timestamp = in->ui.now_ms};
  memcpy(world.face_clocks, in->face_clocks, sizeof(world.face_clocks));
  memcpy(s->reload_clocks, in->reload_clocks, sizeof(s->reload_clocks));
  s->voice_volume = in->ui.voice_master;
  s->movie_clock_ms = in->movie_clock_ms;
  s->movie_restart_clock_ms = in->movie_restart_clock_ms;
  s->reload_movie_clock_ms = in->reload_movie_clock_ms;
  if ((!world.camera_mode && !motion(s, world.motion, e)) ||
      !bk_selection_world_step(
          s->world, &world,
          &(BkSelectionWorldOps){.context = s,
                                 .movie_step = movie_step,
                                 .voice_level = voice_level},
          s->config.random, e) ||
      !bk_selection_render_prepare(s->active, s->config.camera, &s->config.fog,
                                   e))
    return 0;
  s->draw = s->active;
  BkSelectionOps ops = {s,      sound,      music,      position,     motion,
                        warp,   voice_stop, voice_play, voice_status, replace,
                        release};
  if (!bk_selection_ui_view(ui, &s->config.bindings, &in->ui, &ops, &s->frame,
                            e) ||
      !bk_selection_ui_render_prepare(s->ui_render, &s->frame, s->config.width,
                                      s->config.height, e) ||
      !bk_selection_ui_control(ui, &s->config.bindings, &in->ui, &ops, e))
    return 0;
  s->ready = s->pending = 1;
  s->drawn = 0;
  return 1;
}
int bk_selection_session_draw(BkSelectionSession *s, char e[256]) {
  if (!s || !s->ready)
    return fail(e, "no prepared frame");
  if (!bk_selection_render_draw(s->draw, e) ||
      !bk_selection_ui_render_draw(s->ui_render, e))
    return 0;
  s->drawn = 1;
  return 1;
}
int bk_selection_session_after_present(BkSelectionSession *s, char e[256]) {
  if (!s || !s->pending || !s->drawn)
    return fail(e, "missing pending submitted frame");
  s->pending = 0;
  return 1;
}
int bk_selection_session_released(const BkSelectionSession *s) {
  return s && s->released;
}
BkSelectionWorld *bk_selection_session_world(BkSelectionSession *s) {
  return s ? s->world : NULL;
}
