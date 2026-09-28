#ifndef BK_SCENE_DIALOGUE_SESSION_H
#define BK_SCENE_DIALOGUE_SESSION_H
#include "scene/curtain_render.h"
#include "scene/dialogue_audio.h"
#include "scene/dialogue_entry.h"
#include "scene/dialogue_render.h"
#include "scene/dialogue_ui_render.h"
typedef struct BkDialogueSession BkDialogueSession;
/* Retain scalar state across dialogue entries. Script bytes belong to the
 * active session and are closed on release/destroy. Timer.duration aliases
 * ui.text_parameter in the original; the adapter synchronizes that write.
 * Independently shared camera/common/RNG/envelope live in the outer process.
 */
typedef struct {
  BkDialogueAssets script;
  BkDialogueActorState actor;
  BkDialogueUi ui;
  BkTextFlow text;
  BkDialogueBackdrop backdrop;
  BkTimer timer;
  BkDialogueMusic music;
  uint8_t phase;
} BkDialogueSessionState;
typedef struct {
  BkDialogueSessionState *state;
  BkMenuCamera *camera;
  BkCommonHudState *common;
  BkCurtainRender *common_render;
  BkFadeSprite *backdrop_curtain;
  uint8_t *backdrop_curtain_wanted;
  BkDialogueResult *result;
  BkVoiceEnvelope *envelope;
  uint32_t *random;
  void *context;
  int (*unlock)(void *, unsigned group, char error[256]);
  int (*schedule)(void *, uint8_t target, uint8_t mode, char error[256]);
  unsigned group, width, height, music_voice, speech_voice;
  int32_t area;
  uint8_t previous, response;
  uint32_t clocks[4];
  BkFog fog;
} BkDialogueSessionConfig;
/* Owns real actor/forest/lights, background, text and audio. No synthetic
 * next-scene success. Creation failure is terminal; retained state may have
 * received already completed initialization. Call after previous GPU submit.
 * New-game previous38 is supported; end-dialogue previous16 requires actual
 * persistence via unlock. The outer flow owns scene transition/loading.
 */
BkDialogueSession *bk_dialogue_session_create(BkRenderer *, BkResourceStore *,
                                              BkAudio *,
                                              const BkDialogueSessionConfig *,
                                              char error[256]);
void bk_dialogue_session_destroy(BkDialogueSession *);
typedef struct {
  float seconds;
  uint32_t timestamp_ms, timer_clock_ms, face_clocks[3];
  int advance;
  int32_t music_volume, voice_volume;
} BkDialogueSessionInput;
/* Actor update -> backdrop -> published3D snapshot -> UI/media/curtain.
 * Transition is recorded during UI and dispatched after the snapshot is
 * presented. A failed frame cannot be resumed. Exactly one pending step.
 */
int bk_dialogue_session_step(BkDialogueSession *,
                             const BkDialogueSessionInput *, char error[256]);
int bk_dialogue_session_draw(BkDialogueSession *, char error[256]);
int bk_dialogue_session_after_present(BkDialogueSession *, char error[256]);
int bk_dialogue_session_stop(BkDialogueSession *, char error[256]);
int bk_dialogue_session_released(const BkDialogueSession *);
BkDialogueWorld *bk_dialogue_session_world(BkDialogueSession *);
#endif
