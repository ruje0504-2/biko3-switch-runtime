#ifndef BK_SCENE_SELECTION_UI_H
#define BK_SCENE_SELECTION_UI_H
#include "scene/pause.h"
#include "ui/zoom_sprite.h"
#define BK_SELECTION_SPRITES 48
#define BK_SELECTION_CURSOR 48
#define BK_SELECTION_CURTAIN 49
#define BK_SELECTION_DRAWS 22
typedef struct {
  BkZoomSprite transform; /* fade modes1/1/0; scale may be cropped */
  float rect[4], uv[4];
} BkSelectionSprite;
typedef struct {
  BkSelectionSprite sprites[BK_SELECTION_SPRITES];
  float pointer[2], motion[2], reveal[5], scroll;
  int32_t character_hover, camera_hover, action_hover, info, row, column;
  int32_t selected, camera_mode, music_volume;
  uint8_t voice_active, music_mode;
  uint64_t loaded;
} BkSelectionUi;
typedef struct {
  BkCommonHudState *common;
  BkFlowTransition *flow;
  BkMenuCursor *cursor;
  uint8_t *hover_latched;
  const int32_t *photos; /* five retained721b14 counts */
  int32_t *group, *area, *photo_count;
} BkSelectionBindings;
typedef struct {
  void *context;
  int (*sound)(void *, unsigned system_slot, char error[256]);
  int (*music_gain)(void *, int32_t volume, char error[256]);
  int (*position)(void *, float out[2], char error[256]);
  int (*motion)(void *, float out[2], char error[256]);
  int (*warp)(void *, float x, float y, char error[256]);
  int (*voice_stop)(void *, char error[256]);
  int (*voice_play)(void *, int32_t volume, char error[256]);
  int (*voice_status)(void *, int *playing, char error[256]);
  /* Ordered505b87..50638e resource replacement, with original lighting,
   * variant RNG/face/focus/voice lifetime. Mandatory when selection changes;
   * not an animation request or a successful no-op. */
  int (*replace_actor)(void *, unsigned group, char error[256]);
  int (*release)(void *, uint8_t flow, char error[256]);
} BkSelectionOps;
typedef struct {
  uint32_t buttons, now_ms;
  float seconds, scale;
  int32_t music_master, voice_master;
  uint8_t special, voice_present;
} BkSelectionInput;
typedef struct {
  unsigned slot;
  float corners[4], uv[4], alpha;
} BkSelectionDraw;
typedef struct {
  unsigned count;
  BkSelectionDraw draws[BK_SELECTION_DRAWS];
} BkSelectionFrame;
const char *bk_selection_image(unsigned slot, uint8_t special);
/* Only502480's UI construction and music state; preserves all other state,
 * including skipped sprites. Actor/camera/voice resources owned separately. */
int bk_selection_ui_initialize(BkSelectionUi *, unsigned width, uint8_t special,
                               int32_t music_master, char error[256]);
/*504335: advances/captures UI, then samples pointer and shared cursor. The
 * returned immutable frame must be used even if control releases resources.
 * Called after3D actor draw, before504802 control. */
int bk_selection_ui_view(BkSelectionUi *, const BkSelectionBindings *,
                         const BkSelectionInput *, const BkSelectionOps *,
                         BkSelectionFrame *, char error[256]);
/*504802 UI/input,506629 navigation and51c47e; resource replacement remains an
 * explicit required service. Call after view, exactly once per tick. */
int bk_selection_ui_control(BkSelectionUi *, const BkSelectionBindings *,
                            const BkSelectionInput *, const BkSelectionOps *,
                            char error[256]);
#endif
