#ifndef BK_SCENE_SPECIAL_UI_H
#define BK_SCENE_SPECIAL_UI_H
#include "scene/common_hud.h"
#include "scene/ending_ui.h"
#include "game/special_event.h"
#include "world/ending_camera.h"
#define BK_SPECIAL_UI_SPRITES 30 /*17 controls followed by13 counter images*/
typedef struct {
  BkEndingUiSprite sprites[BK_SPECIAL_UI_SPRITES];
  uint32_t loaded;
  uint8_t cursor_wanted; /*7341bf, separately retained from fade.stage*/
} BkSpecialUi;
typedef struct {
  int32_t row; /*71adb8*/
  int8_t open; /*71ad9c*/
} BkSpecialUiControl;
typedef struct {
  BkCommonHudState *common;
  BkSpecialUiControl *control;
  BkMenuCamera *camera;
  BkEndingCameraPresets *presets;
  int32_t *phase, *camera_clip, *camera_mode;
  const int32_t *photo_count;
  const int8_t *special;
  const uint8_t *previous_flow;
  uint8_t *visibility, *hover_latched;
} BkSpecialUiBindings;
typedef enum {
  BK_SPECIAL_CAPTURE_FLUSH, /*49d0eb, before counter/UI*/
  BK_SPECIAL_CAPTURE_REQUEST, /*49d085*/
  BK_SPECIAL_CAPTURE_CONFIGURE /*4affb8, AFTER the request*/
} BkSpecialCapture;
typedef struct {
  void *context;
  int (*image)(void *, unsigned slot, const char *name, char[256]);
  int (*capture)(void *, BkSpecialCapture, char[256]);
  int (*position)(void *, float out[2], char[256]);
  int (*motion)(void *, float out[2], char[256]);
  int (*key)(void *, uint32_t code, unsigned mode, uint32_t *, char[256]);
  int (*sound)(void *, unsigned system_slot, char[256]);
  int (*clock)(void *, uint32_t *, char[256]); /*GetTickCount*/
  int (*warp)(void *, float x, float y, char[256]);
  int (*schedule)(void *, uint8_t target, uint8_t mode, char[256]);
} BkSpecialUiOps;
typedef struct {
  BkEndingUiFrame sprites;
  BkCommonHudFrame curtain;
  int complete;
} BkSpecialUiFrame;
const char *bk_special_ui_image(unsigned slot, int8_t special);
/*4e6416 and UI constructors4e2dca..4e3a55, in original order. The counter
 * icon's native filename prefix aliases private flow48 state at71ad90:
 * reset/copy those fields exactly, NOT a process-wide reinitialization.
 * Retain per-sprite timer/direction/motion, including unloaded15/16.
 * Image services must load real resources; failure retains the prefix. */
int bk_special_ui_initialize(BkSpecialUi *, BkSpecialUiControl *,
    BkSpecialEventState *, unsigned width, int8_t special,
    const BkSpecialUiOps *, char error[256]);
/*Full51b617 dispatch,4e4472,4e533b/5744 sidebar and4e6ac2 counter.
 * Borrow the same camera/presets/phase/visibility/common owners as world.
 * Callbacks can mutate live aliases; input and time are sampled in native
 * order. Captures immutable UI draws; capture FLUSH is a required real GPU
 * boundary when reached. This is not a registered application scene.
 * Invalid reached array indices and original uninitialized cursor selection
 * reject explicitly. No successful placeholder services are supplied. */
int bk_special_ui_step(BkSpecialUi *, const BkSpecialUiBindings *,
    const BkSpecialUiOps *, float scale, float seconds,
    BkSpecialUiFrame *, char error[256]);
#endif
