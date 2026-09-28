#include "scene/dialogue_actor_assets.h"
#include "scene/dialogue_audio.h"
#include "world/menu_camera.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(v)                                                               \
  do {                                                                         \
    if (!(v))                                                                  \
      goto done;                                                               \
  } while (0)
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
/* Consumes original-x86 command trace, not the portable controller. */
typedef struct {
  BkAudioClip *speech, *music;
  int32_t speech_volume;
} Reference;
static int reference_command(Reference *r, BkResourceStore *store, BkAudio *m,
                             FILE *trace, char e[256]) {
  int kind, volume, loop;
  char pack[256], name[256];
  if (fscanf(trace, "%d %255s %255s %d %d", &kind, pack, name, &volume,
             &loop) != 5)
    return 0;
  switch (kind) {
  case 0:
    if (!bk_audio_clear(m, 61, e))
      return 0;
    bk_audio_clip_release(r->speech);
    r->speech = NULL;
    return 1;
  case 1:
    assert(!r->speech);
    r->speech = bk_audio_clip_load(store, pack, name, e);
    r->speech_volume = volume;
    return r->speech != NULL;
  case 2:
    return bk_audio_play(m, 61, r->speech, 0, r->speech_volume, 0, e);
  case 3:
    return bk_audio_gain(m, 60, volume, 0, e);
  case 4:
    return bk_audio_pause(m, 60, e);
  case 5: {
    BkAudioClip *next = bk_audio_clip_load(store, pack, name, e);
    if (!next)
      return 0;
    int ok = bk_audio_play(m, 60, next, loop, volume, 0, e);
    if (ok) {
      bk_audio_clip_release(r->music);
      r->music = next;
    } else
      bk_audio_clip_release(next);
    return ok;
  }
  }
  return 0;
}
/* Independent audible-PCM envelope; no adapter/envelope API. */
static float envelope(BkVoiceEnvelope *s, const BkAudioCursor *c, float dt) {
  if (!c->playing || !c->buffered)
    return 0;
  size_t channels = bk_pcm_channels(c->pcm),
         count = bk_pcm_frames(c->pcm) * channels;
  if (count < 221)
    return 0;
  const int16_t *pcm = bk_pcm_samples(c->pcm);
  int32_t sum = 0;
  for (unsigned i = 0; i < 220; i++) {
    int x = pcm[(c->source_frame * channels + i) % count];
    sum += x < 0 ? -x : x;
  }
  s->target = (float)((sum / 110) / 512.);
  if (s->target < s->smoothed) {
    s->smoothed = (float)((double)s->smoothed - 10. * dt);
    if (s->smoothed <= s->target)
      s->smoothed = s->target;
  } else if (s->target > s->smoothed) {
    s->smoothed = (float)((double)s->smoothed + 10. * dt);
    if (s->smoothed >= s->target)
      s->smoothed = s->target;
  } else
    s->smoothed = s->target;
  if (s->smoothed >= 9)
    s->smoothed = 9;
  return s->smoothed;
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  char error[256] = {0}, path[1024];
  int rc = 1;
  FILE *trace = fopen(argv[2], "r");
  BkResourceStore *store = bk_resources_create(error);
  Sink sinks[2] = {{.hash = UINT64_C(14695981039346656037)},
                   {.hash = UINT64_C(14695981039346656037)}};
  BkAudio *mix[2] = {0};
  BkDialogueAudio *audio = NULL;
  BkDialogueActorAssets *actor = NULL;
  Reference ref = {0};
  BkVoiceEnvelope actual_env = {0}, ref_env = {0};
  BkDialogueMusic music = {0};
  BkDialogue dialogue = {0};
  BkDialogueActorState actor_state = {0};
  BkTimer timer = {0};
  uint8_t phase = 0;
  unsigned frames = 0, commands = 0, levels = 0, active = 0, paused = 0,
           loads = 0;
  uint32_t rng = 97531;
  int last_group = -1;
  BkMenuCamera camera = {0};
  CHECK(bk_menu_camera_dialogue(&camera));
  CHECK(trace && store);
  const char *packs[] = {"bk3_01", "bk3_02", "bk3_06"};
  for (unsigned i = 0; i < 3; i++) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  for (unsigned i = 0; i < 2; i++) {
    mix[i] = bk_audio_create(
        &(BkAudioSink){&sinks[i], 48000, 480, 1920, submit, poll}, error);
    CHECK(mix[i]);
  }
  for (;;) {
    int group, tick, master, sp, mp, pause, expected_volume, expected_wanted,
        expected_sp, expected_mp, n;
    float dt;
    char speech[256], song[256];
    int read = fscanf(trace, "%d %d %f %d %d %d %d %255s %255s %d %d %d %d %d",
                      &group, &tick, &dt, &master, &sp, &mp, &pause, speech,
                      song, &expected_volume, &expected_wanted, &expected_sp,
                      &expected_mp, &n);
    if (read == EOF)
      break;
    assert(read == 14 && group >= 0 && group < 5 && n >= 0 && n <= 8);
    if (tick == -1) {
      assert(group == last_group + 1);
      last_group = group;
      if (audio)
        CHECK(bk_dialogue_audio_stop(audio, error));
      bk_dialogue_audio_destroy(audio);
      audio = NULL;
      bk_dialogue_actor_assets_destroy(actor);
      actor = NULL;
      CHECK(bk_audio_clear(mix[1], 60, error) &&
            bk_audio_clear(mix[1], 61, error));
      bk_audio_clip_release(ref.speech);
      bk_audio_clip_release(ref.music);
      ref = (Reference){0};
      audio = bk_dialogue_audio_create(store, mix[0], 60, 61, error);
      CHECK(audio);
      uint32_t clock = 1000 + frames * 50,
               clocks[4] = {clock, clock + 1, clock + 2, clock + 3};
      actor =
          bk_dialogue_actor_assets_create(store, group, clocks, &rng, error);
      CHECK(actor);
      actor_state = (BkDialogueActorState){0};
      CHECK(bk_dialogue_actor_initialize(&actor_state, group));
      timer = (BkTimer){11, 0, 1};
      phase = 0;
      music = (BkDialogueMusic){-1234, 255};
      dialogue = (BkDialogue){.code_e = 9};
      loads++;
    }
    assert(audio && actor);
    for (unsigned i = 0; i < 2; i++) {
      uint64_t advance = (unsigned[]){0, 240, 480, 960, 1920, 3000}[frames % 6];
      uint64_t queued = sinks[i].submitted - sinks[i].consumed;
      sinks[i].consumed += advance < queued ? advance : queued;
      CHECK(bk_audio_poll(mix[i], error));
      sinks[i].count = 0;
    }
    dialogue.sound_pending = (uint8_t)sp;
    dialogue.music_pending = (uint8_t)mp;
    snprintf(dialogue.sound, sizeof(dialogue.sound), "%s",
             !strcmp(speech, "-") ? "" : speech);
    snprintf(dialogue.music, sizeof(dialogue.music), "%s",
             !strcmp(song, "-") ? "" : song);
    if (tick == -1)
      CHECK(bk_dialogue_audio_music_open(audio, &music, &dialogue, error));
    else {
      /* Actor consumes previously queued audio; text/sound updates follow.
       * Full outer dialogue sequencing remains a later integration gate. */
      float mouth = 0;
      dialogue.code_c = tick % 7 ? group + 1 : 0;
      if (dialogue.code_c) {
        BkAudioCursor c;
        assert(bk_audio_cursor(mix[1], 61, &c));
        float wanted = envelope(&ref_env, &c, dt);
        CHECK(bk_dialogue_audio_voice_level(audio, &actual_env, dt, &mouth,
                                            error));
        assert(mouth == wanted &&
               !memcmp(&actual_env, &ref_env, sizeof(ref_env)));
        levels++;
        active += mouth > 0;
      }
      BkDialogueActorInput input = {.game_seconds = dt,
                                    .mouth_level = mouth,
                                    .timestamp_ms = 1100 + frames * 50,
                                    .timer_clock_ms = 1101 + frames * 50,
                                    .face_clocks = {1102 + frames * 50,
                                                    1103 + frames * 50,
                                                    1104 + frames * 50}};
      memcpy(input.camera_world, camera.pose.world, 64);
      CHECK(bk_dialogue_actor_assets_step(actor, &actor_state, &dialogue,
                                          &phase, &timer, &input, &rng, error));
      assert(actor_state.mouth == mouth);
      bk_actor_pose_publish(bk_dialogue_actor_assets_pose(actor));
      if (pause) {
        CHECK(bk_dialogue_audio_speech_pause(audio, error));
        if (ref.speech)
          CHECK(bk_audio_pause(mix[1], 61, error));
        paused++;
      }
      CHECK(bk_dialogue_audio_speech_step(audio, &dialogue, -700 - group * 200,
                                          error));
      CHECK(bk_dialogue_audio_music_step(audio, &music, &dialogue, dt, master,
                                         error));
      frames++;
    }
    assert(music.volume == expected_volume && music.wanted == expected_wanted &&
           dialogue.sound_pending == expected_sp &&
           dialogue.music_pending == expected_mp);
    for (int i = 0; i < n; i++) {
      CHECK(reference_command(&ref, store, mix[1], trace, error));
      commands++;
    }
    for (unsigned voice = 60; voice <= 61; voice++) {
      int a, b;
      assert(bk_audio_playing(mix[0], voice, &a) &&
             bk_audio_playing(mix[1], voice, &b) && a == b);
      BkAudioCursor ca, cb;
      assert(bk_audio_cursor(mix[0], voice, &ca) &&
             bk_audio_cursor(mix[1], voice, &cb));
      assert(ca.source_frame == cb.source_frame && ca.playing == cb.playing &&
             ca.buffered == cb.buffered &&
             ca.pending_change == cb.pending_change);
      assert((ca.pcm != NULL) == (cb.pcm != NULL));
      if (ca.pcm)
        assert(bk_pcm_frames(ca.pcm) == bk_pcm_frames(cb.pcm) &&
               bk_pcm_rate(ca.pcm) == bk_pcm_rate(cb.pcm));
    }
    for (unsigned i = 0; i < 2; i++)
      CHECK(bk_audio_fill(mix[i], error));
    assert(sinks[0].count == sinks[1].count &&
           !memcmp(sinks[0].batch, sinks[1].batch,
                   sinks[0].count * sizeof(int16_t)));
    assert(sinks[0].submitted == sinks[1].submitted &&
           sinks[0].hash == sinks[1].hash);
  }
  assert(frames == 1800 && loads == 5 && commands == 1930 && levels > 1000 &&
         active > 100 && paused == 10 && sinks[0].nonzero);
  /* A real missing file preserves the pending request and the completed
   * release/stop prefix. It must never silently acknowledge successful audio.
   */
  dialogue.sound_pending = 1;
  snprintf(dialogue.sound, sizeof(dialogue.sound),
           "missing-dialogue-voice.wav");
  CHECK(!bk_dialogue_audio_speech_step(audio, &dialogue, -700, error));
  assert(dialogue.sound_pending == 1);
  int playing;
  assert(bk_audio_playing(mix[0], 61, &playing) && !playing);
  music = (BkDialogueMusic){-6000, 0};
  dialogue.music_pending = 1;
  snprintf(dialogue.music, sizeof(dialogue.music),
           "missing-dialogue-music.wav");
  CHECK(
      !bk_dialogue_audio_music_step(audio, &music, &dialogue, 0, -700, error));
  assert(dialogue.music_pending == 1 && music.wanted == 0);
  assert(bk_audio_playing(mix[0], 60, &playing) && !playing);
  CHECK(bk_dialogue_audio_stop(audio, error));
  printf("PASS dialogue audio: %u actor frames, %u original commands, %u exact "
         "envelopes (%u nonzero), %u pauses, %llu exact PCM samples, hash "
         "%016llx; missing media retained pending\n",
         frames, commands, levels, active, paused,
         (unsigned long long)sinks[0].samples,
         (unsigned long long)sinks[0].hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  if (trace)
    fclose(trace);
  if (audio) {
    char ignored[256];
    bk_dialogue_audio_stop(audio, ignored);
  }
  bk_dialogue_audio_destroy(audio);
  bk_dialogue_actor_assets_destroy(actor);
  bk_audio_clip_release(ref.speech);
  bk_audio_clip_release(ref.music);
  for (unsigned i = 0; i < 2; i++)
    bk_audio_destroy(mix[i]);
  bk_resources_destroy(store);
  return rc;
}
