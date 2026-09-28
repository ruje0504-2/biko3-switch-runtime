/* Explicit offline sink: consumer advances only under this test's control.
 * JSONL commands/cursors plus a WAV allow an independent reference audit. */
#include "scene/npc_audio.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  int16_t *samples;
  uint64_t submitted, consumed, capacity;
} Capture;
static int submit(void *ctx, const int16_t *pcm, size_t frames, char *error) {
  Capture *capture = ctx;
  if (capture->submitted + frames > capture->capacity) {
    snprintf(error, 256, "offline capture full");
    return 0;
  }
  memcpy(capture->samples + capture->submitted * 2, pcm, frames * 4);
  capture->submitted += frames;
  return 1;
}
static int poll(void *ctx, uint64_t *consumed, char *error) {
  (void)error;
  *consumed = ((Capture *)ctx)->consumed;
  return 1;
}
static int mount(BkResourceStore *store, const char *directory,
                 const char *pack, char *error) {
  char path[1024];
  if (snprintf(path, sizeof(path), "%s/%s.pp", directory, pack) >=
      (int)sizeof(path))
    return 0;
  return bk_resources_mount(store, pack, path, error);
}
static void command(BkAudio *audio, unsigned voice, const char *kind,
                    const char *pack, const char *name, int loop,
                    int32_t volume, int32_t pan) {
  printf(
      "{\"type\":\"%s\",\"at\":%" PRIu64 ",\"voice\":%u,"
      "\"pack\":\"%s\",\"name\":\"%s\",\"loop\":%d,\"volume\":%d,\"pan\":%d}\n",
      kind, bk_audio_stats(audio).submitted, voice, pack, name, loop, volume,
      pan);
}
static void u32(uint8_t *out, uint32_t value) {
  for (unsigned i = 0; i < 4; i++)
    out[i] = (uint8_t)(value >> (i * 8));
}
static int save(Capture *capture, const char *path, char *error) {
  FILE *file = fopen(path, "wb");
  if (!file) {
    snprintf(error, 256, "cannot open offline WAV");
    return 0;
  }
  uint8_t header[44] = {0};
  memcpy(header, "RIFF", 4);
  u32(header + 4, (uint32_t)(capture->consumed * 4 + 36));
  memcpy(header + 8, "WAVEfmt ", 8);
  u32(header + 16, 16);
  header[20] = 1;
  header[22] = 2;
  u32(header + 24, 48000);
  u32(header + 28, 192000);
  header[32] = 4;
  header[34] = 16;
  memcpy(header + 36, "data", 4);
  u32(header + 40, (uint32_t)(capture->consumed * 4));
  int ok = fwrite(header, 1, 44, file) == 44;
  for (uint64_t i = 0; ok && i < capture->consumed * 2; i++) {
    uint16_t value = (uint16_t)capture->samples[i];
    uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    ok = fwrite(bytes, 1, 2, file) == 2;
  }
  if (fclose(file))
    ok = 0;
  if (!ok)
    snprintf(error, 256, "offline WAV write failed");
  return ok;
}
static int frames(BkResourceStore *store, BkAudio *audio, BkNpcAudio *npc,
                  Capture *capture, const char *directory, char *error) {
  if (!mount(store, directory, "bk3_01", error) ||
      !mount(store, directory, "bk3_04", error) ||
      !bk_resources_mount_directory(store, "routes", directory, 20480, error) ||
      !bk_resources_mount_directory(store, "faces", directory, 20480, error))
    return 0;
  for (unsigned group = 0; group < 5; group++) {
    BkEntryRequest request = {.group = group, .area = 8, .previous_flow = 8};
    BkEntryAssets *entry = bk_entry_assets_create(store, &request, error);
    int ok = 0;
    if (!entry)
      return 0;
    BkFaceState face;
    uint32_t random = 1, clocks[] = {1000, 1000, 1000, 1000};
    if (!bk_entry_assets_load_mesh_shadow(entry, store, error) ||
        !bk_face_assets_initialize(bk_entry_assets_face(entry), &face, clocks,
                                   &random, error) ||
        !bk_npc_audio_stop(npc, error) ||
        !bk_npc_audio_speak(npc, "bk3_06", "PH10101.wav", 0, 0, error))
      goto entry_done;
    BkNpcSpatialState state = {.alpha = 1};
    state.ai.point.motion.action = 1;
    BkVoiceEnvelope envelope = {0};
    uint8_t latches[BK_NPC_FOOTSTEP_LATCH_COUNT] = {0};
    BkNpcFrameTailInput input = {
        .actions = {{1, 2}, {4, 5}, {10, 12}, {7, 9}}, /* explicit fixture */
        .effect_volume = -200,
        .presentation = {.seconds = .01f, .interface_mode = 2}};
    int voiced = 0;
    for (unsigned frame = 0; frame < 160; frame++) {
      capture->consumed = capture->submitted;
      if (!bk_audio_poll(audio, error) || !bk_audio_fill(audio, error))
        goto entry_done;
      input.presentation.timestamp_ms = 1000 + frame * 10;
      input.presentation.request_clock_ms = input.presentation.timestamp_ms;
      input.presentation.mouth_clock_ms = input.presentation.timestamp_ms;
      input.presentation.blink_clock_ms = input.presentation.timestamp_ms;
      if (!bk_npc_frame_tail(entry, npc, &state, &face, &random, &envelope,
                             latches, sizeof(latches), &input, error))
        goto entry_done;
      voiced |= envelope.smoothed > 0;
      bk_actor_pose_publish(bk_entry_assets_actor(entry));
      bk_npc_shadow_publish(bk_entry_assets_shadow(entry));
    }
    if (!voiced) {
      snprintf(error, 256, "NPC frame fixture did not consume voice samples");
      goto entry_done;
    }
    ok = 1;
  entry_done:
    bk_entry_assets_destroy(entry);
    if (!ok)
      return 0;
  }
  fprintf(
      stderr,
      "PASS NPC frame tail: 5 entries x160 frames, real voice/face/shadow\n");
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: audio-probe DATA_DIRECTORY OFFLINE.wav\n");
    return 2;
  }
  char error[256] = {0};
  Capture capture = {.capacity = 2400000};
  capture.samples = calloc((size_t)capture.capacity * 2, sizeof(int16_t));
  BkAudioSink sink = {&capture, 48000, 480, 1920, submit, poll};
  BkAudio *audio = bk_audio_create(&sink, error);
  BkResourceStore *store = bk_resources_create(error);
  BkNpcAudio *npc = NULL;
  int rc = 1;
  if (!capture.samples || !audio || !store ||
      !mount(store, argv[1], "bk3_02", error) ||
      !mount(store, argv[1], "bk3_06", error))
    goto done;
  npc = bk_npc_audio_create(store, audio, 0, 1, error);
  if (!npc || !bk_npc_audio_speak(npc, "bk3_06", "PH10101.wav", 0, 0, error))
    goto done;
  command(audio, 1, "play", "bk3_06", "PH10101.wav", 0, 0, 0);
  if (!bk_audio_fill(audio, error))
    goto done;
  BkVoiceEnvelope envelope = {0};
  const char *steps[] = {"se106.wav", "se107.wav", "se116.wav", "se117.wav",
                         "se118.wav", "se144.wav", "se148.wav"};
  for (unsigned i = 0; i < 1400; i++) {
    capture.consumed = i == 600 ? capture.submitted : capture.consumed + 480;
    if (!bk_audio_poll(audio, error))
      goto done;
    if (i % 37 == 0) {
      const char *name = steps[(i / 37) % 7];
      BkEntryNpcFootsteps event = {
          .footsteps = {.count = i % 2 + 1, .sound_file = name},
          .audio = {-(int32_t)(i % 2000), (int32_t)(i % 3001) - 1500}};
      if (!bk_npc_audio_footsteps(npc, &event, error))
        goto done;
      command(audio, 0, "play", "bk3_02", name, 0, event.audio.volume,
              event.audio.pan);
    }
    if (i == 280 || i == 740 || i == 1100) {
      const char *name = i == 280   ? "PH15102.wav"
                         : i == 740 ? "PH10201.wav"
                                    : "PH10101.wav";
      int loop = i == 740;
      if (!bk_npc_audio_speak(npc, "bk3_06", name, loop, 0, error))
        goto done;
      command(audio, 1, "play", "bk3_06", name, loop, 0, 0);
    }
    if (i == 320 || i == 380) {
      int32_t volume = i == 320 ? -10000 : -600;
      if (!bk_audio_gain(audio, 1, volume, 0, error))
        goto done;
      command(audio, 1, "gain", "", "", 0, volume, 0);
    }
    if (i == 1050) {
      if (!bk_npc_audio_stop(npc, error))
        goto done;
      command(audio, 0, "clear", "", "", 0, 0, 0);
      command(audio, 1, "clear", "", "", 0, 0, 0);
    }
    float level;
    BkAudioCursor cursor;
    if (!bk_npc_audio_voice(npc, &envelope, .01f, &level, error) ||
        !bk_audio_cursor(audio, 1, &cursor))
      goto done;
    printf(
        "{\"type\":\"cursor\",\"consumed\":%" PRIu64 ",\"submitted\":%" PRIu64
        ",\"source_frame\":%zu,\"playing\":%d,\"buffered\":%d,\"pending\":%d,"
        "\"target\":%.9g,\"smoothed\":%.9g,\"level\":%.9g}\n",
        capture.consumed, capture.submitted, cursor.source_frame,
        cursor.playing, cursor.buffered, cursor.pending_change, envelope.target,
        envelope.smoothed, level);
    if (!bk_audio_fill(audio, error))
      goto done;
  }
  if (!save(&capture, argv[2], error))
    goto done;
  printf("{\"type\":\"end\",\"frames\":%" PRIu64 ",\"queue_drains\":%" PRIu64
         ",\"offline\":true}\n",
         capture.consumed, bk_audio_stats(audio).queue_drains);
  if (!frames(store, audio, npc, &capture, argv[1], error))
    goto done;
  rc = 0;
  fprintf(
      stderr,
      "PASS offline audio capture and NPC frame adapter; no device claim\n");
done:
  if (rc)
    fprintf(stderr, "FAIL audio probe: %s\n", error);
  bk_npc_audio_destroy(npc);
  bk_audio_destroy(audio);
  bk_resources_destroy(store);
  free(capture.samples);
  return rc;
}
