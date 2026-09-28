#include "scene/npc_event_audio.h"
#include <stdlib.h>
struct BkNpcEventAudio {
  BkAudio *audio;
  BkAreaAudio *area;
  BkSystemAudio *system;
  BkAudioClip *clips[2];
  unsigned voices[2];
  int32_t volume;
};
static int fail(char *error, const char *why) {
  snprintf(error, 256, "NPC event audio: %s", why);
  return 0;
}
BkNpcEventAudio *
bk_npc_event_audio_create(BkResourceStore *store, BkAudio *audio, unsigned v1,
                          unsigned v2, int32_t volume, BkAreaAudio *area,
                          BkSystemAudio *system, char error[256]) {
  if (!store || !audio || !area || !system || v1 >= BK_AUDIO_VOICES ||
      v2 >= BK_AUDIO_VOICES || v1 == v2 || volume < -10000 || volume > 0 ||
      bk_audio_stats(audio).failed) {
    fail(error, "invalid services/voice slots/volume");
    return NULL;
  }
  BkNpcEventAudio *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->audio = audio;
  a->area = area;
  a->system = system;
  a->voices[0] = v1;
  a->voices[1] = v2;
  a->volume = volume;
  const char *names[] = {"se114.wav", "se115.wav"};
  for (unsigned i = 0; i < 2; ++i)
    if (!(a->clips[i] = bk_audio_clip_load(store, "bk3_02", names[i], error))) {
      bk_npc_event_audio_destroy(a);
      return NULL;
    }
  return a;
}
void bk_npc_event_audio_destroy(BkNpcEventAudio *a) {
  if (!a)
    return;
  for (unsigned i = 0; i < 2; ++i)
    bk_audio_clip_release(a->clips[i]);
  free(a);
}
int bk_npc_event_audio_stop(BkNpcEventAudio *a, char error[256]) {
  if (!a)
    return fail(error, "missing instance");
  return bk_audio_clear(a->audio, a->voices[0], error) &&
         bk_audio_clear(a->audio, a->voices[1], error);
}
int bk_npc_event_audio_apply(BkNpcEventAudio *a, const BkNpcSpatialEffects *e,
                             int32_t group, int32_t area, const float player[3],
                             float yaw, int32_t volume, char error[256]) {
  if (!a || !e || e->ai.sound > 2 || e->route.point.play_wait_sound > 1 ||
      e->route.point.play_route_sound > 1 ||
      (e->route.point.play_wait_sound && e->route.point.play_route_sound))
    return fail(error, "invalid events");
  BkNpcRouteSound route = {0};
  if (e->route.point.play_route_sound &&
      !bk_npc_route_sound(&route, group, area, e->route.sound_cursor,
                          e->route.sound_position, player, yaw, volume))
    return fail(error, "invalid route sound geometry");
  if (e->ai.sound) {
    unsigned i = e->ai.sound - 1;
    if (!bk_audio_play(a->audio, a->voices[i], a->clips[i], 0, a->volume, 0,
                       error))
      return 0;
  }
  if (e->route.point.play_wait_sound && !bk_system_audio_wait(a->system, error))
    return 0;
  return bk_area_audio_route(a->area, &route, error);
}
