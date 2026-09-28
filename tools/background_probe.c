/* Actual background CPU frames/collision and optional offline PCM mixer.
 * No audio device, render loop, mission driver or rain sprite claim. */
#include "scene/background_assets.h"
#include "scene/background_audio.h"
#include <inttypes.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  uint64_t submitted, consumed, hash, nonzero;
} AudioSink;
static int submit(void *context, const int16_t *pcm, size_t frames,
                  char *error) {
  (void)error;
  AudioSink *sink = context;
  for (size_t i = 0; i < frames * 2; ++i) {
    uint16_t bits = (uint16_t)pcm[i];
    for (unsigned j = 0; j < 2; ++j) {
      sink->hash ^= (bits >> (j * 8)) & 255;
      sink->hash *= UINT64_C(1099511628211);
    }
    sink->nonzero += pcm[i] != 0;
  }
  sink->submitted += frames;
  return 1;
}
static int poll(void *context, uint64_t *consumed, char *error) {
  (void)error;
  *consumed = ((AudioSink *)context)->consumed;
  return 1;
}
static int consume(void *context, const BkBackgroundCommands *c, char *error) {
  (void)error;
  unsigned *commands = context;
  *commands += c->music_update;
  for (unsigned i = 0; i < 8; ++i) {
    *commands += (c->ambient[i].action != 0) + c->ambient[i].spatial;
    assert(c->ambient[i].gain.volume >= -6000 &&
           c->ambient[i].gain.volume <= 0);
  }
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2 && (argc != 3 || strcmp(argv[2], "--audio")))
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkBackgroundAssets *a = NULL;
  BkBackgroundAudio *audio = NULL;
  BkAudio *mixer = NULL;
  int with_audio = argc == 3;
  AudioSink sink = {.hash = UINT64_C(14695981039346656037)};
  int rc = 1;
  unsigned profiles = 0, frames = 0, objects = 0, commands = 0, meshes = 0;
  if (!store)
    goto done;
  const char *packs[] = {"bk3_03", "bk3_17", "bk3_20", "bk3_02"};
  for (unsigned i = 0; i < (with_audio ? 4u : 3u); ++i) {
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >=
            (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, error))
      goto done;
  }
  if (!bk_resources_mount_directory(store, "collision", argv[1],
                                    BK_COLLISION_ATR_SIZE, error))
    goto done;
  BkBackgroundState state = {.music_volume = -6000, .music_mode = 1};
  if (with_audio) {
    BkAudioSink output = {&sink, 48000, 480, 1920, submit, poll};
    mixer = bk_audio_create(&output, error);
    if (!mixer)
      goto done;
  }
  uint32_t random = 0x4f737b, now = 0;
  for (unsigned quality = 0; quality < 2; ++quality)
    for (unsigned g = 0; g < 5; ++g)
      for (unsigned area = 0; area < 9; ++area) {
        a = bk_background_assets_create(store, g, area, quality, 1, error);
        if (!a)
          goto done;
        ++profiles;
        const BkBackgroundConfig *config = bk_background_assets_config(a);
        for (unsigned i = 0; i < 3; ++i)
          objects += bk_background_assets_pose(a, i) != NULL;
        meshes += bk_collision_count(bk_background_assets_collision(a));
        state.music_volume = -6000;
        if (with_audio) {
          BkBackgroundState retained = state;
          audio = bk_background_audio_create(store, mixer, 20, config, &state,
                                             -500, error);
          if (!audio || !bk_audio_fill(mixer, error))
            goto done;
          assert(!memcmp(&state, &retained, sizeof(state)));
        }
        for (unsigned step = 0; step < 180; ++step) {
          BkBackgroundInput in = {.now = now,
                                  .seconds = 1.f / 60.f,
                                  .music_master = -1000,
                                  .effect_master = -500,
                                  .player = {84, 0, 0},
                                  .npc = {200, 0, 0},
                                  .weather_enabled = 1};
          now += 16;
          in.player[0] += (float)(step % 80);
          in.player_yaw = (float)step * 2;
          in.ambient_gate = step % 2;
          state.music_mode = step < 120 ? 1 : 0;
          for (unsigned i = 0; i < 8; ++i) {
            const BkAmbientConfig *c = &config->ambient[i];
            in.ambient[i].present = c->file != NULL;
            in.ambient[i].playing = step % 7 < 4;
            in.ambient[i].loop = c->loop_mode != 0;
            in.ambient[i].trigger = c->trigger;
            memcpy(in.ambient[i].position, c->position, 12);
          }
          BkBackgroundCommands out;
          if (with_audio) {
            uint64_t remaining = sink.submitted - sink.consumed;
            sink.consumed += remaining < 800 ? remaining : 800;
            if (!bk_audio_poll(mixer, error) ||
                !bk_background_audio_step(audio, a, &state, &random, &in, &out,
                                          error) ||
                !bk_audio_fill(mixer, error))
              goto done;
            consume(&commands, &out, error);
            for (unsigned i = 0; i < 9; ++i) {
              BkAudioCursor cursor;
              assert(bk_audio_cursor(mixer, 20 + i, &cursor));
              if (cursor.pcm)
                assert(cursor.source_frame <= bk_pcm_frames(cursor.pcm));
            }
          } else if (!bk_background_assets_step(a, &state, &random, &in, &out,
                                                consume, &commands, error))
            goto done;
          if (step % 7 != 3)
            bk_background_assets_publish(a);
          for (unsigned i = 0; i < 3; ++i) {
            const BkActorPose *pose = bk_background_assets_pose(a, i);
            if (!pose)
              continue;
            BkClipState clip;
            assert(bk_actor_pose_state(pose, &clip) && isfinite(clip.source));
            const BkModel *model = bk_actor_pose_model(pose);
            for (uint32_t f = 0; f < model->frame_count; ++f) {
              const float *world = bk_actor_pose_frame(pose, f);
              for (unsigned k = 0; k < 16; ++k)
                assert(isfinite(world[k]));
            }
          }
          ++frames;
        }
        if (audio && !bk_background_audio_stop(audio, error))
          goto done;
        bk_background_audio_destroy(audio);
        audio = NULL;
        bk_background_assets_destroy(a);
        a = NULL;
      }
  printf("PASS %u background profiles (both qualities), %u objects, %u frames, "
         "%u audio commands, %u static meshes; %s\n",
         profiles, objects, frames, commands, meshes,
         with_audio ? "real PCM playback status"
                    : "explicit sound status fixture");
  if (with_audio) {
    assert(sink.nonzero && sink.submitted >= sink.consumed);
    printf("PASS background PCM %" PRIu64 " stereo frames, %" PRIu64
           " nonzero samples, FNV64 %016" PRIx64 "\n",
           sink.submitted, sink.nonzero, sink.hash);
  }
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL background profile %u frame %u: %s\n", profiles,
            frames, error);
  if (audio) {
    char ignored[256];
    bk_background_audio_stop(audio, ignored);
  }
  bk_background_audio_destroy(audio);
  bk_audio_destroy(mixer);
  bk_background_assets_destroy(a);
  bk_resources_destroy(store);
  return rc;
}
