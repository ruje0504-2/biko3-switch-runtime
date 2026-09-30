#ifndef BK_SCENE_SPECIAL_SESSION_H
#define BK_SCENE_SPECIAL_SESSION_H
#include "core/input.h"
#include "scene/special_audio.h"
#include "scene/special_capture.h"
#include "scene/special_render.h"
#include "scene/special_ui_render.h"
#include "scene/system_audio.h"
typedef struct BkSpecialSession BkSpecialSession;
/* Initialize once with zero process/CRT values. Shared ending/menu aliases
 * remain explicit borrowed config fields rather than new copies here. */
typedef struct {
  BkSpecialEventState event;
  BkSpecialUi ui;
  BkSpecialUiControl control;
  int32_t phase; /*BFBBA0, distinct from ending721e00*/
  int8_t music_wanted; /*72221c, set1 by50d4fa, retained on release*/
  uint8_t effect_loops[4];
  float effect_positions[4][4];
} BkSpecialProcess;
typedef struct {
  BkSpecialProcess *process;
  BkCommonHudState *common;
  BkCurtainRender *curtain;
  BkMenuCamera *camera;
  BkEndingCameraTransitions *transitions;
  BkVoiceEnvelope *envelope;
  int32_t *group, *camera_clip, *camera_mode, *photo_count, *photos;
  uint32_t *random;
  const uint32_t *album_group;
  uint8_t *latches, *visibility, *hover;
  size_t latch_count;
  const int8_t *paused;
  const uint8_t *previous_flow;
  BkScreenshot *screenshot;
  BkSystemAudio *sounds[8];
  BkViewport viewport;
  BkFog fog;
  unsigned first_voice, speech_voice;
  int32_t music_volume, voice_volume, effect_volume;
  float loading_seconds;
  void *context;
  /*0=timeGetTime,1=GetTickCount,2=CRT process elapsed ms (signed bit pattern).*/
  int (*clock)(void *, int timer, uint32_t *, char[256]);
  int (*inventory)(void *, int32_t counts[5], char[256]);
  int (*release_speech)(void *, char[256]);
  int (*schedule)(void *, uint8_t target, uint8_t mode, char[256]);
} BkSpecialSessionConfig;
/*4e29b0 real retail special0: inventory/UI/body/media/lights/camera/warp.
 * No frame is simulated by construction. First draw needs a real step.
 * All config pointers/services outlive the session. Failure retains native
 * scalar prefixes; any constructed media is stopped before CPU cleanup. */
BkSpecialSession *bk_special_session_create(BkRenderer *, BkResourceStore *,
    BkAudio *, const BkSpecialSessionConfig *, char[256]);
/* Logical4e42e4, outside the active GPU frame. Retains the last prepared
 * draw snapshot until destruction at the next actual tick. Idempotent stop,
 * and destruction of a stopped old owner cannot change a replacement. */
int bk_special_session_stop(BkSpecialSession *, char[256]);
void bk_special_session_destroy(BkSpecialSession *);
int bk_special_session_step(BkSpecialSession *, float seconds,
                             const BkInput *, char[256]);
/*World draws first; the first real draw executes51b617 including capture,
 * then snapshots UI. Repeated draws do no CPU/input/capture/audio work.*/
int bk_special_session_draw(BkSpecialSession *, char[256]);
int bk_special_session_after_present(BkSpecialSession *, char[256]);
BkSpecialWorld *bk_special_session_world(BkSpecialSession *);
const BkVirtualPointer *bk_special_session_pointer(const BkSpecialSession *);
#endif
