#ifndef BK_WORLD_FACE_CONTROLLER_H
#define BK_WORLD_FACE_CONTROLLER_H
#include <stdint.h>
/* CPU controller for 0x410fd8/0x411985/0x4110ef. Immutable mesh bindings
 * belong to the scene; this object stores the native animation scalars. */
typedef struct {
  float eye, mouth;
  int32_t blink_phase;
  uint32_t deadline_ms, blink_duration_ms;
  int32_t cycles, rapid_count, expression, eye_mode;
  float eye_max, eye_min;
  int32_t mouth_mode;
  float mouth_max, mouth_min;
  int32_t previous_expression;
  float transition;
  uint32_t transition_start_ms;
  float transition_rate, previous_eye, previous_mouth;
  int32_t dirty;
  uint32_t eye_count, mouth_count;
} BkFaceState;
typedef struct {
  uint32_t group, index; /* group0 eyes, group1 mouth; index in bound array */
  int32_t blend;
  float from, to, weight;
} BkFaceCommand;
typedef struct {
  uint32_t count;
  BkFaceCommand commands[16];
} BkFaceCommands;
/* Native constructor scalar defaults; at most8 meshes per group. */
int bk_face_init(BkFaceState *state, unsigned eyes, unsigned mouths,
                  char error[256]);
int bk_face_request(BkFaceState *state, int32_t expression,
                     uint32_t clock_ms, char error[256]);
/* Native range/transition setters (0x411de1/0x411ea7/0x411f5d). Eye range
 * changes can restart the blink cycle and consume the shared RNG. */
int bk_face_eye_range(BkFaceState *state, float minimum, float maximum,
                       uint32_t *random_state, char error[256]);
int bk_face_mouth_range(BkFaceState *state, float minimum, float maximum,
                         char error[256]);
int bk_face_transition_seconds(BkFaceState *state, float seconds,
                                char error[256]);
/* Morph commands preserve native order and per-group duplicate submissions.
 * A plain command clamps the relative value to0..9, then adds expression*10;
 * a blend uses unclamped values. Commands still need real MORP evaluation.
 * Mouth uses its own clock read. Blink receives both the caller's earlier
 * timestamp (deadline test) and its own clock read (expression transition).
 * RNG is the same explicit shared state used by AI. Failure changes nothing. */
int bk_face_mouth(BkFaceState *state, float level, uint32_t clock_ms,
                   BkFaceCommands *out, char error[256]);
int bk_face_blink(BkFaceState *state, uint32_t timestamp_ms,
                   uint32_t clock_ms, uint32_t *random_state,
                   BkFaceCommands *out, char error[256]);
#endif
