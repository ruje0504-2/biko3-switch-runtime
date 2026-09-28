/* Actual entry prop lifecycle; explicit empty static-collision fixture.
 * Anchors use the real(0,5) background. Does not execute sound/game flow. */
#include "scene/prop_assets.h"
#include "scene/prop_audio.h"
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
int main(int argc, char **argv) {
  if (argc != 2 && (argc != 3 || strcmp(argv[2], "--audio")))
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkModel *background = NULL;
  BkCollision *collision = NULL;
  BkPropAssets *props = NULL;
  BkAudio *mixer = NULL;
  BkPropAudio *audio = NULL;
  BkPropSoundState retained[16] = {0};
  AudioSink sink = {.hash = UINT64_C(14695981039346656037)};
  int with_audio = argc == 3;
  float *world = NULL;
  unsigned char *atr = calloc(1, BK_COLLISION_ATR_SIZE);
  BkBlob blob = {0};
  int rc = 1;
  unsigned profiles = 0, instances = 0, frames = 0, meshes = 0;
  unsigned stopped_static = 0;
  if (!store || !atr)
    goto done;
  const char *packs[] = {"bk3_03", "bk3_07", "bk3_02"};
  for (unsigned i = 0; i < (with_audio ? 3u : 2u); i++) {
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
  size_t n = (size_t)background->frame_count * 16;
  world = malloc(n * sizeof(float));
  if (!world || !bk_model_world_matrices(background, world, n, error))
    goto done;
  collision = bk_collision_create(background, world, n, "m01_01.x", atr,
                                  BK_COLLISION_ATR_SIZE, error);
  if (!collision)
    goto done;
  if (with_audio) {
    BkAudioSink output = {&sink, 48000, 480, 1920, submit, poll};
    mixer = bk_audio_create(&output, error);
    if (!mixer)
      goto done;
  }
  BkPropShared shared = {0};
  for (unsigned g = 0; g < 5; g++)
    for (unsigned a = 0; a < 9; a++) {
      props = bk_prop_assets_create(store, g, a, background, world, n, error);
      if (!props)
        goto done;
      const BkPropConfig *config;
      uint32_t count;
      assert(bk_prop_config(&config, &count, g, a));
      assert(count == bk_prop_assets_count(props));
      instances += count;
      profiles++;
      if (with_audio) {
        audio = bk_prop_audio_create(store, mixer, 4, props, retained, 16, 0,
                                     error);
        if (!audio || !bk_audio_fill(mixer, error))
          goto done;
        for (unsigned i = 0; i < count; ++i)
          assert(bk_prop_audio_state(audio, i)->stage == retained[i].stage);
      }
      for (unsigned i = 0; i < count; i++) {
        const BkPropState *s = bk_prop_assets_state(props, i);
        const BkActorPose *p = bk_prop_assets_pose(props, i);
        assert(s->kind == config[i].kind && s->action == 0 &&
               s->path.cursor == config[i].cursor);
        assert(!memcmp(s->path.position, bk_actor_pose_placement(p)->position,
                       12));
        if (config[i].anchor) {
          uint32_t f;
          assert(bk_model_find_frame(background, config[i].anchor, &f, error));
          assert(
              !memcmp(bk_actor_pose_placement(p)->world, world + f * 16, 64));
        }
      }
      for (unsigned step = 0; step < 240; step++) {
        BkPropMotionInput input = {.seconds = 1.0f / 60.0f,
                                   .npc_last_crossed = 5};
        for (unsigned i = 0; i < 21; i++)
          input.player_actions[i] = (int32_t)i + 100;
        input.player_action = input.player_actions[(step / 30) % 2 ? 13 : 12];
        int car_reaction = getenv("BK_CAR_IMPACT") && step >= 120 && step < 180;
        if (car_reaction)
          input.player_action = input.player_actions[10];
        BkPropRoute before[16];
        for (unsigned i = 0; i < count; i++)
          before[i] = bk_prop_assets_state(props, i)->path;
        input.player_mode = step % 3;
        input.background_clip = (step / 40) % 3;
        input.npc_previous_flags[0] = step % 7 == 0 ? 3 : 0;
        BkNpcSceneInput ground[16] = {0};
        for (unsigned i = 0; i < count; i++) {
          ground[i].excluded_surface = "";
          ground[i].cone.player_action = input.player_action;
        }
        if (!bk_prop_assets_collision(props, collision, error))
          goto done;
        meshes += bk_collision_count(collision);
        BkPropMotionEffects effects[16];
        if (with_audio) {
          uint64_t remaining = sink.submitted - sink.consumed;
          sink.consumed += remaining < 800 ? remaining : 800;
          if (!bk_audio_poll(mixer, error))
            goto done;
          BkPropSoundInput listener = {.listener_yaw = (float)step * 1.5f,
                                       .effect_volume = step > 160 ? -1000 : 0};
          if (count)
            memcpy(listener.listener,
                   bk_prop_assets_state(props, 0)->path.position, 12);
          listener.listener[0] += (float)(step % 80) * 2;
          listener.listener[2] += 30;
          if (step == 120)
            for (unsigned i = 0; i < count; ++i)
              if (config[i].kind == 0 ||
                  (config[i].kind >= 13 && config[i].kind <= 17))
                assert(bk_prop_audio_set_stage(audio, i, 4));
          if (!bk_prop_audio_step(audio, props, &shared, &input, collision,
                                  ground, count, effects, &listener, error) ||
              !bk_audio_fill(mixer, error))
            goto done;
          for (unsigned i = 0; i < count; ++i) {
            BkAudioCursor cursor;
            assert(bk_audio_cursor(mixer, 4 + i, &cursor));
            if (cursor.pcm)
              assert(cursor.source_frame <= bk_pcm_frames(cursor.pcm));
          }
        } else if (!bk_prop_assets_step_spatial(props, &shared, &input,
                                                collision, ground, count,
                                                effects, error))
          goto done;
        for (unsigned i = 0; i < count; i++) {
          const BkActorPose *p = bk_prop_assets_pose(props, i);
          const BkPropState *s = bk_prop_assets_state(props, i);
          assert(!memcmp(s->path.position, bk_actor_pose_placement(p)->position,
                         12));
          if (car_reaction && s->path.last <= 0) {
            /* Ground may adjust Y, but a zero-distance static object must
             * neither require negative route indices nor move in X/Z. */
            assert(s->path.position[0] == before[i].position[0] &&
                   s->path.position[2] == before[i].position[2] &&
                   s->path.yaw == before[i].yaw &&
                   s->path.cursor == before[i].cursor);
            stopped_static++;
          }
        }
        if (!bk_prop_assets_step_presentation(props, input.seconds, error))
          goto done;
        bk_collision_end_props(collision);
        if (step % 7 != 3)
          bk_prop_assets_publish(props);
        frames += count;
      }
      /* CPU collision owns its copies across instance destruction. */
      if (!bk_prop_assets_collision(props, collision, error))
        goto done;
      if (with_audio) {
        for (unsigned i = 0; i < count; ++i)
          retained[i] = *bk_prop_audio_state(audio, i);
        if (!bk_prop_audio_stop(audio, error))
          goto done;
        bk_prop_audio_destroy(audio);
        audio = NULL;
      }
      bk_prop_assets_destroy(props);
      props = NULL;
      for (uint32_t i = 0; i < bk_collision_count(collision); i++) {
        const BkCollisionMesh *m = bk_collision_mesh(collision, i);
        assert(m->vertex_count && isfinite(m->vertices[0][0]));
      }
      bk_collision_end_props(collision);
    }
  printf("PASS %u profiles, %u actual props, %u instance frames, %u dynamic "
         "mesh snapshots; explicit empty static fixture\n",
         profiles, instances, frames, meshes);
  if (with_audio) {
    assert(sink.nonzero && !bk_audio_stats(mixer).failed);
    printf("PASS offline prop audio: %" PRIu64 " stereo frames, %" PRIu64
           " nonzero samples, FNV64 %016" PRIx64
           "; retained stage reloads and car one-shots\n",
           sink.submitted, sink.nonzero, sink.hash);
  }
  if (getenv("BK_CAR_IMPACT")) {
    assert(stopped_static);
    printf("PASS car reaction: %u stationary object steps preserve X/Z/yaw/cursor\n",
           stopped_static);
  }
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL profile %u instances %u frames %u: %s\n", profiles,
            instances, frames, error);
  bk_blob_free(&blob);
  if (audio) {
    char ignored[256];
    bk_prop_audio_stop(audio, ignored);
  }
  bk_prop_audio_destroy(audio);
  bk_audio_destroy(mixer);
  bk_prop_assets_destroy(props);
  bk_collision_destroy(collision);
  free(atr);
  free(world);
  bk_model_destroy(background);
  bk_resources_destroy(store);
  return rc;
}
