#include "scene/volume_menu_audio.h"
#include <stdlib.h>
struct BkVolumeMenuAudio {
  BkAudio *audio;
  BkAudioClip *clips[6];
  unsigned first;
  unsigned loaded;
};
static int index_of(unsigned slot) {
  static const unsigned slots[] = {0,3,6,9,10,11};
  for (unsigned i=0;i<6;++i) if (slot==slots[i]) return (int)i;
  return -1;
}
static int fail(char e[256], const char *why) {
  snprintf(e,256,"volume audio: %s",why);return 0;
}
BkVolumeMenuAudio *bk_volume_menu_audio_create(BkResourceStore *store, BkAudio *audio,
                                               unsigned first, char e[256]) {
  if (!store || !audio || first>BK_AUDIO_VOICES-6) {
    fail(e,"invalid services/voice range");return NULL;
  }
  for (unsigned i=0;i<6;++i) {
    int32_t volume,pan;
    if (bk_audio_get_gain(audio,first+i,&volume,&pan)) {
      fail(e,"mixer range is already owned");return NULL;
    }
  }
  BkVolumeMenuAudio *s=calloc(1,sizeof(*s));
  if (!s) {fail(e,"allocation failed");return NULL;}
  s->audio=audio;s->first=first;
  for (unsigned slot=0;slot<16;++slot) {
    const char *name=bk_volume_menu_sound(slot);
    if (!name) continue;
    int i=index_of(slot);
    s->clips[i] = slot == 3 ? bk_audio_clip_load_music(store,"bk3_02",name,e)
                          : bk_audio_clip_load(store,"bk3_02",name,e);
    if (!s->clips[i]) {bk_volume_menu_audio_destroy(s);return NULL;}
  }
  return s;
}
void bk_volume_menu_audio_destroy(BkVolumeMenuAudio *s) {
  if (!s) return;
  for (unsigned i=0;i<6;++i) {
    char ignored[256];
    if (s->loaded&(1u<<i)) bk_audio_clear(s->audio,s->first+i,ignored);
    bk_audio_clip_release(s->clips[i]);
  }
  free(s);
}
int bk_volume_menu_audio_play(BkVolumeMenuAudio *s,unsigned slot,int32_t volume,char e[256]) {
  if (!s || slot>=16) return fail(e,"invalid sample");
  int i=index_of(slot);
  if (i<0) return 1;
  if (!bk_audio_play(s->audio,s->first+(unsigned)i,s->clips[i],0,volume,0,e)) return 0;
  s->loaded|=1u<<i;return 1;
}
int bk_volume_menu_audio_stop(BkVolumeMenuAudio *s,unsigned slot,char e[256]) {
  if (!s || slot>=16) return fail(e,"invalid sample");
  int i=index_of(slot);
  return i<0 || !(s->loaded&(1u<<i)) || bk_audio_pause(s->audio,s->first+(unsigned)i,e);
}
int bk_volume_menu_audio_gain(BkVolumeMenuAudio *s,unsigned slot,int32_t volume,char e[256]) {
  if (!s || slot>=16) return fail(e,"invalid sample");
  int i=index_of(slot);
  return i<0 || !(s->loaded&(1u<<i)) || bk_audio_gain(s->audio,s->first+(unsigned)i,volume,0,e);
}
int bk_volume_menu_audio_playing(BkVolumeMenuAudio *s,unsigned slot,int *playing,char e[256]) {
  if (!s || !playing || slot>=16) return fail(e,"invalid sample/status");
  int i=index_of(slot);
  if (i<0 || !(s->loaded&(1u<<i))) {*playing=0;return 1;}
  return bk_audio_playing(s->audio,s->first+(unsigned)i,playing) || fail(e,"missing owned voice");
}
