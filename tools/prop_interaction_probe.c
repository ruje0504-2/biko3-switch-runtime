/* Full4f4306 with real45 prop sets and WAVs. An independent rational PCM
 * reference consumes the exported commands, including pause/resume gaps.
 * Offline output, not a device or completed application test. */
#include "scene/prop_audio.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkPcm *pcm;
  uint64_t phase;
  double gain;
  int paused, loop;
} Voice;
typedef struct {
  Voice voices[16];
  uint64_t submitted, consumed, samples, nonzero, hash;
} Sink;
static int submit(void *context, const int16_t *pcm, size_t frames,
                  char *error) {
  Sink *s = context;
  for (size_t i = 0; i < frames; ++i) {
    float sum[2] = {0};
    for (unsigned j = 0; j < 16; ++j) {
      Voice *v = &s->voices[j];
      if (!v->pcm || v->paused)
        continue;
      size_t count = bk_pcm_frames(v->pcm), at = v->phase / 48000;
      if (at >= count)
        continue;
      size_t next = at + 1 < count ? at + 1 : v->loop ? 0 : at;
      unsigned channels = bk_pcm_channels(v->pcm);
      const int16_t *source = bk_pcm_samples(v->pcm);
      for (unsigned c = 0; c < 2; ++c) {
        unsigned channel = channels == 1 ? 0 : c;
        double a = source[at * channels + channel],
               b = source[next * channels + channel];
        sum[c] +=
            (float)((a + (b - a) * ((double)(v->phase % 48000) / 48000.)) *
                    v->gain);
      }
      v->phase += bk_pcm_rate(v->pcm);
      if (v->loop)
        v->phase %= count * 48000;
      else if (v->phase > count * 48000)
        v->phase = count * 48000;
    }
    for (unsigned c = 0; c < 2; ++c) {
      int expected = sum[c] <= -32768  ? -32768
                     : sum[c] >= 32767 ? 32767
                                       : (int)lroundf(sum[c]);
      if (abs(pcm[2 * i + c] - expected) > 1) {
        snprintf(error, 256,
                 "reference mismatch at frame %" PRIu64 " channel %u: %d != %d",
                 s->submitted + i, c, pcm[2 * i + c], expected);
        return 0;
      }
      uint16_t bits = (uint16_t)pcm[2 * i + c];
      for (unsigned b = 0; b < 2; ++b) {
        s->hash ^= (bits >> (8 * b)) & 255;
        s->hash *= UINT64_C(1099511628211);
      }
      ++s->samples;
      s->nonzero += pcm[2 * i + c] != 0;
    }
  }
  s->submitted += frames;
  return 1;
}
static int poll(void *context, uint64_t *consumed, char *error) {
  (void)error;
  *consumed = ((Sink *)context)->consumed;
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkModel *background = NULL;
  BkPropAssets *props = NULL;
  BkPropAudio *audio = NULL;
  BkAudio *mixer = NULL;
  BkBlob blob = {0};
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  float *world = NULL;
  int result = 1;
  unsigned frames = 0, plays = 0, pauses = 0, resumed = 0, prompts = 0,
           profiles = 0, instances = 0;
  if (!store)
    goto done;
  const char *packs[] = {"bk3_02", "bk3_03", "bk3_07"};
  for (unsigned i = 0; i < 3; ++i) {
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >=
            (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, error))
      goto done;
  }
  if (!bk_resources_mount_directory(store, "routes", argv[1], 20480, error) ||
      bk_resources_read(store, "bk3_03", "m01_01.x", &blob, error) !=
          BK_RESOURCE_OK ||
      bk_model_decode(blob.data, blob.size, &background, error) != BK_MODEL_OK)
    goto done;
  bk_blob_free(&blob);
  size_t floats = (size_t)background->frame_count * 16;
  world = malloc(floats * sizeof(float));
  if (!world || !bk_model_world_matrices(background, world, floats, error))
    goto done;
  BkPropInteractionInput in = {.wall = "unmatched"};
  for (unsigned i = 0; i < 21; ++i)
    in.player_actions[i] = (int32_t)i + 100;
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned a = 0; a < 9; ++a) {
      props =
          bk_prop_assets_create(store, g, a, background, world, floats, error);
      if (!props)
        goto done;
      unsigned count = bk_prop_assets_count(props);
      instances += count;
      ++profiles;
      BkAudioSink output = {&sink, 48000, 240, 960, submit, poll};
      sink.consumed = sink.submitted = 0;
      mixer = bk_audio_create(&output, error);
      audio = bk_prop_audio_create(store, mixer, 4, props, NULL, 0, 0, error);
      if (!audio)
        goto done;
      for (unsigned i = 0; i < count; ++i) {
        BkPropSoundCommand cmd;
        assert(bk_prop_sound_initial(bk_prop_assets_state(props, i)->kind, 0,
                                     &cmd));
        if (cmd.file) {
          if (bk_resources_read(store, "bk3_02", cmd.file, &blob, error) !=
              BK_RESOURCE_OK)
            goto done;
          sink.voices[i].pcm = bk_pcm_decode(blob.data, blob.size, error);
          bk_blob_free(&blob);
          if (!sink.voices[i].pcm)
            goto done;
          sink.voices[i].gain = pow(10., (double)cmd.initial_volume / 2000.);
          sink.voices[i].paused = !cmd.loop;
          sink.voices[i].loop = cmd.loop;
        }
      }
      BkPlayerControl player = {0};
      BkPropInteractionShared shared = {0};
      int8_t noise = 0;
      uint8_t outcome = 0;
      /* Target each actual actor, alternate near/far to exercise first Stop,
       * Play, paused gain retention and resume at a nonzero fractional phase.
       * Presentation is BEFORE interaction, matching51a682; no motion fixture
       * is inserted and cached roots are never republished by interaction. */
      for (unsigned target = 0; target < count; ++target)
        for (unsigned step = 0; step < 12; ++step) {
          const BkPropState *p = bk_prop_assets_state(props, target);
          memcpy(player.spatial.movement.position, p->path.position, 12);
          if (step % 4 == 0 || step % 4 == 3)
            player.spatial.movement.position[0] += 10000;
          else
            player.spatial.movement.position[0] += 1;
          player.spatial.movement.action = in.player_actions[3];
          in.now_ms = frames * 16;
          sink.consumed = sink.submitted;
          if (!bk_audio_poll(mixer, error) ||
              !bk_prop_assets_step_presentation(props, 1.f / 60, error))
            goto done;
          BkClipState clocks[16];
          float roots[16][16];
          for (unsigned i = 0; i < count; ++i) {
            BkPropInteractionActor bound;
            assert(bk_prop_assets_bind_interaction(props, i, &bound));
            memcpy(roots[i], bound.world, 64);
            assert(
                bk_actor_pose_state(bk_prop_assets_pose(props, i), &clocks[i]));
          }
          BkPropInteractionCommands cmd;
          if (!bk_prop_audio_interact(audio, props, &player, &noise, &outcome,
                                      &shared, &in, &cmd, error))
            goto done;
          for (unsigned i = 0; i < count; ++i) {
            BkClipState clock;
            BkPropInteractionActor bound;
            assert(bk_prop_assets_bind_interaction(props, i, &bound));
            assert(!memcmp(roots[i], bound.world, 64));
            assert(bk_actor_pose_state(bk_prop_assets_pose(props, i), &clock) &&
                   !memcmp(&clocks[i], &clock, sizeof(clock)));
          }
          for (unsigned i = 0; i < cmd.count; ++i) {
            BkPropInteractionSound *c = &cmd.sounds[i];
            Voice *v = &sink.voices[c->index];
            assert(v->pcm);
            if (c->play) {
              ++plays;
              resumed += v->paused && v->phase > 0;
              if (v->phase == bk_pcm_frames(v->pcm) * 48000)
                v->phase = 0;
              v->paused = 0;
              v->loop = c->loop;
            } else {
              ++pauses;
              v->paused = 1;
            }
          }
          prompts += shared.available;
          ++frames;
          if (!bk_audio_fill(mixer, error))
            goto done;
        }
      if (!bk_prop_audio_stop(audio, error))
        goto done;
      bk_prop_audio_destroy(audio);
      audio = NULL;
      bk_audio_destroy(mixer);
      mixer = NULL;
      bk_prop_assets_destroy(props);
      props = NULL;
      for (unsigned i = 0; i < 16; ++i) {
        bk_pcm_destroy(sink.voices[i].pcm);
        sink.voices[i] = (Voice){0};
      }
    }
  assert(plays && pauses && resumed && prompts && sink.nonzero);
  printf("PASS prop interactions: %u profiles, %u actors, %u frames, %u plays, "
         "%u pauses, %u nonzero resumes, %u prompts; %" PRIu64
         " PCM samples, %" PRIu64 " nonzero, FNV64 %016" PRIx64 "\n",
         profiles, instances, frames, plays, pauses, resumed, prompts,
         sink.samples, sink.nonzero, sink.hash);
  result = 0;
done:
  if (result)
    fprintf(stderr, "FAIL profile%u frame%u: %s\n", profiles, frames, error);
  bk_blob_free(&blob);
  if (audio) {
    char ignored[256];
    bk_prop_audio_stop(audio, ignored);
  }
  bk_prop_audio_destroy(audio);
  bk_audio_destroy(mixer);
  bk_prop_assets_destroy(props);
  for (unsigned i = 0; i < 16; ++i)
    bk_pcm_destroy(sink.voices[i].pcm);
  free(world);
  bk_model_destroy(background);
  bk_resources_destroy(store);
  return result;
}
