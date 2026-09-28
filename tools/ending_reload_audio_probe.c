/* Native-generated release/wait commands with real PCM and delayed
 * consumption. Phase0 is an explicit no-resource fixture, not full flow16. */
#include "game/ending_reload.h"
#include "scene/ending_audio.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
enum { FIRST = 8, N = BK_ENDING_SOUND_BUFFERS };
typedef struct {
  uint64_t submitted, consumed, samples, nonzero, hash;
  int16_t batch[8192];
  size_t count;
} Sink;
static int submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e;
  Sink *s = p;
  assert(s->count + frames * 2 <= 8192);
  memcpy(s->batch + s->count, pcm, frames * 2 * sizeof(*pcm));
  s->count += frames * 2;
  for (size_t i = 0; i < frames * 2; i++) {
    uint16_t v = (uint16_t)pcm[i];
    s->nonzero += v != 0;
    s->hash = (s->hash ^ (v & 255)) * UINT64_C(1099511628211);
    s->hash = (s->hash ^ (v >> 8)) * UINT64_C(1099511628211);
  }
  s->samples += frames * 2;
  s->submitted += frames;
  return 1;
}
static int poll(void *p, uint64_t *out, char e[256]) {
  (void)e;
  *out = ((Sink *)p)->consumed;
  return 1;
}
static int advance(BkAudio *mix[2], Sink sink[2], unsigned step, char e[256]) {
  for (unsigned i = 0; i < 2; i++) {
    uint64_t n = (unsigned[]){0, 240, 480, 960, 1920}[step % 5];
    uint64_t queued = sink[i].submitted - sink[i].consumed;
    sink[i].consumed += n < queued ? n : queued;
    sink[i].count = 0;
    if (!bk_audio_poll(mix[i], e))
      return 0;
  }
  return 1;
}
static int compare(BkAudio *mix[2], Sink sink[2], char e[256]) {
  for (unsigned slot = 0; slot < BK_AUDIO_VOICES; slot++) {
    int a, b;
    assert(bk_audio_playing(mix[0], slot, &a) &&
           bk_audio_playing(mix[1], slot, &b) && a == b);
    BkAudioCursor ca, cb;
    assert(bk_audio_cursor(mix[0], slot, &ca) &&
           bk_audio_cursor(mix[1], slot, &cb));
    assert(ca.source_frame == cb.source_frame && ca.playing == cb.playing &&
           ca.buffered == cb.buffered &&
           ca.pending_change == cb.pending_change);
    assert((ca.pcm != NULL) == (cb.pcm != NULL));
    if (ca.pcm)
      assert(bk_pcm_frames(ca.pcm) == bk_pcm_frames(cb.pcm));
  }
  for (unsigned i = 0; i < 2; i++)
    if (!bk_audio_fill(mix[i], e))
      return 0;
  assert(
      sink[0].count == sink[1].count &&
      !memcmp(sink[0].batch, sink[1].batch, sink[0].count * sizeof(int16_t)));
  assert(sink[0].submitted == sink[1].submitted &&
         sink[0].hash == sink[1].hash);
  return 1;
}
static int command(BkEndingAudio *owner, BkAudio *reference,
                   BkAudioClip *clips[N], unsigned slot,
                   BkEndingAudioOperation op, int32_t volume, char e[256]) {
  BkEndingAudioCall call = {
      .operation = op, .slot = slot, .flags = 1, .volume = volume};
  int playing;
  if (!bk_ending_audio_call(owner, 0, 0, 0, &call, &playing, e))
    return 0;
  if (!clips[slot])
    return 1;
  if (op == BK_ENDING_AUDIO_RESTART)
    return bk_audio_play(reference, FIRST + slot, clips[slot], 1, volume, 0, e);
  assert(op == BK_ENDING_AUDIO_PAUSE);
  return bk_audio_pause(reference, FIRST + slot, e);
}
static int bind_voice(BkEndingAudio *owner, BkResourceStore *store,
                      BkAudio *reference, BkAudioClip *clips[N], unsigned slot,
                      const char *name, char e[256]) {
  if (!bk_ending_audio_bind(owner, slot, "bk3_06", name, e) ||
      !bk_audio_clear(reference, FIRST + slot, e))
    return 0;
  bk_audio_clip_release(clips[slot]);
  clips[slot] = bk_audio_clip_load(store, "bk3_06", name, e);
  return clips[slot] != NULL;
}
typedef struct {
  BkEndingAudio *owner;
  unsigned pauses, lights;
} Context;
static int presence(void *p, unsigned slot, int *out, char e[256]) {
  if (bk_ending_audio_present(((Context *)p)->owner, slot, out))
    return 1;
  snprintf(e, 256, "presence query failed");
  return 0;
}
static int sound_status(void *p, unsigned slot, int *out, char e[256]) {
  BkEndingAudioCall c = {.operation = BK_ENDING_AUDIO_STATUS, .slot = slot};
  return bk_ending_audio_call(((Context *)p)->owner, 0, 0, 0, &c, out, e);
}
static int sound_pause(void *p, unsigned slot, char e[256]) {
  Context *ctx = p;
  BkEndingAudioCall c = {.operation = BK_ENDING_AUDIO_PAUSE, .slot = slot};
  int out;
  if (!bk_ending_audio_call(ctx->owner, 0, 0, 0, &c, &out, e))
    return 0;
  ctx->pauses++;
  return 1;
}
static int light_observer(void *p, BkEndingReloadLight op, char e[256]) {
  (void)e;
  assert(op <= BK_ENDING_LIGHT_ENABLE);
  ((Context *)p)->lights++;
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  int rc = 1;
  char e[256] = {0}, path[1024], label[32];
  FILE *trace = fopen(argv[2], "r");
  BkResourceStore *store = bk_resources_create(e);
  BkAudio *mix[2] = {0};
  BkAudioClip *clips[N] = {0}, *guard = NULL;
  BkEndingAudio *owner = NULL;
  Sink sink[2] = {{.hash = UINT64_C(14695981039346656037)},
                  {.hash = UINT64_C(14695981039346656037)}};
  unsigned profiles = 0, frames = 0, pauses = 0, waits = 0, resumes = 0,
           aliases = 0;
  CHECK(trace && store);
  for (unsigned i = 0; i < 2; i++) {
    const char *pack = i ? "bk3_06" : "bk3_02";
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], pack);
    CHECK(bk_resources_mount(store, pack, path, e));
    BkAudioSink output = {&sink[i], 48000, 480, 1920, submit, poll};
    mix[i] = bk_audio_create(&output, e);
    CHECK(mix[i]);
  }
  guard = bk_audio_clip_load(store, "bk3_02", "se002.wav", e);
  CHECK(guard);
  for (unsigned i = 0; i < 2; i++)
    for (unsigned j = 0; j < 2; j++)
      CHECK(bk_audio_play(mix[i], j ? 63 : 0, guard, 1, -4000, 0, e));
  while (fscanf(trace, "%31s", label) == 1) {
    unsigned group, variant;
    CHECK(!strcmp(label, "ENTRY") &&
          fscanf(trace, "%u %u", &group, &variant) == 2);
    owner = bk_ending_audio_create_entry(store, mix[0], FIRST, group, variant,
                                         -1900, e);
    CHECK(owner);
    for (unsigned slot = 2; slot < N; slot++) {
      const char *name = slot == 47 ? bk_ending_sound_music(group, variant)
                                    : bk_ending_sound_effect(slot - 2);
      if (name) {
        clips[slot] = bk_audio_clip_load(store, "bk3_02", name, e);
        CHECK(clips[slot]);
      }
    }
    CHECK(bk_audio_play(mix[1], FIRST + 47, clips[47], 1, -1900, 0, e));
    for (unsigned slot = 0; slot < 6; slot++) {
      CHECK(bind_voice(owner, store, mix[1], clips, slot,
                       slot % 2 ? "PH12105.wav" : "PH11201.wav", e));
      aliases += slot >= 2;
    }
    Context ctx = {.owner = owner};
    BkEndingFrameState frame = {.group = group};
    BkEndingControlState control = {0};
    BkEndingAuxiliaryState aux = {0};
    int8_t previous = 8;
    int32_t selected = 0, next = 0;
    uint8_t saved[4] = {0};
    BkEndingReloadBindings b = {&frame,
                                &control,
                                &aux,
                                &previous,
                                &frame.transition_action,
                                &frame.curtain_wanted,
                                &selected,
                                &next,
                                saved};
    BkEndingReloadOps ops = {.context = &ctx,
                             .lighting = light_observer,
                             .present = presence,
                             .status = sound_status,
                             .pause = sound_pause};
    for (unsigned t = 0; t < 240; t++) {
      unsigned event, mode, action, wanted;
      unsigned long long mask;
      CHECK(fscanf(trace, "%31s %u %u %llx %u %u", label, &event, &mode, &mask,
                   &action, &wanted) == 6 &&
            !strcmp(label, "FRAME"));
      CHECK(advance(mix, sink, frames, e));
      if (event == 1) {
        for (unsigned slot = 0; slot < 47; slot++) {
          CHECK(command(owner, mix[1], clips, slot, BK_ENDING_AUDIO_RESTART,
                        -2300, e));
          if (clips[slot])
            for (unsigned i = 0; i < 2; i++)
              CHECK(bk_audio_frequency(mix[i], FIRST + slot, 12347 + slot * 211,
                                       e));
        }
      } else if (event == 2)
        CHECK(command(owner, mix[1], clips, 0, BK_ENDING_AUDIO_PAUSE, 0, e));
      else if (event == 3)
        for (unsigned slot = 0; slot < 47; slot++)
          if (clips[slot]) {
            for (unsigned i = 0; i < 2; i++)
              CHECK(bk_audio_resume(mix[i], FIRST + slot, 1, e));
            resumes++;
          }
      if (mode == 1)
        CHECK(bk_ending_reload_release(&b, &ops, e));
      else if (mode == 2) {
        frame.transition_action = 63;
        frame.curtain_wanted = 1;
        int early = -1;
        CHECK(bk_ending_reload_transition(&b, 3, &ops, &early, e));
        assert(!early);
        waits += frame.curtain_wanted == 1;
      }
      assert(frame.transition_action == action &&
             frame.curtain_wanted == wanted);
      for (unsigned slot = 0; slot < N; slot++)
        if (mask & (1ULL << slot)) {
          CHECK(bk_audio_pause(mix[1], FIRST + slot, e));
          pauses++;
        }
      /* Both voices, music and unrelated slots are compared too. Keeping a
       * fractional source phase across pause/resume must match every sample. */
      CHECK(compare(mix, sink, e));
      frames++;
    }
    assert(ctx.pauses == 164 && ctx.lights == 16);
    CHECK(bk_ending_audio_stop(owner, e));
    bk_ending_audio_destroy(owner);
    owner = NULL;
    for (unsigned slot = 0; slot < N; slot++) {
      CHECK(bk_audio_clear(mix[1], FIRST + slot, e));
      bk_audio_clip_release(clips[slot]);
      clips[slot] = NULL;
    }
    CHECK(advance(mix, sink, 3, e) && compare(mix, sink, e));
    profiles++;
  }
  assert(profiles == 10 && frames == 2400 && pauses == 1640 && waits == 40 &&
         aliases == 40 && sink[0].nonzero);
  printf("PASS ending reload PCM: %u profiles/%u frames, %u native pauses/%u "
         "voice waits/%u resumes/%u aliases; %llu exact PCM samples, "
         "FNV%016llx; 64 cursor/status slots and ten retirements\n",
         profiles, frames, pauses, waits, resumes, aliases,
         (unsigned long long)sink[0].samples, (unsigned long long)sink[0].hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL ending reload PCM: %s\n", e);
  if (trace)
    fclose(trace);
  bk_ending_audio_destroy(owner);
  for (unsigned slot = 0; slot < N; slot++)
    bk_audio_clip_release(clips[slot]);
  bk_audio_clip_release(guard);
  for (unsigned i = 0; i < 2; i++)
    bk_audio_destroy(mix[i]);
  bk_resources_destroy(store);
  return rc;
}
