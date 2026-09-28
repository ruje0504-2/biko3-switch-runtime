#ifndef BK_SCENE_DIALOGUE_ACTOR_ASSETS_H
#define BK_SCENE_DIALOGUE_ACTOR_ASSETS_H
#include "core/timer.h"
#include "game/dialogue_actor.h"
#include "resource/dialogue.h"
#include "scene/eye_assets.h"
#include "scene/face_assets.h"
typedef struct BkDialogueActorAssets BkDialogueActorAssets;
/*4ef6a0 body/face subset: hNN_70, slot0 instant, real FAM/hNN_20 source,
 * warm-up, then group-specific root placement. Owns CPU resources only.
 * RNG commits on successful construction; shared scalar state is separate. */
BkDialogueActorAssets *bk_dialogue_actor_assets_create(BkResourceStore *,
                                                       unsigned group,
                                                       const uint32_t clocks[4],
                                                       uint32_t *random,
                                                       char error[256]);
void bk_dialogue_actor_assets_destroy(BkDialogueActorAssets *);
BkActorPose *bk_dialogue_actor_assets_pose(BkDialogueActorAssets *);
BkFaceAssets *bk_dialogue_actor_assets_face(BkDialogueActorAssets *);
BkEyeAssets *bk_dialogue_actor_assets_eyes(BkDialogueActorAssets *);
const BkFaceState *
bk_dialogue_actor_assets_face_state(const BkDialogueActorAssets *);
uint32_t bk_dialogue_actor_assets_root(const BkDialogueActorAssets *);
typedef struct {
  float game_seconds, mouth_level;
  uint32_t timestamp_ms, timer_clock_ms, face_clocks[3];
  float camera_world[16];
} BkDialogueActorInput;
/*4f11f2 after the caller has applied4bd641's fixed camera world. Rules,
 * speaker-gated real envelope input, shared timer, request/completion, eyes,
 * face, visibility, then .2*game_seconds animation. No world publication.
 * NULL actor preserves the native missing-body branch (rules only).
 * Mouth input must be from the current dialogue sound consumption cursor;
 * this CPU adapter does not claim playback. Mid-frame failure is terminal,
 * not a whole-frame rollback. Invalid input is rejected before mutation. */
int bk_dialogue_actor_assets_step(BkDialogueActorAssets *,
                                  BkDialogueActorState *, BkDialogue *,
                                  uint8_t *phase, BkTimer *,
                                  const BkDialogueActorInput *,
                                  uint32_t *random, char error[256]);
#endif
