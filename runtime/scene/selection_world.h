#ifndef BK_SCENE_SELECTION_WORLD_H
#define BK_SCENE_SELECTION_WORLD_H
#include "scene/lighting_assets.h"
#include "scene/menu_camera_assets.h"
#include "scene/selection_actor_assets.h"
typedef struct BkSelectionWorld BkSelectionWorld;
enum {
  BK_SELECTION_PRIMARY,
  BK_SELECTION_SECONDARY,
  BK_SELECTION_STAGE,
  BK_SELECTION_BODY,
  BK_SELECTION_OBJECTS
};
/* CPU owner for retail502480(special0): retained_group selects only the
 * secondary camera. The initial body is ALWAYS group0. Alternate is the
 * already-resolved5073de result; it must not be sampled again here.
 * Owns tracks/stage/body/lights/forest and borrows the retained camera state.
 * Face warm-up shares RNG. Failure preserves caller camera/RNG.
 * Alternate61 still requires the outer owner to initialize its real movie
 * before presenting; this does not create video, GPU or audio devices. */
BkSelectionWorld *
bk_selection_world_create(BkResourceStore *, unsigned retained_group,
                          uint8_t alternate, float loading_seconds,
                          BkMenuCamera *, const uint32_t clocks[4],
                          uint32_t *random, char error[256]);
void bk_selection_world_destroy(BkSelectionWorld *);
BkActorPose *bk_selection_world_pose(BkSelectionWorld *, unsigned object);
BkSelectionActorAssets *bk_selection_world_body(BkSelectionWorld *);
BkActorForest *bk_selection_world_forest(BkSelectionWorld *);
BkSceneLighting *bk_selection_world_lighting(BkSelectionWorld *);
uint32_t bk_selection_world_root(const BkSelectionWorld *, unsigned object);
uint32_t bk_selection_world_focus(const BkSelectionWorld *);
/* CPU part of505b87..50638e. Track/stage timelines and camera are retained;
 * attaching the replacement refreshes every connected published cache.
 * On success transfers OLD body ownership to *retired. Its GPU/voice loans
 * must be retired before destroying it. Previous forest/lights are invalid.
 * Decode failure preserves current owners/RNG. A topology failure terminates
 * this frame and may have published retained pose caches, as attachment does.
 * Movie/voice release+initialization remain ordered outer-owner operations. */
int bk_selection_world_replace(BkSelectionWorld *, unsigned group,
                               uint8_t alternate, const uint32_t clocks[4],
                               uint32_t *random,
                               BkSelectionActorAssets **retired,
                               char error[256]);
typedef struct {
  void *context;
  /* Required iff alternate61, before envelope/body. Must update real media. */
  int (*movie_step)(void *, char error[256]);
  /* Required iff voice_active; consumes actual audible cursor/shared envelope.
   * The original733700 game step is passed, independently of audio time. */
  int (*voice_level)(void *, float seconds, float *out, char error[256]);
} BkSelectionWorldOps;
typedef struct {
  unsigned selected, camera_mode, buttons;
  uint8_t voice_active;
  float seconds, motion[2];
  uint32_t timestamp, face_clocks[3];
} BkSelectionWorldInput;
/* Entire retail51ac5d order: camera/global pre-refresh -> movie -> envelope
 * -> body(.4*seconds)/face -> stage(seconds). No final world publication.
 * special1's4bb9de camera is a separate, currently unsupported flow.
 * Later failures preserve already-applied side effects and abort the frame. */
int bk_selection_world_step(BkSelectionWorld *, const BkSelectionWorldInput *,
                            const BkSelectionWorldOps *, uint32_t *random,
                            char error[256]);
/* flow38/mode1 root dispatch: stage slot0, body slot4. Caller executes each
 * ordered light command and forest draw/flush; no drawing is simulated here. */
int bk_selection_world_pass(BkSelectionWorld *, BkLightingPass *,
                            char error[256]);
#endif
