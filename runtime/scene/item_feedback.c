#include "scene/item_feedback.h"
#include "scene/dialogue_assets.h"
#include <stdlib.h>
struct BkItemFeedback {
  BkAudio *audio;
  BkAudioClip *clip;
  BkDialogueAssets dialogue;
  unsigned voice;
  int32_t volume;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "item feedback: %s", message);
  return 0;
}
BkItemFeedback *bk_item_feedback_create(BkResourceStore *store, BkAudio *audio,
                                        unsigned voice, int32_t volume,
                                        char error[256]) {
  if (!store || !audio || voice >= BK_AUDIO_VOICES || volume < -10000 ||
      volume > 0 || bk_audio_stats(audio).failed) {
    fail(error, "invalid resources/mixer/voice/volume");
    return NULL;
  }
  BkItemFeedback *f = calloc(1, sizeof(*f));
  if (!f) {
    fail(error, "allocation failed");
    return NULL;
  }
  f->audio = audio;
  f->voice = voice;
  f->volume = volume;
  if (!bk_item_feedback_reload_message(f, store, error)) {
    goto bad;
  }
  f->clip = bk_audio_clip_load(store, "bk3_02", "se101.wav", error);
  if (!f->clip)
    goto bad;
  return f;
bad:
  bk_item_feedback_destroy(f);
  return NULL;
}
void bk_item_feedback_destroy(BkItemFeedback *f) {
  if (!f)
    return;
  bk_dialogue_assets_close(&f->dialogue);
  bk_audio_clip_release(f->clip);
  free(f);
}
int bk_item_feedback_stop(BkItemFeedback *f, char error[256]) {
  if (!f)
    return fail(error, "missing instance");
  return bk_audio_clear(f->audio, f->voice, error);
}
static int sound(void *context, char error[256]) {
  BkItemFeedback *f = context;
  int playing;
  if (!f || !bk_audio_playing(f->audio, f->voice, &playing))
    return fail(error, "unavailable playback status");
  /*46435e queries status but does not gate the rewind on its playing bit. */
  return bk_audio_play(f->audio, f->voice, f->clip, 0, f->volume, 0, error);
}
static int notice(void *context, uint32_t id, char error[256]) {
  BkItemFeedback *f = context;
  if (!f || id > INT32_MAX)
    return fail(error, "missing instance/invalid message ID");
  return bk_message_lookup(f->dialogue.raw.data, f->dialogue.raw.size,
                           (int32_t)id, &f->dialogue.state.text, error);
}
BkItemPickupOps bk_item_feedback_ops(BkItemFeedback *f) {
  return f ? (BkItemPickupOps){f, sound, notice} : (BkItemPickupOps){0};
}
const BkMessage *bk_item_feedback_message(const BkItemFeedback *f) {
  return f ? &f->dialogue.state.text : NULL;
}
int bk_item_feedback_reload_message(BkItemFeedback *f, BkResourceStore *store,
                                    char error[256]) {
  if (!f)
    return fail(error, "missing instance");
  return bk_dialogue_assets_load(&f->dialogue, store, "i00_00.txt", error);
}
const BkDialogue *bk_item_feedback_dialogue(const BkItemFeedback *f) {
  return f ? &f->dialogue.state : NULL;
}

void bk_item_feedback_close_message(BkItemFeedback *f) {
  if (f)
    bk_dialogue_assets_close(&f->dialogue);
}

int bk_item_feedback_release_sound(BkItemFeedback *f, char error[256]) {
  if (!f || !bk_audio_clear(f->audio, f->voice, error))
    return 0;
  bk_audio_clip_release(f->clip);
  f->clip = NULL;
  return 1;
}
int bk_item_feedback_reload_sound(BkItemFeedback *f, BkResourceStore *store,
                                  char error[256]) {
  if (!f || !store)
    return fail(error, "missing sound reload services");
  BkAudioClip *clip = bk_audio_clip_load(store, "bk3_02", "se101.wav", error);
  if (!clip)
    return 0;
  if (!bk_audio_clear(f->audio, f->voice, error)) {
    bk_audio_clip_release(clip);
    return 0;
  }
  bk_audio_clip_release(f->clip);
  f->clip = clip;
  return 1;
}
