#ifndef BK_GAME_SPECIAL_EVENT_H
#define BK_GAME_SPECIAL_EVENT_H
#include "core/timer.h"
#include "world/voice_envelope.h"

/* flow48 is independent of dialogue8 and ending10. Only these private
 * process fields are new; all other globals are borrowed live aliases. */
typedef struct {
  BkTimer sequence_timer; /*71ad90*/
  float face_target;     /*71ada0*/
  BkTimer face_timer;     /*71ada8*/
  int8_t sequence;       /*71adbc*/
} BkSpecialEventState;
typedef struct {
  BkSpecialEventState *state;
  int32_t *group, *phase, *camera_clip, *camera_mode;
  const float *seconds; /*733700, read at each original service boundary*/
  const int8_t *paused, *music_wanted, *packed;
  const uint8_t *visibility;
  uint8_t *effect_loop; /*72257d: first effect's stored loop flag*/
  const int32_t *effect_volume;
  const float *listener; /*71b358, four floats*/
  float *effect_positions[4]; /*722564 + i*120, four floats each*/
  BkVoiceEnvelope *envelope; /*same708878/7c owner as gameplay/menu*/
} BkSpecialEventBindings;
typedef enum {
  BK_SPECIAL_CAMERA_OPEN, BK_SPECIAL_CAMERA_TRANSITION,
  BK_SPECIAL_CAMERA_ORBIT, BK_SPECIAL_CAMERA_TRACK
} BkSpecialEventCamera;
typedef enum {
  BK_SPECIAL_PRIMARY_FACE, BK_SPECIAL_CAMERA_PRIMARY,
  BK_SPECIAL_CAMERA_SECONDARY, BK_SPECIAL_MOVIE,
  BK_SPECIAL_EFFECT0, BK_SPECIAL_EFFECT1,
  BK_SPECIAL_EFFECT2, BK_SPECIAL_EFFECT3
} BkSpecialEventObject;
typedef enum {
  BK_SPECIAL_MUSIC_FADE, BK_SPECIAL_EFFECT_LOAD,
  BK_SPECIAL_EFFECT_PLAY, BK_SPECIAL_EFFECT_DIRECT_PLAY,
  BK_SPECIAL_EFFECT_SPATIAL
} BkSpecialEventAudioOperation;
typedef struct {
  BkSpecialEventAudioOperation operation;
  unsigned slot;
  const char *pack, *name;
  int32_t volume;
  uint32_t flags;
  float amount, source[4], listener[4];
} BkSpecialEventAudioCall;
typedef enum {
  BK_SPECIAL_FACE_MODE, BK_SPECIAL_FACE_RANGE, BK_SPECIAL_FACE_EXPRESSION,
  BK_SPECIAL_FACE_MOUTH, BK_SPECIAL_FACE_BLINK
} BkSpecialEventFace;
typedef struct {
  void *context;
  /*seconds exposes the original implicit733700 input for all controllers;
   * OPEN alone substitutes zero when paused. clip applies to OPEN/TRANSITION,
   * center to TRANSITION/ORBIT; done is consumed only for TRANSITION.*/
  int (*camera)(void *, BkSpecialEventCamera, int32_t clip,
                 const float center[3], float seconds, uint8_t *done, char[256]);
  /*Read-only queries; camera0 is body, camera1 is secondary camera XAN.
   * Read the live active descriptor, never advance or publish here.*/
  int (*timing)(void *, unsigned camera, float *source, float *end, char[256]);
  int (*key)(void *, uint32_t code, uint8_t *pressed, char[256]);
  int (*present)(void *, BkSpecialEventObject, int *, char[256]);
  /*422c49 or4241e3 relative to the actual645600 reference. object0/1
   * selects the primary/secondary camera root. Values are XYZ or axis.*/
  int (*place)(void *, unsigned object, const float values[3], float degrees,
                char[256]);
  int (*audio)(void *, const BkSpecialEventAudioCall *, char[256]);
  int (*cue)(void *, int32_t tick, uint8_t *triggered, char[256]); /*4afe00(...,0)*/
  int (*movie)(void *, char[256]); /*521ed2, only when71adc0 exists*/
  /*timer=1 queries GetTickCount; timer=0 queries timeGetTime.*/
  int (*clock)(void *, int timer, uint32_t *, char[256]);
  int (*random)(void *, int32_t *, char[256]); /*shared CRT rand, nonnegative*/
  int (*level)(void *, unsigned effect, float seconds, float *, char[256]);
  int (*request)(void *, int32_t clip, char[256]); /*401b0a fixed10tick*/
  int (*advance)(void *, float seconds, char[256]); /*4026fe primary*/
  int (*gaze)(void *, float pitch, float yaw, char[256]); /*4f3bb9(1,...)*/
  int (*face)(void *, BkSpecialEventFace, int32_t value, float amount,
               uint32_t timestamp, char[256]);
  int (*hide)(void *, uint32_t value, char[256]); /*423b01 on721b38*/
} BkSpecialEventOps;
/*PE initialization only. Loader/release reset sequence but retain timers,
 * face target and phase. Do not reinitialize this on every resource load.*/
BkSpecialEventState bk_special_event_initial(void);
/*Complete51b647 and helpers4e5c91/4e5901/4e5dc0/4e6154/4e620a.
 * Retains the executed prefix on failure; callbacks must perform real
 * services and may change aliases. No implicit actor/world publication.
 * This is not the4e29b0 loader or4e4472 drawing/input controller.*/
int bk_special_event_step(const BkSpecialEventBindings *,
                           const BkSpecialEventOps *, char error[256]);
#endif
