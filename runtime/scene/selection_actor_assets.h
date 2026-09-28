#ifndef BK_SCENE_SELECTION_ACTOR_ASSETS_H
#define BK_SCENE_SELECTION_ACTOR_ASSETS_H
#include "game/selection_actor.h"
#include "media/audio.h"
#include "scene/eye_assets.h"
#include "scene/face_assets.h"
typedef struct BkSelectionActorAssets BkSelectionActorAssets;
/* Body/face/eye/voice portion of502480 and505b87. Loader selects slot0,
 * retains authored root, performs real face warm-up, and finds focus by the
 * original per-variant table. No playback time is consumed by creation.
 * For alternate61, this is CPU resource assembly ONLY: the outer scene MUST
 * create/update4e5f89's bk3_18/poi.avi replacement D_moza.bmp before exposing
 * that actor. This interface does not implement the video service.
 * Failed creation leaves caller RNG unchanged. Store needs bk3_01,bk3_06,faces.
 * Borrowed forest/render/audio objects must be retired before destruction. */
BkSelectionActorAssets *
bk_selection_actor_assets_create(BkResourceStore *, unsigned group,
                                 uint8_t alternate, const uint32_t clocks[4],
                                 uint32_t *random, char error[256]);
void bk_selection_actor_assets_destroy(BkSelectionActorAssets *);
BkActorPose *bk_selection_actor_assets_pose(BkSelectionActorAssets *);
BkFaceAssets *bk_selection_actor_assets_face(BkSelectionActorAssets *);
BkEyeAssets *bk_selection_actor_assets_eyes(BkSelectionActorAssets *);
BkAudioClip *bk_selection_actor_assets_voice(BkSelectionActorAssets *);
const BkFaceState *
bk_selection_actor_assets_face_state(const BkSelectionActorAssets *);
uint32_t bk_selection_actor_assets_root(const BkSelectionActorAssets *);
uint32_t bk_selection_actor_assets_focus(const BkSelectionActorAssets *);
int bk_selection_actor_assets_needs_movie(const BkSelectionActorAssets *);
/* Body/face tail of51ac5d only; camera/movie/envelope occur earlier, stage
 * animation occurs later. Original body time is .4*seconds (NOT game NPC .5).
 * Old active source tick controls eyes. No world publication. Failed later
 * components terminate after preceding side effects, not rollback. */
int bk_selection_actor_assets_step(BkSelectionActorAssets *, unsigned selected,
                                   float seconds, float mouth_level,
                                   const float camera_world[16],
                                   uint32_t timestamp, const uint32_t clocks[3],
                                   uint32_t *random, char error[256]);
#endif
