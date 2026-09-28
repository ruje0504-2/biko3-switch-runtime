#include "scene/dialogue_session.h"
#include "scene/dialogue_backdrop_render.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkDialogueSession {
  BkDialogueSessionConfig c;
  BkDialogueWorld *world;
  BkDialogueRender *render;
  BkDialogueBackdropRender *background;
  BkDialogueUiRender *ui;
  BkDialogueAudio *audio;
  BkDialogueSessionInput input;
  int ready, pending, drawn, released, transition, failed;
  uint8_t target, mode;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "dialogue session: %s", why);
  return 0;
}
int bk_dialogue_session_stop(BkDialogueSession *s, char e[256]) {
  if (!s)
    return fail(e, "missing owner");
  if (s->released)
    return 1;
  if (s->audio && !bk_dialogue_audio_stop(s->audio, e))
    return 0;
  bk_dialogue_assets_close(&s->c.state->script);
  s->c.state->actor.speaker = 0;
  s->c.state->actor.rules.clip = 0;
  s->released = 1;
  return 1;
}
void bk_dialogue_session_destroy(BkDialogueSession *s) {
  if (!s)
    return;
  char ignored[256];
  bk_dialogue_session_stop(s, ignored);
  bk_dialogue_audio_destroy(s->audio);
  bk_dialogue_ui_render_destroy(s->ui);
  bk_dialogue_backdrop_render_destroy(s->background);
  bk_dialogue_render_destroy(s->render);
  bk_dialogue_world_destroy(s->world);
  free(s);
}
BkDialogueSession *bk_dialogue_session_create(BkRenderer *renderer,
                                              BkResourceStore *store,
                                              BkAudio *mixer,
                                              const BkDialogueSessionConfig *c,
                                              char e[256]) {
  if (!renderer || !store || !mixer || !c || !c->state || !c->camera ||
      !c->common || !c->common_render || !c->backdrop_curtain ||
      !c->backdrop_curtain_wanted || !c->result || !c->envelope || !c->random ||
      !c->unlock || !c->schedule || c->group >= 5 || !c->width || !c->height ||
      c->state->script.raw.data || !bk_fog_validate(&c->fog, e)) {
    fail(e, "invalid services/state or previous script still owned");
    return NULL;
  }
  BkDialogueEntry entry;
  if (!bk_dialogue_entry_select(&entry, c->previous, (int32_t)c->group, c->area,
                                c->response, e))
    return NULL;
  BkDialogueSession *s = calloc(1, sizeof(*s));
  if (!s) {
    fail(e, "allocation failed");
    return NULL;
  }
  s->c = *c;
  BkDialogueSessionState *state = c->state;
  bk_dialogue_ui_initialize(&state->ui);
  bk_dialogue_actor_initialize(&state->actor, c->group);
  s->world = bk_dialogue_world_create(store, c->group, c->camera, c->clocks,
                                      c->random, e);
  if (!s->world || !bk_dialogue_entry_open(&state->script, &state->text,
                                           &state->backdrop, store, &entry, e))
    goto bad;
  s->ui = bk_dialogue_ui_render_create(renderer, store, e);
  s->background = bk_dialogue_backdrop_render_create(
      renderer, store, state->script.state.image, e);
  s->render = bk_dialogue_render_create(renderer, store, s->world, e);
  s->audio = bk_dialogue_audio_create(store, mixer, c->music_voice,
                                      c->speech_voice, e);
  if (!s->ui || !s->background || !s->render || !s->audio ||
      !bk_dialogue_audio_music_open(s->audio, &state->music,
                                    &state->script.state, e))
    goto bad;
  state->ui.text_parameter = (int32_t)state->timer.duration;
  return s;
bad:
  bk_dialogue_session_destroy(s);
  return NULL;
}
static int replace(void *p, const char *name, char e[256]) {
  return bk_dialogue_backdrop_render_replace(
      ((BkDialogueSession *)p)->background, name, e);
}
static int pause_voice(void *p, char e[256]) {
  return bk_dialogue_audio_speech_pause(((BkDialogueSession *)p)->audio, e);
}
static int next_page(void *p, int *done, char e[256]) {
  return bk_dialogue_assets_next(&((BkDialogueSession *)p)->c.state->script,
                                 done, e);
}
static int bind_text(void *p, char e[256]) {
  BkDialogueSession *s = p;
  /*4aa9f4 only stores the message pointer. The retained script.text address
   * is already the stable source used by prepare_text on every call. */
  return s->c.state->script.raw.data ? 1 : fail(e, "no owned text source");
}
static int prepare_text(void *p, float dt, char e[256]) {
  BkDialogueSession *s = p;
  BkDialogueSessionState *state = s->c.state;
  return bk_dialogue_ui_render_text(s->ui, &state->script.state.text,
                                    &state->text, dt, s->c.width, s->c.height,
                                    e);
}
static int speech(void *p, char e[256]) {
  BkDialogueSession *s = p;
  return bk_dialogue_audio_speech_step(s->audio, &s->c.state->script.state,
                                       s->input.voice_volume, e);
}
static int music(void *p, char e[256]) {
  BkDialogueSession *s = p;
  return bk_dialogue_audio_music_step(
      s->audio, &s->c.state->music, &s->c.state->script.state, s->input.seconds,
      s->input.music_volume, e);
}
static int unlock(void *p, unsigned group, char e[256]) {
  BkDialogueSession *s = p;
  return s->c.unlock(s->c.context, group, e);
}
static int schedule(void *p, uint8_t target, uint8_t mode, char e[256]) {
  BkDialogueSession *s = p;
  if (s->transition)
    return fail(e, "duplicate transition request");
  s->target = target;
  s->mode = mode;
  s->transition = 1;
  return 1;
}
int bk_dialogue_session_step(BkDialogueSession *s,
                             const BkDialogueSessionInput *in, char e[256]) {
  if (!s || !in || s->released || s->pending || s->failed ||
      !isfinite(in->seconds) || in->seconds < 0 ||
      (double)in->seconds * 60 >= INT32_MAX ||
      (in->advance != 0 && in->advance != 1) || in->music_volume < -10000 ||
      in->music_volume > 0 || in->voice_volume < -10000 || in->voice_volume > 0)
    return fail(e, "invalid input or previous snapshot not presented");
  s->ready = 0;
  s->input = *in;
  s->failed = 1;
  BkDialogueSessionState *state = s->c.state;
  BkDialogue *d = &state->script.state;
  BkDialogueActorInput actor = {.game_seconds = in->seconds,
                                .timestamp_ms = in->timestamp_ms,
                                .timer_clock_ms = in->timer_clock_ms};
  memcpy(actor.face_clocks, in->face_clocks, sizeof(actor.face_clocks));
  if (state->actor.speaker == d->code_c && state->phase == 0 &&
      !bk_dialogue_audio_voice_level(s->audio, s->c.envelope, in->seconds,
                                     &actor.mouth_level, e))
    return 0;
  if (!bk_dialogue_world_step(s->world, &state->actor, d, &state->phase,
                              &state->timer, &actor, s->c.random, e))
    return 0;
  BkDialogueBackdropFrame background;
  BkDialogueBackdropBindings bb = {
      &state->phase, &state->actor.rules.expression, s->c.backdrop_curtain,
      s->c.backdrop_curtain_wanted, d->image};
  if (!bk_dialogue_backdrop_step(&state->backdrop, &bb,
                                 &(BkDialogueBackdropOps){s, replace},
                                 in->seconds, &background, e) ||
      !bk_dialogue_backdrop_render_prepare(s->background, &background,
                                           s->c.width, s->c.height, e) ||
      !bk_dialogue_render_prepare(s->render, s->c.camera, &s->c.fog, e))
    return 0;
  BkDialogueUiBindings ub = {d,
                             &state->text,
                             &state->backdrop,
                             &state->phase,
                             &s->c.common->curtain,
                             &s->c.common->blocked,
                             s->c.result};
  BkDialogueUiInput ui = {.seconds = in->seconds,
                          .advance = in->advance,
                          .voice_present =
                              bk_dialogue_audio_speech_present(s->audio),
                          .previous = s->c.previous,
                          .response = s->c.response,
                          .group = s->c.group,
                          .area = s->c.area};
  BkDialogueUiOps ops = {s,         pause_voice,  next_page,
                         bind_text, prepare_text, speech,
                         music,     unlock,       schedule};
  BkDialogueUiFrame frame;
  int ok = bk_dialogue_ui_step(&state->ui, &ub, &ui, &ops, &frame, e);
  state->timer.duration = (uint32_t)state->ui.text_parameter;
  if (!ok ||
      !bk_dialogue_ui_render_prepare(s->ui, &frame, s->c.width, s->c.height,
                                     e) ||
      !bk_curtain_render_prepare(s->c.common_render,
                                 &(BkCommonHudFrame){frame.curtain_alpha},
                                 s->c.width, s->c.height, e))
    return 0;
  s->ready = s->pending = 1;
  s->drawn = 0;
  s->failed = 0;
  return 1;
}
int bk_dialogue_session_draw(BkDialogueSession *s, char e[256]) {
  if (!s || !s->ready)
    return fail(e, "no prepared snapshot");
  if (!bk_dialogue_backdrop_render_draw(s->background, e) ||
      !bk_dialogue_render_draw(s->render, e) ||
      !bk_dialogue_ui_render_draw(s->ui, e) ||
      !bk_curtain_render_draw(s->c.common_render, e))
    return 0;
  s->drawn = 1;
  return 1;
}
int bk_dialogue_session_after_present(BkDialogueSession *s, char e[256]) {
  if (!s || !s->pending || !s->drawn || s->failed)
    return fail(e, "no pending submitted snapshot");
  s->pending = 0;
  if (s->transition) {
    s->failed = 1;
    if (!s->c.schedule(s->c.context, s->target, s->mode, e))
      return 0;
    s->transition = 0;
    s->failed = 0;
  }
  return 1;
}
int bk_dialogue_session_released(const BkDialogueSession *s) {
  return s && s->released;
}
BkDialogueWorld *bk_dialogue_session_world(BkDialogueSession *s) {
  return s ? s->world : NULL;
}
