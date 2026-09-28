/* Explicit six-outcome fixtures with actual assets, PCM and FTT output. */
#include "scene/entry_forest.h"
#include "scene/failure_session.h"
#include "scene/game_frame.h"
#include "scene/player_hud_session.h"
#include "ui/text_canvas.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  uint64_t submitted, samples, nonzero, hash;
} Sink;
static int submit(void *context, const int16_t *pcm, size_t frames,
                  char *error) {
  (void)error;
  Sink *s = context;
  s->submitted += frames;
  for (size_t i = 0; i < frames * 2; ++i) {
    s->nonzero += pcm[i] != 0;
    s->samples++;
    uint16_t bits = (uint16_t)pcm[i];
    for (unsigned b = 0; b < 2; ++b) {
      s->hash ^= (bits >> (b * 8)) & 255;
      s->hash *= UINT64_C(1099511628211);
    }
  }
  return 1;
}
static int poll(void *context, uint64_t *consumed, char *error) {
  (void)error;
  *consumed = ((Sink *)context)->submitted;
  return 1;
}
typedef struct {
  BkResourceStore *store;
  BkFont *font;
  BkTextCanvas *canvas;
  BkTextStyle style;
  BkMessage empty;
  const BkMessage *message;
  unsigned uploads;
} CpuText;
static int text_clear(void *context, char error[256]) {
  (void)error;
  CpuText *t = context;
  bk_text_canvas_destroy(t->canvas);
  t->canvas = NULL;
  bk_font_destroy(t->font);
  t->font = NULL;
  return 1;
}
static int text_recreate(void *context, BkNoticeTextKind kind,
                         char error[256]) {
  CpuText *t = context;
  text_clear(t, error);
  BkBlob blob = {0};
  if (!bk_notice_text_style(kind, &t->style) ||
      bk_resources_read(t->store, "fonts", "Type_S.FTT", &blob, error) !=
          BK_RESOURCE_OK)
    return 0;
  t->font = bk_font_decode(blob.data, blob.size, error);
  bk_blob_free(&blob);
  if (!t->font)
    return 0;
  t->canvas = bk_text_canvas_create(t->font, (uint32_t)t->style.width,
                                    (uint32_t)t->style.height, error);
  t->message = &t->empty;
  return t->canvas != NULL;
}
static int text_bind(void *context, const BkMessage *message, char error[256]) {
  (void)error;
  ((CpuText *)context)->message = message;
  return 1;
}
static int text_prepare(CpuText *t, BkTextFlow *flow, float seconds,
                        char error[256]) {
  BkTextDraw draw;
  int upload;
  if (!t->canvas || !t->message ||
      !bk_text_canvas_prepare(t->canvas, &t->style, t->message->bytes,
                              t->message->length, seconds, 640, flow, &draw,
                              &upload, error))
    return 0;
  t->uploads += upload;
  return 1;
}
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "failure probe line%d g%u outcome%u: %s\n", __LINE__,    \
              group, outcome, error);                                          \
      goto done;                                                               \
    }                                                                          \
  } while (0)
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[2048];
  unsigned group = 0, outcome = 0, profiles = 0, frames = 0;
  int status = 1;
  BkResourceStore *store = bk_resources_create(error);
  BkAudio *audio = NULL;
  BkGameFrameServices v = {0};
  BkDialogueAssets dialogue = {0};
  BkEntryForest *forest = NULL;
  CpuText text = {.store = store};
  Sink sink = {.hash = UINT64_C(14695981039346656037)};
  CHECK(store);
  const char *packs[] = {"bk3_01", "bk3_02", "bk3_03", "bk3_04",
                         "bk3_05", "bk3_06", "bk3_07", "bk3_16"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    CHECK(snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) <
          (int)sizeof(path));
    CHECK(bk_resources_mount(store, packs[i], path, error));
  }
  const char *loose[] = {"faces", "routes", "collision", "fonts"};
  for (unsigned i = 0; i < 4; ++i)
    CHECK(bk_resources_mount_directory(store, loose[i], argv[1],
                                       16 * 1024 * 1024, error));
  for (group = 0; group < 5; ++group)
    for (outcome = 1; outcome <= 6; ++outcome) {
      unsigned area = 0;
      if (outcome == 5) {
        int found = 0;
        for (area = 0; area < 9 && !found; ++area) {
          const BkPropConfig *config;
          uint32_t count;
          CHECK(bk_prop_config(&config, &count, group, area));
          for (unsigned i = 0; i < count; ++i) {
            BkPropSoundCommand command;
            CHECK(bk_prop_sound_initial(config[i].kind, -600, &command));
            if (command.file)
              found = 1;
          }
        }
        CHECK(found);
        --area;
      }
      BkEntryRequest request = {group, area, 0, 8};
      BkGameFrameState s;
      BkEntryProgress progress;
      CHECK(bk_scene_game_frame_boot_state(&s, &progress, 123));
      v.entry = bk_entry_assets_create(store, &request, error);
      CHECK(v.entry);
      CHECK(bk_scene_game_frame_initialize_entry(
          v.entry, &s, 0, (uint32_t[4]){1, 1, 1, 1}, error));
      CHECK(bk_entry_assets_load_mesh_shadow(v.entry, store, error));
      CHECK(bk_entry_assets_load_player_shadow(v.entry, store, error));
      v.background =
          bk_background_assets_create(store, group, area, 1, 0, error);
      CHECK(v.background);
      const BkActorPose *bg = bk_background_assets_pose(v.background, 0);
      const BkModel *model = bk_actor_pose_model(bg);
      size_t n = (size_t)model->frame_count * 16;
      float *world = malloc(n * sizeof(float));
      CHECK(world);
      for (unsigned i = 0; i < model->frame_count; ++i)
        memcpy(world + 16 * i, bk_actor_pose_frame(bg, i), 64);
      v.props =
          bk_prop_assets_create(store, group, area, model, world, n, error);
      free(world);
      CHECK(v.props);
      v.items = bk_item_assets_create(store, group, area, s.pickup.collected,
                                      (BkItemState[16]){{0}}, error);
      CHECK(v.items);
      sink.submitted = 0;
      BkAudioSink output = {&sink, 48000, 240, 960, submit, poll};
      audio = bk_audio_create(&output, error);
      CHECK(audio);
      v.player_audio = bk_player_audio_create(store, audio, 1, error);
      CHECK(v.player_audio);
      v.npc_audio = bk_npc_audio_create(store, audio, 2, 3, error);
      CHECK(v.npc_audio);
      v.item_feedback = bk_item_feedback_create(store, audio, 7, -600, error);
      CHECK(v.item_feedback);
      v.background_audio = bk_background_audio_create(
          store, audio, 8, bk_background_assets_config(v.background),
          &s.background, -600, error);
      CHECK(v.background_audio);
      v.prop_audio =
          bk_prop_audio_create(store, audio, 17, v.props, NULL, 0, -600, error);
      CHECK(v.prop_audio);
      if (outcome == 5) {
        int found = 0;
        for (unsigned i = 0; i < bk_prop_assets_count(v.props); ++i) {
          BkPropSoundCommand command;
          CHECK(bk_prop_sound_initial(bk_prop_assets_state(v.props, i)->kind,
                                      -600, &command));
          if (command.file) {
            s.prop_interaction.selected = (int32_t)i;
            found = 1;
            break;
          }
        }
        CHECK(found);
      }
      s.interaction.outcome = (uint8_t)outcome;
      s.player.completion_requested = (int8_t)outcome;
      s.hotkeys.photo_count = 17;
      s.interaction.response = 4;
      BkNpcSpatialState npc_before = s.npc;
      BkClipState clip_before, clip_after;
      CHECK(bk_actor_pose_state(bk_entry_assets_actor(v.entry), &clip_before));
      BkTextFlow flow = {.delay = 9, .scroll = 2, .target = 7};
      BkFailureServices f = {store,
                             v.entry,
                             v.npc_audio,
                             v.player_audio,
                             &dialogue,
                             v.item_feedback,
                             {&text, text_recreate, text_clear, text_bind},
                             -600};
      CHECK(bk_failure_session_load(&f, &s, &flow, error));
      CHECK(s.interaction.outcome == outcome && s.hotkeys.photo_count == 17 &&
            !s.interaction.response);
      CHECK(bk_actor_pose_state(bk_entry_assets_actor(v.entry), &clip_after));
      CHECK(!memcmp(&clip_before, &clip_after, sizeof(clip_before)));
      npc_before.ai.point.background_wait = npc_before.ai.point.fade_out = 0;
      CHECK(!memcmp(&npc_before, &s.npc, sizeof(s.npc)));
      CHECK(dialogue.state.text.length > 0 && flow.started == 0 &&
            flow.enabled == 1 && flow.delay == 9);
      int playing;
      CHECK(bk_audio_playing(audio, 3, &playing) && !playing);
      forest = bk_entry_forest_create_failure(v.entry, v.background, v.props,
                                              v.items, error);
      CHECK(forest);
      unsigned objects = bk_entry_forest_count(forest);
      CHECK(bk_entry_forest_object(forest, objects - 2)->kind ==
            BK_ENTRY_TREE_PLAYER);
      CHECK(bk_entry_forest_object(forest, objects - 1)->kind ==
            BK_ENTRY_TREE_PLAYER_SHADOW);
      BkCameraLens lens = {1, .75f, .5f, 126384};
      uint8_t visible = 0;
      BkGameFrameInput input = {
          .seconds = .25f, .music_volume = -900, .effect_volume = -600};
      BkGameFrameState held = s;
      BkClipState track_before, track_after;
      CHECK(bk_follow_camera_clip_state(bk_entry_assets_camera(v.entry),
                                        &track_before));
      for (unsigned step = 0; step < 90; ++step) {
        input.now_ms = 1000 + step * 250;
        for (unsigned i = 0; i < 4; ++i)
          input.face_clocks[i] = input.now_ms;
        CHECK(bk_audio_poll(audio, error));
        BkGameFrameResult result;
        CHECK(bk_scene_failure_frame(&v, &s, &input, &visible, &lens, &result,
                                     error));
        CHECK(result.count == 4 &&
              result.events[0] == BK_FRAME_PLAYER_PRESENTATION &&
              result.events[3] == BK_FRAME_PROP_PRESENTATION);
        CHECK(!memcmp(&s.npc.path, &held.npc.path, sizeof(s.npc.path)));
        CHECK(!memcmp(s.player.spatial.movement.position,
                      held.player.spatial.movement.position, 12));
        CHECK(s.pickup.collected[0] == held.pickup.collected[0]);
        CHECK(bk_follow_camera_clip_state(bk_entry_assets_camera(v.entry),
                                          &track_after));
        CHECK(!memcmp(&track_before, &track_after, sizeof(track_before)));
        const BkFrameVisit *visits;
        uint32_t count;
        CHECK(bk_entry_forest_draw(forest, 0, &visits, &count, error));
        if (visible)
          CHECK(text_prepare(&text, &flow, input.seconds, error));
        CHECK(bk_audio_fill(audio, error));
        ++frames;
      }
      CHECK(visible == 1);
      if (outcome != 1)
        CHECK(lens.fov_y == .2f);
      if (outcome == 6) {
        CHECK(bk_failure_session_message(&f, 10401 + group * 10000, error));
        CHECK(dialogue.state.text.length > 0);
      }
      text_clear(&text, error);
      bk_dialogue_assets_close(&dialogue);
      bk_entry_forest_destroy(forest);
      forest = NULL;
      bk_npc_audio_destroy(v.npc_audio);
      bk_player_audio_destroy(v.player_audio);
      bk_background_audio_destroy(v.background_audio);
      bk_prop_audio_destroy(v.prop_audio);
      bk_item_feedback_destroy(v.item_feedback);
      bk_audio_destroy(audio);
      audio = NULL;
      bk_item_assets_destroy(v.items);
      bk_prop_assets_destroy(v.props);
      bk_background_assets_destroy(v.background);
      bk_entry_assets_destroy(v.entry);
      v = (BkGameFrameServices){0};
      ++profiles;
    }
  printf(
      "PASS failure-session profiles=%u frames=%u glyph-uploads=%u PCM=%" PRIu64
      " nonzero=%" PRIu64 " hash=%016" PRIx64 "\n",
      profiles, frames, text.uploads, sink.samples, sink.nonzero, sink.hash);
  status = 0;
done:
  text_clear(&text, error);
  bk_dialogue_assets_close(&dialogue);
  bk_entry_forest_destroy(forest);
  bk_npc_audio_destroy(v.npc_audio);
  bk_player_audio_destroy(v.player_audio);
  bk_background_audio_destroy(v.background_audio);
  bk_prop_audio_destroy(v.prop_audio);
  bk_item_feedback_destroy(v.item_feedback);
  bk_audio_destroy(audio);
  bk_item_assets_destroy(v.items);
  bk_prop_assets_destroy(v.props);
  bk_background_assets_destroy(v.background);
  bk_entry_assets_destroy(v.entry);
  bk_resources_destroy(store);
  return status;
}
