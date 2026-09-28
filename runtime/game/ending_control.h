#ifndef BK_GAME_ENDING_CONTROL_H
#define BK_GAME_ENDING_CONTROL_H
#include "game/ending_frame.h"
#include "world/ending_camera.h"
#define BK_ENDING_CONTROL_RECTS 13
/* Native inclusive bounds, in the order4d7ac4 tests them. The actual ending
 * UI supplies these rectangles; control does not invent layout or scaling. */
typedef struct {
  float x, y, width, height;
} BkEndingControlRect;
typedef struct {
  int32_t hover, hover_armed, previous_hover; /*70c8bc,709f18,709f20*/
  int32_t mode_721ec4, target_choice, previous_phase, state_721eec;
  float targets[3][3];     /*70c8d8*/
  float saved_camera[16];  /*+4e4, separate from the +4a4 blend source*/
  uint8_t variant;         /*721b3d*/
  uint8_t toggles[8];      /*7220f8..ff; nonboolean values must be retained*/
  uint8_t pause_selection; /*beeb7d*/
  uint8_t pause_flags[6];  /*7392cb/739437/7395a3/73970f/73987b/7399e7*/
} BkEndingControlState;
typedef struct {
  BkEndingFrameState *frame;
  BkMenuCamera *camera;
  BkEndingCameraPresets *presets;
  /* Live old published positions:721ef4,719b40,721f08,721f28.
   * Required only when a control actually reads them. */
  const float *nodes[4];
  const BkEndingControlRect *rects;
  const float *scale;           /*721ad0*/
  const int32_t *effect_volume; /*be9a10*/
} BkEndingControlBindings;
typedef struct {
  void *context;
  /*4b76c2: code0/Z/0x33450; modes1(edge) and2(held). Uses low AL. */
  int (*key)(void *, unsigned code, unsigned mode, uint32_t *result,
             char error[256]);
  /*4ad2bf restart, loop0, system slots0(confirm),5(reject),3(hover). */
  int (*sound)(void *, unsigned slot, int32_t volume, char error[256]);
  /*Actual495d92 transition. Full EAX result; may update live frame fields.
   * Missing service is an error when this button requires the transition. */
  int (*auxiliary)(void *, int32_t proposed, int32_t *result, char error[256]);
  int (*warp)(void *, float x, float y, char error[256]);
} BkEndingControlOps;
/* Complete4d7ac4..4d901b. Captured pointer words9/10 remain unchanged after
 * warp. Every hit region is processed independently, preserving short-circuit
 * key order and repeated sounds. Later failure retains the ordered prefix.
 * Does not own UI sprites, play media without services, or supply495d92. */
int bk_ending_control_step(BkEndingControlState *,
                           const BkEndingControlBindings *,
                           const BkEndingFrameInput *,
                           const BkEndingControlOps *, char error[256]);
#endif
