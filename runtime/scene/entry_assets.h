#ifndef BK_SCENE_ENTRY_ASSETS_H
#define BK_SCENE_ENTRY_ASSETS_H
#include "game/actor_entry.h"
#include "game/area_entry.h"
#include "game/camera_policy.h"
#include "game/entry.h"
#include "game/npc_footsteps.h"
#include "game/npc_head.h"
#include "game/npc_spatial.h"
#include "game/player_control.h"
#include "game/player_events.h"
#include "game/player_spatial.h"
#include "game/player_view_policy.h"
#include "model/material_pose.h"
#include "resource/store.h"
#include "scene/eye_assets.h"
#include "scene/face_assets.h"
#include "scene/npc_shadow.h"
#include "world/actor_pose.h"
#include "world/follow_camera.h"
#include "world/spatial_audio.h"
/* CPU scene asset adapter for main-flow 2. Owns decoded route/models/XAN and
 * their pose instances. Resources are borrowed only during construction.
 * Does not step AI, decide camera branches, render actors or claim a mission
 * has started. Caller must mount bk3_01, bk3_04 and loose routes/faces
 * explicitly. */
typedef struct BkEntryAssets BkEntryAssets;
BkEntryAssets *bk_entry_assets_create(BkResourceStore *resources,
                                      const BkEntryRequest *request,
                                      char error[256]);
void bk_entry_assets_destroy(BkEntryAssets *assets);
const BkEntrySelection *bk_entry_assets_selection(const BkEntryAssets *assets);
const BkEntryRequest *bk_entry_assets_request(const BkEntryAssets *assets);
const BkRoute *bk_entry_assets_route(const BkEntryAssets *assets);
BkActorPose *bk_entry_assets_player(BkEntryAssets *assets);
BkActorPose *bk_entry_assets_actor(BkEntryAssets *assets);
/* Native actor+298 pre-placement head heights; no cached world refresh. */
int bk_entry_assets_base_heights(const BkEntryAssets *, float *player,
                                 float *npc);
/* Once per fresh entry, bind partial native CPU resets to loaded base heads.
 * Body placements/clip requests were made during asset construction. These
 * calls do not advance/publish poses. Preserve all unassigned session fields;
 * this is not a new-game/save initializer. NPC reset mutates its owned CKP. */
int bk_entry_assets_initialize_player(BkEntryAssets *, BkPlayerControl *,
                                      int32_t actions[21],
                                      uint8_t collected[BK_ITEM_TYPES],
                                      char error[256]);
int bk_entry_assets_initialize_npc(BkEntryAssets *, BkNpcSpatialState *,
                                   BkNpcFootstepActions *, BkNpcEntryRoute *,
                                   uint32_t route_start, float *vertical,
                                   char error[256]);
/*4bfdd6/4bf7db: replace player pose/material/shadow at the retained body
 * placement. Immutable model/XAN are reused; pose and shadow are fresh.
 * Destroy all borrowing GPU/forest objects BEFORE this call. Other entry
 * actors/route/camera/caches remain owned and unchanged. Failure before the
 * replacement preserves this entry; no state reset or audio is implicit. */
int bk_entry_assets_reload_player(BkEntryAssets *, BkResourceStore *,
                                  const float position[3], float yaw,
                                  char error[256]);
/*4e94e4 actor/CKP/track portion: advance exactly one area of the same group,
 * keeping player/NPC poses, clips, head caches, face/eye/shadows/materials.
 * Only route and camera model/XAN/controller are replaced. Caller must first
 * destroy forest/track borrowers and perform area-prompt resource releases.
 * No actor placement/publication, AI/animation advance or face warm-up occurs.
 * Background/props/items/dialogue remain the enclosing scene's responsibility.
 * Resource failure preserves entry and represented CPU outputs. */
void bk_entry_assets_unload_track(BkEntryAssets *);
int bk_entry_assets_advance_area(BkEntryAssets *, BkResourceStore *,
                                 const BkEntryRequest *, BkPlayerControl *,
                                 BkNpcSpatialState *, float *npc_vertical,
                                 BkPlayerView *, BkGameCameraState *,
                                 const BkNpcEntryRoute *,
                                 const BkNpcFootstepActions *, char error[256]);
/* Project the NPC's held head world frame. Caller supplies the previously
 * published camera view/lens/viewport; this does not publish fresh actors. */
int bk_entry_assets_project_actor_head(const BkEntryAssets *assets,
                                       BkScreenPoint *point,
                                       const float view[16],
                                       const BkCameraLens *lens,
                                       const BkViewport *viewport);
/* Bind movement/physics to real player active clip and held head cache.
 * Input's active_clip/base_head_height/cached_head_height are filled here.
 * Root placement commits with CPU state; no time advance or child publish.
 * Caller still supplies live action IDs, interaction outcome and camera data.
 */
