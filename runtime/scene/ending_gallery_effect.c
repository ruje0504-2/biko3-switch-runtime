#include "scene/ending_gallery_effect.h"
#include "core/random.h"
typedef struct {
  BkActorPose *primary;
  BkEndingAudio *audio;
  uint32_t *random;
  const BkEndingGalleryEffectBindings *bindings;
} Context;
static int active(void *p, int32_t *out, char e[256]) {
  Context *c = p;
  BkClipState state;
  if (!bk_actor_pose_state(c->primary, &state)) {
    if (e) snprintf(e, 256, "gallery effect: missing primary clip state");
    return 0;
  }
  *out = state.slot;
  return 1;
}
static int timing(void *p, unsigned slot, BkEndingClipTiming *out, char e[256]) {
  Context *c = p;
  BkClipTiming value;
  if (!bk_actor_pose_timing(c->primary, slot, &value)) {
    if (e) snprintf(e, 256, "gallery effect: missing primary slot%u", slot);
    return 0;
  }
  *out = (BkEndingClipTiming){value.start, value.end, value.source};
  return 1;
}
static int audio(void *p, const BkEndingAudioCall *call, int *playing, char e[256]) {
  Context *c = p;
  const BkEndingGalleryEffectBindings *b = c->bindings;
  return bk_ending_audio_call(c->audio, b->frame->group,
      b->auxiliary->variant, b->auxiliary->selection, call, playing, e);
}
static int random_value(void *p, int32_t *out, char e[256]) {
  (void)e;
  *out = bk_random_next(((Context *)p)->random);
  return 1;
}
int bk_ending_gallery_effect_apply(BkActorPose *primary, BkEndingAudio *audio_owner,
    uint32_t *random, const BkEndingGalleryEffectBindings *b,
    float seconds, char e[256]) {
  if (!primary || !audio_owner || !random) {
    if (e) snprintf(e, 256, "gallery effect: missing scene resources");
    return 0;
  }
  Context c = {primary, audio_owner, random, b};
  BkEndingGalleryEffectOps ops = {&c, active, timing, audio, random_value};
  return bk_ending_gallery_effect_step(b, seconds, &ops, e);
}
