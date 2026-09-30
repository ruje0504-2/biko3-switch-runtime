#include "scene/special_session.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
struct BkSpecialSession {
  BkSpecialSessionConfig c;
  BkRenderer *renderer;
  BkResourceStore *store;
  BkAudio *audio;
  BkSpecialWorld *world;
  BkSpecialRender *render;
  BkSpecialAudio *sound;
  BkSpecialUiRender *ui;
  BkSpecialCaptureService capture;
  BkSpecialEventBindings bindings;
  BkSpecialUiBindings ui_bindings;
  BkSpecialUiOps ui_ops;
  BkVirtualPointer pointer;
  BkInput input;
  float seconds;
  int8_t packed, special;
  uint8_t target, mode;
  int pending, drawn, ui_ready, stopped, transition;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "special session: %s", why); return 0;
}
static int clock_read(void *p, int timer, uint32_t *out, char e[256]) {
  BkSpecialSession *s = p;
  return s->c.clock(s->c.context, timer, out, e);
}
static int ui_clock(void *p, uint32_t *out, char e[256]) {
  return clock_read(p, 1, out, e);
}
static int image(void *p, unsigned slot, const char *name, char e[256]) {
  return bk_special_ui_render_image(((BkSpecialSession *)p)->ui, slot, name, e);
}
static int capture(void *p, BkSpecialCapture op, char e[256]) {
  return bk_special_capture_call(&((BkSpecialSession *)p)->capture, op, e);
}
static int position(void *p, float out[2], char e[256]) {
  (void)e; memcpy(out, ((BkSpecialSession *)p)->pointer.position, 8); return 1;
}
static int motion(void *p, float out[2], char e[256]) {
  (void)e; memcpy(out, ((BkSpecialSession *)p)->pointer.motion, 8); return 1;
}
static int warp(void *p, float x, float y, char e[256]) {
  BkSpecialSession *s = p;
  if (!isfinite(x) || !isfinite(y)) return fail(e, "invalid pointer warp");
  s->pointer.position[0] = x; s->pointer.position[1] = y; return 1;
}
static int key(void *p, uint32_t code, unsigned mode, uint32_t *out, char e[256]) {
  BkSpecialSession *s = p;
  if (mode != 1 && mode != 2) return fail(e, "unknown key polling mode");
  uint32_t button;
  if (code == 0) button = mode == 2 ? BK_BUTTON_CAMERA_ORBIT : BK_BUTTON_CONFIRM;
  else if (code == 1) button = BK_BUTTON_CAMERA_ADJUST;
  else if (code == 0x5a || code == 0x33450) button = BK_BUTTON_CONFIRM;
  else if (code == 0x43 || code == 0x33455) button = BK_BUTTON_PHOTO;
  else if (code == 0x26 || code == 0x30d40) button = BK_BUTTON_UP;
  else if (code == 0x28 || code == 0x30d41) button = BK_BUTTON_DOWN;
  else return fail(e, "unknown input binding");
  *out = !!((mode == 1 ? s->input.pressed : s->input.held) & button);
  return 1;
}
static int world_key(void *p, uint32_t code, uint8_t *out, char e[256]) {
  uint32_t value;
  if (!key(p, code, 1, &value, e)) return 0;
  *out = (uint8_t)value; return 1;
}
static int sound(void *p, unsigned slot, char e[256]) {
  BkSpecialSession *s = p;
  return slot < 8 && s->c.sounds[slot]
      ? bk_system_audio_restart(s->c.sounds[slot], e) : fail(e, "unloaded system sound");
}
static int schedule(void *p, uint8_t target, uint8_t mode, char e[256]) {
  BkSpecialSession *s = p;
  if (s->transition) return fail(e, "duplicate exit request");
  /*Actual release happens after present, outside the active GPU frame.*/
  s->transition = 1; s->target = target; s->mode = mode; return 1;
}
static int present(void *p, BkSpecialEventObject object, int *out, char e[256]) {
  BkSpecialSession *s = p;
  if (object == BK_SPECIAL_MOVIE) {
    *out = bk_special_render_movie_image(s->render) != NULL; return 1;
  }
  if (object >= BK_SPECIAL_EFFECT0 && object <= BK_SPECIAL_EFFECT3)
    return bk_special_audio_present(s->sound, object - BK_SPECIAL_EFFECT0, out);
  return fail(e, "unknown media presence query");
}
static int audio_call(void *p, const BkSpecialEventAudioCall *call, char e[256]) {
  BkSpecialSession *s = p;
  return bk_special_audio_call(s->sound, call, s->c.music_volume,
                                s->c.voice_volume, s->c.effect_volume, e);
}
static int movie_clock(void *p, int32_t *out, char e[256]) {
  uint32_t ms;
  if (!clock_read(p, 2, &ms, e)) return 0;
  memcpy(out, &ms, sizeof(ms)); return 1;
}
static int movie(void *p, char e[256]) {
  BkSpecialSession *s = p;
  return bk_special_render_movie_poll(s->render, movie_clock, s, e);
}
static int level(void *p, unsigned effect, float seconds, float *out, char e[256]) {
  BkSpecialSession *s = p;
  return bk_special_audio_level(s->sound, effect, s->c.envelope, seconds, out, e);
}
static void discard_media(void *p) {
  BkSpecialSession *s = p;
  if (s->sound) {
    char ignored[256]; bk_special_audio_stop(s->sound, ignored);
  }
  bk_special_audio_destroy(s->sound); s->sound = NULL;
  bk_special_render_destroy(s->render); s->render = NULL;
}
static int media(void *p, BkSpecialWorld *world, char e[256]) {
  BkSpecialSession *s = p;
  s->render = bk_special_render_create_clock(s->renderer, s->store, world, movie_clock, s, e);
  if (!s->render) return 0;
  /*50d4fa writes wanted1 before decoding/starting its music.*/
  s->c.process->music_wanted = 1;
  s->sound = bk_special_audio_create(s->store, s->audio, s->c.first_voice,
      (unsigned)*s->c.group, s->c.effect_volume, s->c.process->effect_loops, e);
  if (!s->sound) return 0;
  if (bk_special_audio_initial_effect((unsigned)*s->c.group))
    memset(s->c.process->effect_positions[0], 0, sizeof(float) * 4);
  return 1;
}
BkSpecialSession *bk_special_session_create(BkRenderer *r, BkResourceStore *store,
    BkAudio *a, const BkSpecialSessionConfig *c, char e[256]) {
  if (!r || !store || !a || !c || !c->process || !c->common || !c->curtain ||
      !c->camera || !c->transitions || !c->envelope || !c->group ||
      *c->group < 0 || *c->group >= 5 || !c->camera_clip || !c->camera_mode ||
      !c->photo_count || !c->photos || !c->random || !c->album_group ||
      !c->latches || c->latch_count <= 120 || !c->visibility || !c->hover ||
      !c->paused || !c->previous_flow || !c->screenshot || !c->clock ||
      !c->inventory || !c->release_speech || !c->schedule ||
      !c->viewport.width || !c->viewport.height ||
      c->first_voice > BK_AUDIO_VOICES - 5 || c->speech_voice >= BK_AUDIO_VOICES ||
      (c->speech_voice >= c->first_voice && c->speech_voice < c->first_voice + 5) ||
      !isfinite(c->loading_seconds) || c->loading_seconds < 0) {
    fail(e, "incomplete real loader services"); return NULL;
  }
  const unsigned slots[] = {0, 2, 3, 5, 7};
  for (unsigned i = 0; i < sizeof(slots)/sizeof(*slots); ++i)
    if (!c->sounds[slots[i]]) { fail(e, "missing system audio slot"); return NULL; }
  BkSpecialSession *s = calloc(1, sizeof(*s));
  if (!s) { fail(e, "allocation failed"); return NULL; }
  s->c = *c; s->renderer = r; s->store = store; s->audio = a; s->packed = 1;
  s->capture = (BkSpecialCaptureService){c->screenshot, c->viewport, c->group,
                                         c->album_group, c->photo_count, c->photos};
  s->ui_ops = (BkSpecialUiOps){s, image, capture, position, motion, key, sound,
                               ui_clock, warp, schedule};
  c->process->event.sequence = 0;
  if (!c->inventory(c->context, c->photos, e)) goto bad;
  *c->photo_count = c->photos[*c->group];
  s->ui = bk_special_ui_render_create(r, store, c->curtain, e);
  if (!s->ui || !bk_special_ui_initialize(&c->process->ui, &c->process->control,
      &c->process->event, c->viewport.width, s->special, &s->ui_ops, e)) goto bad;
  uint32_t clocks[4];
  for (unsigned i = 0; i < 4; ++i) if (!clock_read(s, 0, clocks + i, e)) goto bad;
  BkSpecialWorldLoad load = {s, media, discard_media, c->visibility};
  s->world = bk_special_world_load(store, (unsigned)*c->group, c->loading_seconds,
      c->camera, c->transitions, clocks, c->random, c->latches, c->latch_count, &load, e);
  if (!s->world || !warp(s, (float)(1184.0 * (c->viewport.width / 1280.f)),
                          (float)(42.0 * (c->viewport.width / 1280.f)), e)) goto bad;
  /*Original71b358 is camera.position XYZ immediately followed by yaw. Use
   *the complete struct's representation, not an invented world translation.*/
  _Static_assert(offsetof(BkMenuCamera, yaw) ==
      offsetof(BkMenuCamera, pose) + offsetof(BkCameraFollowPose, position) + 12,
      "native listener requires contiguous XYZ/yaw");
  const float *listener = (const float *)((const char *)c->camera +
      offsetof(BkMenuCamera, pose) + offsetof(BkCameraFollowPose, position));
  s->bindings = (BkSpecialEventBindings){&c->process->event, c->group,
      &c->process->phase, c->camera_clip, c->camera_mode, &s->seconds,
      c->paused, &c->process->music_wanted, &s->packed, c->visibility,
      c->process->effect_loops, &s->c.effect_volume, listener,
      {c->process->effect_positions[0], c->process->effect_positions[1],
       c->process->effect_positions[2], c->process->effect_positions[3]}, c->envelope};
  s->ui_bindings = (BkSpecialUiBindings){c->common, &c->process->control,
      c->camera, bk_special_world_presets(s->world), &c->process->phase,
      c->camera_clip, c->camera_mode, c->photo_count, &s->special,
      c->previous_flow, c->visibility, c->hover};
  return s;
bad:
  discard_media(s);
  bk_special_world_destroy(s->world);
  bk_special_ui_render_destroy(s->ui);
  free(s); return NULL;
}
int bk_special_session_stop(BkSpecialSession *s, char e[256]) {
  if (!s) return fail(e, "missing stop owner");
  if (s->stopped) return 1;
  s->c.process->event.sequence = 0;
  /*4b8a01 releases only track handles; camera scalars remain. Resource
   *detachment/registry/movie/body destruction wait for GPU retirement.*/
  if (!s->c.release_speech(s->c.context, e) || !bk_special_audio_stop(s->sound, e)) return 0;
  for (int i = 16; i >= 0; --i) {
    if (!bk_special_ui_render_image(s->ui, (unsigned)i, NULL, e)) return 0;
    s->c.process->ui.loaded &= ~(UINT32_C(1) << i);
  }
  for (int i = 29; i >= 17; --i) {
    if (!bk_special_ui_render_image(s->ui, (unsigned)i, NULL, e)) return 0;
    s->c.process->ui.loaded &= ~(UINT32_C(1) << i);
  }
  s->stopped = 1; return 1;
}
void bk_special_session_destroy(BkSpecialSession *s) {
  if (!s) return;
  if (!s->stopped) { char ignored[256]; bk_special_session_stop(s, ignored); }
  bk_special_render_destroy(s->render);
  bk_special_audio_destroy(s->sound);
  bk_special_world_destroy(s->world);
  bk_special_ui_render_destroy(s->ui);
  free(s);
}
int bk_special_session_step(BkSpecialSession *s, float seconds,
    const BkInput *in, char e[256]) {
  if (!s || s->stopped || s->pending || !in || !isfinite(seconds) || seconds <= 0 || seconds > 1 ||
      !isfinite(in->look_x) || !isfinite(in->look_y)) return fail(e, "invalid step/lifecycle");
  if (!bk_special_ui_render_begin(s->ui, e) ||
      !bk_virtual_pointer_step(&s->pointer, &s->c.viewport, in, seconds, e)) return 0;
  s->seconds = seconds; s->input = *in;
  float camera_motion[] = {-in->look_x, -in->look_y};
  unsigned buttons = ((in->held & BK_BUTTON_CAMERA_ORBIT) ? 1u : 0u) |
                     ((in->held & BK_BUTTON_CAMERA_ADJUST) ? 2u : 0u);
  BkSpecialWorldServices services = {s, world_key, present, audio_call, movie, clock_read, level};
  if (!bk_special_world_step(s->world, &s->bindings, camera_motion, buttons, &services, e) ||
      !bk_special_render_prepare(s->render, s->c.camera, &s->c.fog, e)) return 0;
  s->pending = 1; s->drawn = s->ui_ready = 0; return 1;
}
int bk_special_session_draw(BkSpecialSession *s, char e[256]) {
  if (!s || (!s->pending && !s->drawn)) return fail(e, "no prepared world");
  if (!bk_special_render_draw(s->render, e)) return 0;
  if (!s->ui_ready) {
    BkSpecialUiFrame frame;
    if (!bk_special_ui_step(&s->c.process->ui, &s->ui_bindings, &s->ui_ops,
        s->c.viewport.width / 1280.f, s->seconds, &frame, e) ||
        !bk_special_ui_render_prepare(s->ui, &frame, s->c.viewport.width,
                                        s->c.viewport.height, e)) return 0;
    s->ui_ready = 1;
  }
  if (!bk_special_ui_render_draw(s->ui, e)) return 0;
  s->drawn = 1; return 1;
}
int bk_special_session_after_present(BkSpecialSession *s, char e[256]) {
  if (!s || !s->drawn) return fail(e, "no presented snapshot");
  if (!s->pending) return 1;
  s->pending = 0;
  if (s->transition) {
    if (!s->c.schedule(s->c.context, s->target, s->mode, e)) return 0;
    s->transition = 0;
  }
  return 1;
}
BkSpecialWorld *bk_special_session_world(BkSpecialSession *s) { return s ? s->world : NULL; }
const BkVirtualPointer *bk_special_session_pointer(const BkSpecialSession *s) {
  return s ? &s->pointer : NULL;
}
