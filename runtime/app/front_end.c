#include "app/front_end.h"
#include "save/capture_file.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkFrontEnd {
  BkFrontEndConfig c;
  BkTitleMenuState title;
  BkSelectionUi selection;
  BkDialogueSessionState dialogue;
  BkMenuCamera camera;
  BkMenuCamera *camera_owner;
  BkVoiceEnvelope envelope;
  BkVoiceEnvelope *envelope_owner;
  BkFadeSprite backdrop_curtain;
  BkDialogueResult result;
  BkGalleryMenu gallery;
  BkGalleryMenuSelection gallery_result;
  BkVirtualPointer gallery_pointer;
  int gallery_result_valid, music_playing;
  uint8_t backdrop_wanted;
  BkUnlockTable unlocked;
  int32_t group, area;
  float pointer[2], motion[2];
  BkSystemAudio *sounds[8];
  BkAudioClip *title_music;
  BkAudioClip *gallery_music;
  BkTitleMenuRender *title_render;
  BkGalleryMenuRender *gallery_render;
  BkSelectionSession *selection_session;
  BkDialogueSession *dialogue_session;
  BkSpecialSession *special_session;
  uint8_t active;
  int released;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "front end: %s", why);
  return 0;
}
static int position(void *p, float out[2], char e[256]) {
  (void)e;
  memcpy(out, ((BkFrontEnd *)p)->pointer, 8);
  return 1;
}
static int motion(void *p, float out[2], char e[256]) {
  (void)e;
  memcpy(out, ((BkFrontEnd *)p)->motion, 8);
  return 1;
}
static int warp(void *p, float x, float y, char e[256]) {
  (void)e;
  BkFrontEnd *s = p;
  s->pointer[0] = x;
  s->pointer[1] = y;
  return 1;
}
static int sound(void *p, unsigned slot, char e[256]) {
  BkFrontEnd *s = p;
  return slot < 8 && s->sounds[slot]
             ? bk_system_audio_restart(s->sounds[slot], e)
             : fail(e, "missing system sound");
}
static int gain(void *p, int32_t volume, char e[256]) {
  return bk_audio_gain(((BkFrontEnd *)p)->c.services.audio, 60, volume, 0, e);
}
static int release(void *p, uint8_t flow, char e[256]) {
  return bk_front_end_stop(p, flow, e);
}
static int schedule(void *p, uint8_t target, uint8_t mode, char e[256]) {
  BkFrontEnd *s = p;
  return s->c.schedule(s->c.context, target, mode, e);
}
static int special_clock(void *p, int timer, uint32_t *out, char e[256]) {
  BkFrontEnd *s = p;
  return s->c.special.clock
      ? s->c.special.clock(s->c.special.context, timer, out, e)
      : fail(e, "special clock service is unavailable");
}
static int special_inventory(void *p, int32_t out[5], char e[256]) {
  BkFrontEnd *s = p;
  return bk_capture_files_count_photos(s->c.services.capture_files, out, e);
}
static int special_release_speech(void *p, char e[256]) {
  BkFrontEnd *s = p;
  return s->c.special.release_speech
      ? s->c.special.release_speech(s->c.special.context, e)
      : fail(e, "shared speech owner is unavailable");
}
static int gallery_image(void *p, unsigned slot, const char *name, char e[256]) {
  return bk_gallery_menu_render_image(((BkFrontEnd *)p)->gallery_render,
                                       slot, name, e);
}
static int gallery_story(void *p, unsigned group, char e[256]) {
  (void)e;
  BkFrontEnd *s = p;
  /*4F75DD only writes the destination group and area. Flow48 has its own
   * loader; it is not an alias for the existing dialogue/game entries. */
  *s->c.group = group;
  *s->c.area = 0;
  return 1;
}
static int unlock(void *p, unsigned group, char e[256]) {
  BkFrontEnd *s = p;
  BkUnlockTable next;
  if (!s || !s->c.unlock_file || !s->c.ending_flags ||
      !s->c.ending_flags_valid || !*s->c.ending_flags_valid ||
      !s->c.ending_flags_group || group != *s->c.ending_flags_group)
    return fail(e, "ending unlock working row is not available");
  if (!bk_unlock_file_store(s->c.unlock_file, group, s->c.ending_flags,
                            &next, e))
    return 0;
  s->unlocked = next;
  return 1;
}
BkFrontEnd *bk_front_end_create(const BkFrontEndConfig *c, char e[256]) {
  if (!c || !c->services.renderer || !c->services.resources ||
      !c->services.audio || !c->common || !c->curtain || !c->flow ||
      !c->cursor || !c->hover || !c->group || !c->area || !c->random ||
      !c->photos || !c->photo_count || !c->schedule) {
    fail(e, "missing retained services");
    return NULL;
  }
  BkFrontEnd *s = calloc(1, sizeof(*s));
  if (!s) {
    fail(e, "allocation failed");
    return NULL;
  }
  s->c = *c;
  s->camera_owner = c->camera ? c->camera : &s->camera;
  s->envelope_owner = c->envelope ? c->envelope : &s->envelope;
  if (c->unlock_file &&
      bk_unlock_file_read(c->unlock_file, &s->unlocked, e) == BK_RESOURCE_ERROR) {
    bk_front_end_destroy(s);
    return NULL;
  }
  if (!c->camera)
    for (unsigned i = 0; i < 16; i++)
      s->camera.pose.world[i] = s->camera.matrix[i] = i % 5 == 0;
  bk_fade_sprite_initialize(&s->backdrop_curtain);
  for (unsigned i = 0; i < 8; i++) {
    if (i == 6) continue;
    s->sounds[i] = bk_system_audio_create_slot(
        c->services.resources, c->services.audio, 48 + i, i, -600, e);
    if (!s->sounds[i]) {
      bk_front_end_destroy(s);
      return NULL;
    }
  }
  return s;
}
int bk_front_end_stop(BkFrontEnd *s, uint8_t flow, char e[256]) {
  if (!s || s->active != flow)
    return fail(e, "release of unowned flow");
  if (s->released)
    return 1;
  if (flow == 1 || flow == 0x18) {
    if (!bk_audio_clear(s->c.services.audio, 60, e))
      return 0;
    s->music_playing = 0;
  }
  if (flow == 0x18)
    s->gallery.loaded = 0;
  if (flow == 8 && !bk_dialogue_session_stop(s->dialogue_session, e))
    return 0;
  if (flow == 0x48 && !bk_special_session_stop(s->special_session, e))
    return 0;
  if (flow == 0x38 && !bk_selection_session_released(s->selection_session))
    return fail(e, "selection release must finish its native control step");
  s->released = 1;
  return 1;
}
void bk_front_end_collect(BkFrontEnd *s) {
  if (!s || !s->released)
    return;
  bk_title_menu_render_destroy(s->title_render);
  s->title_render = NULL;
  bk_audio_clip_release(s->title_music);
  s->title_music = NULL;
  bk_gallery_menu_render_destroy(s->gallery_render);
  s->gallery_render = NULL;
  bk_audio_clip_release(s->gallery_music);
  s->gallery_music = NULL;
  bk_selection_session_destroy(s->selection_session);
  s->selection_session = NULL;
  bk_dialogue_session_destroy(s->dialogue_session);
  s->dialogue_session = NULL;
  bk_special_session_destroy(s->special_session);
  s->special_session = NULL;
  s->active = 0;
  s->released = 0;
}
void bk_front_end_destroy(BkFrontEnd *s) {
  if (!s)
    return;
  if (s->music_playing) {
    char ignored[256];
    bk_audio_clear(s->c.services.audio, 60, ignored);
  }
  s->released = 1;
  bk_front_end_collect(s);
  for (unsigned i = 0; i < 8; i++)
    bk_system_audio_destroy(s->sounds[i]);
  free(s);
}
int bk_front_end_active(const BkFrontEnd *s) { return s ? s->active : 0; }
int bk_front_end_unlocks(const BkFrontEnd *s, BkUnlockTable *out, char e[256]) {
  if (!s || !out)
    return fail(e, "missing saved unlock table owner");
  *out = s->unlocked;
  return 1;
}
int bk_front_end_result(const BkFrontEnd *s, BkDialogueResult *out,
                        char e[256]) {
  if (!s || !out)
    return fail(e, "missing dialogue result owner");
  *out = s->result;
  return 1;
}
int bk_front_end_gallery_result(const BkFrontEnd *s, BkGalleryMenuSelection *out,
                                char e[256]) {
  if (!s || !out || !s->gallery_result_valid)
    return fail(e, "no dispatched gallery selection");
  *out = s->gallery_result;
  return 1;
}
int bk_front_end_load(BkFrontEnd *s, uint8_t flow, uint8_t previous,
                      uint8_t response, double wall, float seconds,
                      char e[256]) {
  if (!s || s->active || !isfinite(wall) || wall < 1 || wall > 1e12 ||
      !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid load/previous owner still active");
  s->group = (int32_t)*s->c.group;
  s->area = (int32_t)*s->c.area;
  uint32_t now = (uint32_t)(uint64_t)(wall * 1000);
  if (flow == 1) {
    s->title_music =
        bk_audio_clip_load(s->c.services.resources, "bk3_02", "bg001.wav", e);
    if (!s->title_music ||
        !bk_audio_play(s->c.services.audio, 60, s->title_music, 1, -900, 0, e))
      goto bad;
    s->music_playing = 1;
    s->title_render = bk_title_menu_render_create(
        s->c.services.renderer, s->c.services.resources, 0, e);
    BkTitleMenuOps ops = {s, sound, gain, warp, position, motion, release};
    if (!s->title_render ||
        !bk_title_menu_initialize(&s->title, s->c.viewport.width, 0, -900, &ops,
                                  e))
      goto bad;
  } else if (flow == 0x18) {
    if (previous == 0x10 &&
        (!s->c.ending_flags_valid || !*s->c.ending_flags_valid ||
         !s->c.ending_flags_group || *s->c.ending_flags_group >= 5))
      return fail(e, "gallery return requires the released ending group");
    unsigned ending_group = previous == 0x10 ? *s->c.ending_flags_group : 0;
    s->gallery_result_valid = 0;
    s->gallery_render = bk_gallery_menu_render_create(
        s->c.services.renderer, s->c.services.resources, e);
    BkGalleryMenuOps ops = {s, sound, gallery_image, position};
    if (!s->gallery_render ||
        !bk_gallery_menu_initialize(&s->gallery, s->c.viewport.width, previous,
                                    ending_group, *s->c.group, s->unlocked.flags,
                                    -900, &ops, e))
      goto bad;
    s->gallery_music =
        bk_audio_clip_load(s->c.services.resources, "bk3_02", "bg002.wav", e);
    if (!s->gallery_music ||
        !bk_audio_play(s->c.services.audio, 60, s->gallery_music, 1, -900, 0, e))
      goto bad;
    s->music_playing = 1;
  } else if (flow == 0x48) {
    if (!s->c.services.capture_files || !s->c.special.screenshot)
      return fail(e, "special entry requires writable capture storage");
    BkSpecialSessionConfig c = s->c.special;
    c.common = s->c.common; c.curtain = s->c.curtain;
    c.camera = s->camera_owner; c.envelope = s->envelope_owner;
    c.group = &s->group; c.photos = s->c.photos; c.photo_count = s->c.photo_count;
    c.random = s->c.random; c.hover = s->c.hover;
    c.previous_flow = &s->c.flow->previous;
    c.viewport = s->c.viewport; c.loading_seconds = seconds;
    c.context = s; c.clock = special_clock; c.inventory = special_inventory;
    c.release_speech = special_release_speech; c.schedule = schedule;
    c.first_voice = 0; c.speech_voice = 61;
    memcpy(c.sounds, s->sounds, sizeof(c.sounds));
    s->special_session = bk_special_session_create(s->c.services.renderer,
        s->c.services.resources, s->c.services.audio, &c, e);
    if (!s->special_session) goto bad;
  } else if (flow == 0x38) {
    BkSelectionSessionConfig c = {
        .ui = &s->selection,
        .bindings = {s->c.common, s->c.flow, s->c.cursor, s->c.hover,
                     s->c.photos, &s->group, &s->area, s->c.photo_count},
        .camera = s->camera_owner,
        .envelope = s->envelope_owner,
        .random = s->c.random,
        .unlocked = s->unlocked.flags,
        .pointer = {s, position, motion, warp},
        .music_voice = 60,
        .speech_voice = 61,
        .width = s->c.viewport.width,
        .height = s->c.viewport.height,
        .music_volume = -900,
        .voice_volume = -700,
        .loading_seconds = seconds,
        .clocks = {now, now, now, now},
        .movie_clock_ms = (int32_t)now};
    memcpy(c.sounds, s->sounds, sizeof(c.sounds));
    s->selection_session = bk_selection_session_create(
        s->c.services.renderer, s->c.services.resources, s->c.services.audio,
        &c, e);
    if (!s->selection_session)
      goto bad;
  } else if (flow == 8) {
    BkDialogueSessionConfig c = {.state = &s->dialogue,
                                 .camera = s->camera_owner,
                                 .common = s->c.common,
                                 .common_render = s->c.curtain,
                                 .backdrop_curtain = &s->backdrop_curtain,
                                 .backdrop_curtain_wanted = &s->backdrop_wanted,
                                 .result = &s->result,
                                 .envelope = s->envelope_owner,
                                 .random = s->c.random,
                                 .context = s,
                                 .unlock = unlock,
                                 .schedule = schedule,
                                 .group = *s->c.group,
                                 .area = (int32_t)*s->c.area,
                                 .previous = previous,
                                 .response = response,
                                 .width = s->c.viewport.width,
                                 .height = s->c.viewport.height,
                                 .music_voice = 60,
                                 .speech_voice = 61,
                                 .clocks = {now, now, now, now}};
    s->dialogue_session = bk_dialogue_session_create(
        s->c.services.renderer, s->c.services.resources, s->c.services.audio,
        &c, e);
    if (!s->dialogue_session)
      goto bad;
  } else
    return fail(e, "unknown original-menu flow");
  s->active = flow;
  return 1;
bad:
  if (s->music_playing) {
    char ignored[256];
    bk_audio_clear(s->c.services.audio, 60, ignored);
    s->music_playing = 0;
  }
  s->released = 1;
  bk_front_end_collect(s);
  return 0;
}
static uint32_t buttons(const BkInput *in) {
  return ((in->pressed & BK_BUTTON_CONFIRM) ? BK_PAUSE_CONFIRM : 0) |
         ((in->pressed & BK_BUTTON_UP) ? BK_PAUSE_UP : 0) |
         ((in->pressed & BK_BUTTON_DOWN) ? BK_PAUSE_DOWN : 0) |
         ((in->pressed & BK_BUTTON_LEFT) ? BK_PAUSE_LEFT : 0) |
         ((in->pressed & BK_BUTTON_RIGHT) ? BK_PAUSE_RIGHT : 0);
}
int bk_front_end_step(BkFrontEnd *s, double seconds, double wall,
                      const BkInput *in, char e[256]) {
  if (!s || !in || s->released || !s->active || !isfinite(seconds) ||
      seconds <= 0 || seconds > 1 || !isfinite(wall) || wall < 1 || wall > 1e12)
    return fail(e, "invalid frame input");
  if (s->active == 0x18) {
    memcpy(s->gallery_pointer.position, s->pointer, sizeof(s->pointer));
    if (!bk_virtual_pointer_step(&s->gallery_pointer, &s->c.viewport, in,
                                  seconds, e))
      return 0;
    memcpy(s->pointer, s->gallery_pointer.position, sizeof(s->pointer));
    /*The gallery polls position only. Wake the shared cursor when the
     *Switch stick/d-pad or touch moves it after a title idle timeout.*/
    if (s->gallery_pointer.motion[0] != 0 || s->gallery_pointer.motion[1] != 0 ||
        in->pointer_active) {
      s->c.cursor->wanted = 1;
      s->c.cursor->idle.armed = 0;
    }
  } else if (in->pointer_active) {
    s->pointer[0] = in->pointer_x - s->c.viewport.x;
    s->pointer[1] = in->pointer_y - s->c.viewport.y;
  }
  s->motion[0] = in->pointer_motion_x;
  s->motion[1] = in->pointer_motion_y;
  uint32_t now = (uint32_t)(uint64_t)(wall * 1000);
  float dt = (float)seconds, scale = s->c.viewport.width / 1280.f;
  if (s->active == 1) {
    BkTitleMenuBindings b = {s->c.common, s->c.flow, s->c.cursor, s->c.hover};
    BkTitleMenuOps ops = {s, sound, gain, warp, position, motion, release};
    BkTitleMenuInput input = {buttons(in), now, dt, scale, -900, 0};
    BkTitleMenuFrame f;
    return bk_title_menu_step(&s->title, &b, &input, &ops, &f, e) &&
           bk_title_menu_render_prepare(s->title_render, &f,
                                        s->c.viewport.width,
                                        s->c.viewport.height, e);
  }
  if (s->active == 0x18) {
    BkGalleryMenuBindings b = {s->c.common, s->c.cursor};
    BkGalleryMenuOps ops = {s, sound, gallery_image, position};
    BkGalleryMenuInput input = {
        buttons(in) | ((in->pressed & BK_BUTTON_BACK) ? BK_GALLERY_BACK : 0),
        dt, scale, s->c.viewport.height};
    BkGalleryMenuFrame frame;
    BkGalleryDispatchOps dispatch = {s, release, gallery_story};
    bk_gallery_menu_render_begin(s->gallery_render);
    if (!bk_gallery_menu_step(&s->gallery, &b, &input, &ops, &frame, e) ||
        !bk_gallery_menu_render_prepare(s->gallery_render, &frame,
                                        s->c.viewport.width,
                                        s->c.viewport.height, e) ||
        !bk_gallery_menu_dispatch(&s->gallery, frame.action, &s->gallery_result,
                                  s->c.flow, &dispatch, e))
      return 0;
    if (frame.action >= 0 && frame.action <= 7)
      s->gallery_result_valid = 1;
    return 1;
  }
  if (s->active == 0x38) {
    BkSelectionSessionInput input = {
        .ui = {buttons(in), now, dt, scale, -900, -700, 0, 1},
        .camera_buttons = ((in->held & BK_BUTTON_CONFIRM) ? 1u : 0) |
                          ((in->held & BK_BUTTON_BACK) ? 2u : 0),
        .face_clocks = {now, now, now},
        .reload_clocks = {now, now, now, now},
        .movie_clock_ms = (int32_t)now,
        .movie_restart_clock_ms = (int32_t)now,
        .reload_movie_clock_ms = (int32_t)now};
    if (!bk_selection_session_step(s->selection_session, &input, e))
      return 0;
    *s->c.group = (uint32_t)s->group;
    *s->c.area = (uint32_t)s->area;
    s->released = bk_selection_session_released(s->selection_session);
    return 1;
  }
  if (s->active == 8) {
    BkDialogueSessionInput input = {.seconds = dt,
                                    .timestamp_ms = now,
                                    .timer_clock_ms = now,
                                    .face_clocks = {now, now, now},
                                    .advance =
                                        !!(in->pressed & BK_BUTTON_CONFIRM),
                                    .music_volume = -900,
                                    .voice_volume = -700};
    return bk_dialogue_session_step(s->dialogue_session, &input, e);
  }
  if (s->active == 0x48)
    return bk_special_session_step(s->special_session, dt, in, e);
  return fail(e, "unbound frame");
}
int bk_front_end_draw(BkFrontEnd *s, char e[256]) {
  if (!s || !s->active)
    return fail(e, "no snapshot");
  if (!bk_renderer_viewport(s->c.services.renderer, &s->c.viewport, e))
    return 0;
  int ok = s->active == 1 ? bk_title_menu_render_draw(s->title_render, e)
           : s->active == 0x18
               ? bk_gallery_menu_render_draw(s->gallery_render, e)
           : s->active == 0x38
               ? bk_selection_session_draw(s->selection_session, e)
           : s->active == 8 ? bk_dialogue_session_draw(s->dialogue_session, e)
           : s->active == 0x48 ? bk_special_session_draw(s->special_session, e)
                            : 0;
  return ok && bk_renderer_viewport(s->c.services.renderer, NULL, e);
}
int bk_front_end_after_present(BkFrontEnd *s, char e[256]) {
  if (!s || !s->active)
    return fail(e, "no submitted frame");
  if (s->active == 0x38)
    return bk_selection_session_after_present(s->selection_session, e);
  if (s->active == 8)
    return bk_dialogue_session_after_present(s->dialogue_session, e);
  if (s->active == 0x48) {
    const BkVirtualPointer *p = bk_special_session_pointer(s->special_session);
    memcpy(s->pointer, p->position, sizeof(s->pointer));
    return bk_special_session_after_present(s->special_session, e);
  }
  return 1;
}
