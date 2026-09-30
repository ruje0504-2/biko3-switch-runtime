#ifndef BK_SCENE_SPECIAL_WORLD_H
#define BK_SCENE_SPECIAL_WORLD_H
#include "scene/face_assets.h"
#include "scene/eye_assets.h"
#include "scene/lighting_assets.h"
#include "scene/special_camera_assets.h"
typedef struct BkSpecialWorld BkSpecialWorld;
enum { BK_SPECIAL_WORLD_PRIMARY, BK_SPECIAL_WORLD_SECONDARY,
       BK_SPECIAL_WORLD_BODY, BK_SPECIAL_WORLD_OBJECTS };
/* CPU resources in4e29b0: bk3_14 body, ordinary4f2971 face/eyes from
 * bk3_01 + faces, body lights, then flow48 camera tracks from bk3_04.
 * Borrow camera/transitions/shared RNG and source-tick latch array until
 * destruction. No reset of shared timers, latches or process phase here.
 * Failure preserves caller camera/RNG. Audio/video/UI belong to the outer
 * loader, which must finish them before exposing a production scene. */
BkSpecialWorld *bk_special_world_create(BkResourceStore *, unsigned group,
    float loading_seconds, BkMenuCamera *, BkEndingCameraTransitions *,
    const uint32_t face_clocks[4], uint32_t *random,
    uint8_t *latches, size_t latch_count, char error[256]);
void bk_special_world_destroy(BkSpecialWorld *);
BkActorPose *bk_special_world_pose(BkSpecialWorld *, unsigned object);
BkActorForest *bk_special_world_forest(BkSpecialWorld *);
BkFaceAssets *bk_special_world_face(BkSpecialWorld *);
BkEyeAssets *bk_special_world_eyes(BkSpecialWorld *);
const BkFaceState *bk_special_world_face_state(const BkSpecialWorld *);
BkSceneLighting *bk_special_world_lighting(BkSpecialWorld *);
BkEndingCameraPresets *bk_special_world_presets(BkSpecialWorld *);
uint32_t bk_special_world_root(const BkSpecialWorld *, unsigned object);
uint32_t bk_special_world_focus(const BkSpecialWorld *);
/* Body model frame index, not a forest node. */
uint32_t bk_special_world_back(const BkSpecialWorld *);
int bk_special_world_needs_movie(const BkSpecialWorld *);
/* Remaining real service boundaries. present only receives MOVIE/EFFECT0..3.
 * Clocks must be separate original GetTickCount/timeGetTime queries, including
 * request/mouth/blink's internal reads; no once-per-frame substitute here. */
typedef struct {
  void *context;
  int (*key)(void *, uint32_t, uint8_t *, char[256]);
  int (*present)(void *, BkSpecialEventObject, int *, char[256]);
  int (*audio)(void *, const BkSpecialEventAudioCall *, char[256]);
  int (*movie)(void *, char[256]);
  int (*clock)(void *, int timer, uint32_t *, char[256]);
  int (*level)(void *, unsigned effect, float seconds, float *, char[256]);
} BkSpecialWorldServices;
/* Complete51b647 with actual body/face/eyes/cameras and shared cue/RNG state.
 * Camera runs BEFORE body advancement. OPEN reads live bindings.seconds,
 * including when paused. No final draw/publication; Back recursively disables
 * geometry submissions while retaining world updates (423b01, not423a99).
 * Later failure retains the executed prefix and aborts frame.
 * Services cannot destroy/reenter this owner while the call is active. */
int bk_special_world_step(BkSpecialWorld *, const BkSpecialEventBindings *,
    const float motion[2], unsigned buttons, const BkSpecialWorldServices *,
    char error[256]);
/* flow48 root721b28/slot4, native mode10 for group4, mode1 otherwise.
 * Caller executes ordered commands/draws; nothing renders in this module. */
int bk_special_world_pass(BkSpecialWorld *, unsigned group, BkLightingPass *,
                           char error[256]);
#endif