int bk_entry_assets_step_player_spatial(BkEntryAssets *assets,
                                        BkPlayerSpatial *state,
                                        const BkCollision *collision,
                                        const BkPlayerSpatialInput *input,
                                        char error[256]);
/*4c1313: request idle with401b0a, save/collide/place player root, clear mode.
 * Retains vertical height and shadow position. No animation advance or child
 * publication. A later collision/root failure aborts this frame; the earlier
 * clip request is not rolled back. */
int bk_entry_assets_step_player_idle(BkEntryAssets *, BkPlayerControl *,
                                     int32_t idle_action, const BkCollision *,
                                     const BkPlayerSceneInput *,
                                     char error[256]);
/* Bind full ordinary/script control to the real player's retained 128-slot
 * timing and cached head. Group/area come from the selected entry. Applies
 * player root writes; optional shadow output stays explicitly caller-owned.
 * No animation advance/publish. A root failure aborts this frame. */
int bk_entry_assets_step_player_control(BkEntryAssets *assets,
                                        BkPlayerControl *state,
                                        const BkCollision *collision,
                                        const BkPlayerControlInput *input,
                                        BkPlayerControlEffects *effects,
                                        char error[256]);
typedef struct {
  int8_t camera_mode;
  uint8_t npc_hidden;
  float npc_vertical, seconds;
  uint32_t buttons; /* Cover-controller left/right aliases. */
  int32_t actions[21];
} BkEntryPlayerViewInput;
/* Phase1 camera after player control, before player/NPC animation. Metadata
 * stays caller-owned across entries (especially shared lean, initial10).
 * Binds real held heads/NPC root and camera XAN; carries wall distance/rays
 * between control and camera, and applies original root visibility writes.
 * Logical hidden remains caller-owned for later presentation. Caller uses
 * FOV1, as in4b8a89. Does not publish track or actor children. */
int bk_entry_assets_step_player_view(BkEntryAssets *assets,
                                     BkPlayerControl *player,
                                     BkPlayerView *view, uint8_t *hidden,
                                     const BkEntryPlayerViewInput *input,
                                     char error[256]);
/* Player4bf6xx uses the same independent kage_01 asset as the NPC.
 * Optional creation is explicit; control applies its ordinary Y+.1 placement.
 */
int bk_entry_assets_load_player_shadow(BkEntryAssets *, BkResourceStore *,
                                       char error[256]);
BkNpcShadow *bk_entry_assets_player_shadow(BkEntryAssets *);
const BkMaterialPose *bk_entry_assets_player_materials(const BkEntryAssets *);
typedef struct {
  float seconds;
  int32_t action, actions[21];
  int8_t camera_mode, interface_mode, interaction_mode;
  uint8_t hidden;
  const char *surface;
  int voice_present, voice_playing;
} BkEntryPlayerPresentation;
typedef int (*BkPlayerSoundSubmit)(void *, const BkPlayerEvents *,
                                   char error[256]);
/* Full4bfea9 ordering: alpha overwrite, body/shadow visibility, actual current
 * source4c155d events, synchronous sound consumer, body request/advance, shadow
 * full-speed advance. No child cache publication. The consumer is required:
 * an offline capture must identify itself; do not silently discard commands.
 * Basic validation is atomic, later failure aborts this frame, no rollback. */
int bk_entry_assets_step_player_presentation(
    BkEntryAssets *, BkPlayerEventState *, uint8_t *steps, size_t step_count,
    uint8_t *shared, size_t shared_count, const BkEntryPlayerPresentation *,
    BkPlayerSoundSubmit, void *sound_context, char error[256]);
BkFaceAssets *bk_entry_assets_face(BkEntryAssets *assets);
BkEyeAssets *bk_entry_assets_eyes(BkEntryAssets *assets);
/* Explicit graphics-settings0/1 path. Idempotent; a failed load leaves the
 * entry unchanged. Step/place/publish remain caller-controlled until the
 * complete NPC frame has been assembled. Settings2 has a different backend. */
int bk_entry_assets_load_mesh_shadow(BkEntryAssets *assets,
                                     BkResourceStore *resources,
                                     char error[256]);
BkNpcShadow *bk_entry_assets_shadow(BkEntryAssets *assets);
const BkMaterialPose *
bk_entry_assets_actor_materials(const BkEntryAssets *assets);
BkFollowCamera *bk_entry_assets_camera(BkEntryAssets *assets);
/* Assemble the 0x4fca12 head-stage inputs without advancing or publishing.
 * Actor kinds1/2 bind the separately named hara01/hara02 torso frame from
 * 0x4fbc90, not the immediate head parent. yaw is the post-route body yaw. */
