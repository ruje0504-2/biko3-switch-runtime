#ifndef BK_SCENE_GAME_FRAME_H
#define BK_SCENE_GAME_FRAME_H
#include "game/frame_dispatch.h"
#include "scene/background_audio.h"
#include "scene/item_assets.h"
#include "scene/item_feedback.h"
#include "scene/npc_audio.h"
#include "scene/npc_event_audio.h"
#include "scene/player_hotkeys.h"
#include "scene/prop_audio.h"
typedef struct {
  BkEntryAssets *entry;
  BkBackgroundAssets *background;
  BkPropAssets *props;
  BkItemAssets *items;
  BkBackgroundAudio *background_audio;
  BkPlayerAudio *player_audio;
  BkNpcAudio *npc_audio;
  BkNpcEventAudio *npc_events;
  BkPropAudio *prop_audio;
  BkAreaAudio *area_audio;
  BkItemFeedback *item_feedback;
  BkPlayerHotkeyServices hotkeys;
} BkGameFrameServices;
/* Live session state. No new-game defaults inferred here: initialization/save
 * restore must supply actual timers, action bindings, cached sensors and RNG.
 * camera.phase is the sole main phase, camera.transition is global729780. */
typedef struct {
  uint32_t group, area, random;
  BkGameCameraState camera;
  BkPlayerControl player;
  BkPlayerView player_view;
  uint8_t player_hidden, player_sound_suppressed;
  BkPlayerEventState player_events;
  uint8_t player_latches[BK_PLAYER_EVENT_LATCH_COUNT];
  /*709084 is shared by player4afe00 and NPC footsteps. */
  uint8_t shared_latches[BK_NPC_FOOTSTEP_LATCH_COUNT];
  int32_t player_actions[21];
  BkNpcFootstepActions npc_actions;
  BkNpcSpatialState npc;
  BkNpcEntryRoute npc_entry_route;
  float npc_vertical;
  uint32_t singular_camera_intersections;
  BkNpcInteractionState interaction;
  /* interaction.outcome owns player+7c8; player.completion_requested is
   * rebound around control and at frame return. AI df/e0 read inventory3/4. */
  BkFaceState face;
  BkVoiceEnvelope voice;
  BkBackgroundState background;
  BkPropShared props;
  BkPropInteractionShared prop_interaction;
  BkAreaBoundaryState boundary;
  BkItemPickupState pickup;
  BkPlayerHotkeys hotkeys;
  uint32_t album_group; /*B53954,51917c snapshots group before update*/
} BkGameFrameState;
/* Fresh PROCESS baseline before any menu/entry, not a "new game" reset.
 * Pinned PE globals + all27 game CRT constructors are zero for represented
 * state except shared lean10. 4e6dee seeds CRT rand with low32 time(NULL),
 * supplied here by platform. Never call this when changing area/reloading. */
int bk_scene_game_frame_boot_state(BkGameFrameState *, BkEntryProgress *,
                                   uint32_t epoch_seconds);
/* Actors, then mode2 player controller fields. For already resolved main2
 * entries; retains stage/transition and all unrelated state. This does not
 * execute the background/props/items/UI/resource loader or save restoration. */
int bk_scene_game_frame_initialize_entry(BkEntryAssets *, BkGameFrameState *,
                                         uint32_t route_start,
                                         const uint32_t face_clocks[4],
                                         char error[256]);
/* Apply represented4bf1a0/4fad20 actor state and4ebfd0 phase writes to fresh
 * entry assets. Face warm-up consumes the supplied clocks and retained RNG.
 * Must precede item creation (inventory reset affects its visibility).
 * Other session state, camera stage/transition, latches and UI stay retained;
 * enclosing loader/new-game, dialogue and save initialization are separate.
 * On any failure discard this entry/session, not a transaction across assets.
 */
int bk_scene_game_frame_initialize_actors(BkEntryAssets *, BkGameFrameState *,
                                          uint32_t route_start,
                                          const uint32_t face_clocks[4],
                                          char error[256]);
typedef struct {
  float seconds;
  uint32_t now_ms, face_clocks[4];
  int32_t music_volume, effect_volume;
  int8_t interface_mode;
  uint8_t hud_blocked;
  int weather_enabled;
  uint32_t movement_buttons, interaction_buttons, cover_buttons;
  uint32_t
      hotkey_buttons;   /*decoded edge aliases, BK_PLAYER_PAUSE/CAMERA/PHOTO*/
  uint8_t special_mode; /*BEF778, equality to1 matters*/
  float look[2];
  /* Device projection and retained camera/ray sensor snapshot, from before
   * this update. Only current NPC hidden/excluded surface are rebound here. */
  BkPlayerSceneInput player_scene;
  /* Retained per-prop+808/+810/+814 fields; live global NPC/player positions,
   * action bindings and excluded surface are rebound in the prop stage. */
  BkNpcHeadState prop_head[16];
} BkGameFrameInput;
typedef struct {
  BkGameFrameEvent events[17];
  unsigned count;
  uint32_t static_meshes, frame_meshes;
  BkItemPickups pickups;
} BkGameFrameResult;
/* Real component composition for51a682/4ec78d. Borrows all services/state;
 * mixer poll precedes, fill follows externally. Never publishes child world
 * caches. No51a190 phase/dialogue/UI transitions, menu hotkey mapping or draw
 * dispatch is fabricated. Failure is fatal to the session; do not retry the
 * partially mutated state or publish/draw it. Collision suffix is cleaned.
 * Trace/result is diagnostic and describes reached stages even on failure. */
int bk_scene_game_frame(const BkGameFrameServices *, BkGameFrameState *,
                        const BkGameFrameInput *, BkGameFrameResult *,
                        char error[256]);
/*51b244 composition on retained live resources. Loader4eb0ee must prepare
 * speech, reload player and reset its represented fields first. Camera FOV
 * changes are returned through the actual scene lens. No publication or UI. */
int bk_scene_failure_frame(const BkGameFrameServices *, BkGameFrameState *,
                           const BkGameFrameInput *, uint8_t *visible,
                           BkCameraLens *, BkGameFrameResult *,
                           char error[256]);
#endif
