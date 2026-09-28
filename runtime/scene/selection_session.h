#ifndef BK_SCENE_SELECTION_SESSION_H
#define BK_SCENE_SELECTION_SESSION_H
#include "scene/selection_audio.h"
#include "scene/selection_render.h"
#include "scene/selection_ui_render.h"
#include "scene/system_audio.h"
typedef struct BkSelectionSession BkSelectionSession;
typedef struct {
  void *context;
  int (*position)(void *, float out[2], char error[256]);
  int (*motion)(void *, float out[2], char error[256]);
  int (*warp)(void *, float x, float y, char error[256]);
} BkSelectionPointer;
typedef struct {
  BkSelectionUi *ui;
  BkSelectionBindings bindings;
  BkMenuCamera *camera;
  BkVoiceEnvelope *envelope;
  uint32_t *random;
  const uint8_t (*unlocked)[8]; /* five B54738 rows, not route progress */
  BkSystemAudio *sounds[8];     /* borrowed process-owned slots1..4 */
  BkSelectionPointer pointer;
  unsigned music_voice, speech_voice, width, height;
  int32_t music_volume, voice_volume;
  float loading_seconds;
  uint32_t clocks[4];
  int32_t movie_clock_ms;
  BkFog fog; /* effective retained device state from outer scene entry */
} BkSelectionSessionConfig;
/* Owns retail selection UI/world/GPU/audio. All config pointers/services are
 * borrowed until destruction. Full special0 normal60/alternate61 path with
 * real poi.avi; special1 remains an explicit failure, never substituted.
 * Movie pixel/device policy is documented in avi_texture.h.
 * Constructor loads only; first draw requires an actual step. Failure after
 * loading has begun is fatal, and retained camera/UI may already be changed.
 * Audio slots must be distinct and reserved by the enclosing process. */
BkSelectionSession *
bk_selection_session_create(BkRenderer *, BkResourceStore *, BkAudio *,
                            const BkSelectionSessionConfig *, char error[256]);
void bk_selection_session_destroy(BkSelectionSession *);
typedef struct {
  BkSelectionInput ui;
  unsigned camera_buttons; /* held mouse0/1: rotate/radius-height */
  uint32_t face_clocks[3], reload_clocks[4];
  int32_t movie_clock_ms, movie_restart_clock_ms, reload_movie_clock_ms;
} BkSelectionSessionInput;
/*51ac5d -> mode1 world snapshot ->504335 UI view ->504802 control. UI uses
 * the immutable pre-control frame. Replacement prepares new owners, retains
 * the old draw snapshot until after_present, then collects on the next step.
 * Exactly one pending step; do not continue a partially failed frame. */
int bk_selection_session_step(BkSelectionSession *,
                              const BkSelectionSessionInput *, char error[256]);
/* Active GPU frame, caller sets the4:3 logical viewport. Re-draw is inert. */
int bk_selection_session_draw(BkSelectionSession *, char error[256]);
/* After renderer.end, before another step/destruction. No loading callback
 * or target-scene success is invented here. Outer flow50 owns target loading.
 */
int bk_selection_session_after_present(BkSelectionSession *, char error[256]);
int bk_selection_session_released(const BkSelectionSession *);
BkSelectionWorld *bk_selection_session_world(BkSelectionSession *);
#endif
