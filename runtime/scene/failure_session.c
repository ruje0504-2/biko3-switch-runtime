#include "scene/failure_session.h"
int bk_failure_resources(uint32_t group, uint8_t outcome,
                         BkFailureResources *out) {
  if (!out || group >= 5 || outcome < 1 || outcome > 6)
    return 0;
  static const char *const speech[5] = {"PT13000.wav", "PT23000.wav",
                                        "PT33000.wav", "PT43000.wav",
                                        "PT53000.wav"};
  static const char *const text[5] = {"i01_04.txt", "i02_04.txt", "i03_04.txt",
                                      "i04_04.txt", "i05_04.txt"};
  static const int32_t offset[6] = {300, 0, 0, 100, 200, 400};
  *out =
      (BkFailureResources){speech[group], text[group],
                           (int32_t)(group + 1) * 10000 + offset[outcome - 1]};
  return 1;
}
int bk_failure_session_message(const BkFailureServices *s, uint32_t label,
                               char error[256]) {
  if (!s || !s->dialogue || !s->text.bind || label > INT32_MAX) {
    snprintf(error, 256, "failure message: missing bindings/invalid label");
    return 0;
  }
  BkDialogueAssets *d = s->dialogue;
  return bk_message_lookup(d->raw.data, d->raw.size, (int32_t)label,
                           &d->state.text, error) &&
         s->text.bind(s->text.context, &d->state.text, error);
}
int bk_failure_session_load(const BkFailureServices *s, BkGameFrameState *g,
                            BkTextFlow *flow, char error[256]) {
  BkFailureResources resources;
  if (!s || !g || !flow || !s->store || !s->entry || !s->npc_audio ||
      !s->player_audio || !s->dialogue || !s->items || !s->text.clear ||
      !s->text.recreate || !s->text.bind ||
      !bk_failure_resources(g->group, g->interaction.outcome, &resources)) {
    snprintf(error, 256, "failure loader: missing services/profile/outcome");
    return 0;
  }
  if (!bk_player_audio_release_buffer(s->player_audio, error) ||
      !bk_entry_assets_reload_player(s->entry, s->store,
                                     g->player.spatial.movement.position,
                                     g->player.spatial.movement.yaw, error))
    return 0;
  float player_height, npc_height;
  if (!bk_entry_assets_base_heights(s->entry, &player_height, &npc_height) ||
      !bk_player_failure_reset(&g->player, g->player_actions, player_height)) {
    snprintf(error, 256, "failure loader: invalid player reset");
    return 0;
  }
  g->player.completion_requested = (int8_t)g->interaction.outcome;
  g->player_hidden = g->player_sound_suppressed = g->hotkeys.menu_request = 0;
  g->interaction.response = 0;
  g->npc.ai.point.background_wait = g->npc.ai.point.fade_out = 0;
  if (!bk_npc_audio_prepare_speech(s->npc_audio, "bk3_06", resources.speech,
                                   s->speech_volume, error) ||
      !s->text.clear(s->text.context, error))
    return 0;
  bk_dialogue_assets_close(s->dialogue);
  bk_item_feedback_close_message(s->items);
  if (!bk_dialogue_assets_load(s->dialogue, s->store, resources.message,
                               error) ||
      !s->text.recreate(s->text.context, BK_NOTICE_TEXT_OPENING, error) ||
      !bk_failure_session_message(s, (uint32_t)resources.label, error))
    return 0;
  flow->started = 0;
  flow->enabled = 1;
  return 1;
}