int bk_entry_assets_head_input(const BkEntryAssets *assets, float yaw,
                               BkNpcHeadInput *input, char error[256]);
typedef struct {
  int32_t player_action, suppressed_actions[6], background_clip,
      short_range_action;
  float player_direction[3];
  int8_t interaction_df, interaction_e0;
  const char *excluded_surface;
} BkEntryNpcContext;
/* Bind the spatial stage to these real actors and apply its root placement.
 * State is supplied explicitly: this does not invent new-game/save timers,
 * route flags or cursor overrides. Uses live active clip, cached heads,
 * actual local torso/head matrices and the initial NPC head height.
 * State/shared/RNG/effects and pose commit together; no clip time is consumed.
 * Caller still must implement/execute subsequent action/media dispatch,
 * animation and publication before calling this a complete game frame. */
int bk_entry_assets_step_npc_spatial(
    BkEntryAssets *assets, BkNpcSpatialState *state,
    BkNpcInteractionState *interaction, uint32_t *random_state,
    const BkCollision *collision, const BkEntryNpcContext *context,
    float seconds, uint32_t now_ms, BkNpcSpatialEffects *effects,
    char error[256]);
/* Post-animation fade/material stage only, including original named marker
 * subtrees. Materials are per-entry instances; decoded assets stay immutable.
 * Hidden actors still execute this in 0x4fc36d; visibility is separate. */
int bk_entry_assets_step_npc_fade(BkEntryAssets *assets,
                                  BkNpcSpatialState *state, float seconds,
                                  char error[256]);
const BkModelMaterial *
bk_entry_assets_actor_material(const BkEntryAssets *assets, uint32_t index);
/* Pre-animation primary actor visibility/marker stage. Explicit interface
 * mode and current global phase are distinct from the NPC motion mode. */
int bk_entry_assets_step_npc_visibility(BkEntryAssets *assets,
                                        const BkNpcSpatialState *state,
                                        int8_t interface_mode, int8_t phase,
                                        char error[256]);
typedef struct {
  float seconds, voice_level;
  int8_t interface_mode, phase;
  uint32_t timestamp_ms, request_clock_ms, mouth_clock_ms, blink_clock_ms;
} BkEntryNpcPresentation;
/* Complete 4fc36d presentation after spatial/action dispatch. The voice
 * envelope and four clock reads are supplied by the platform/audio caller;
 * this does not simulate an audio backend. Face state must be initialized.
 * Order: visibility, body request/half-speed advance, expression0/mouth/blink,
 * optional full-speed shadow, fade. Hidden roots pause only their clocks.
 * Does not place the shadow or publish any child world caches. Invalid basic
 * inputs fail before mutation; any later stage failure is fatal to this frame
 * (do not publish/draw it), not an all-object transaction. */
int bk_entry_assets_step_npc_presentation(BkEntryAssets *assets,
                                          BkNpcSpatialState *state,
                                          BkFaceState *face,
                                          uint32_t *random_state,
                                          const BkEntryNpcPresentation *input,
                                          char error[256]);
typedef struct {
  BkNpcFootsteps footsteps;
  BkSpatialAudio audio;
} BkEntryNpcFootsteps;
/* Pre-presentation 4fd796 CPU preparation: reads the current active source
 * tick, live NPC/player placement and spatial ground name. Copies the shared
 * latch prefix transactionally; does not advance either actor or play audio.
 * Caller consumes footsteps in order, then places its optional mesh shadow
 * (4fcdd0), before running presentation. Live action bindings are explicit. */
int bk_entry_assets_prepare_npc_footsteps(const BkEntryAssets *assets,
                                          const BkNpcSpatialState *state,
                                          const BkNpcFootstepActions *actions,
                                          int32_t effect_volume,
                                          uint8_t *latches, size_t latch_count,
                                          BkEntryNpcFootsteps *out,
                                          char error[256]);
/* Camera dispatch only, after caller's actor updates and before publication.
 * Caller supplies original game state and obstacle correction; phase1 requires
 * the separate player-view entry point with explicit player/control context.
 * All failures preserve camera pose and game camera state. No AI/time advance
 * for actors occurs. */
int bk_entry_assets_step_camera(BkEntryAssets *assets, BkGameCameraState *state,
                                const float *correction, float seconds,
                                char error[256]);
/* Same dispatch, additionally carries4bdc12's +43c output into the shared
 * player scene camera_distance. Only FOLLOW writes it; handover/hold retain.
 * Pass that same value to phase1 collision/player view, without resetting. */
int bk_entry_assets_step_camera_distance(BkEntryAssets *, BkGameCameraState *,
                                         const float *correction, float seconds,
                                         float *distance, char error[256]);
#endif
