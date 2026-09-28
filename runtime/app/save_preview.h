#ifndef BK_APP_SAVE_PREVIEW_H
#define BK_APP_SAVE_PREVIEW_H
#include "save/checkpoint_file.h"
#include "scene/save_menu_render.h"
#include "scene/scene.h"
/* Process-owned CPU state, retained across resource loads like the original
 * globals. Initialize control.tab=3 once; other fields start zero. */
typedef struct {
  BkSaveMenuControl control;
  BkSaveMenuView view;
  BkTextFlow text;
} BkSavePreviewState;
typedef struct {
  uint32_t *group, *area, *random;
  uint8_t *inventory;      /* five live item bytes */
  uint8_t *inventory_tail; /* remaining three native bytes, retained by owner */
} BkSavePreviewGame;
typedef struct {
  void *context;
  int (*release)(void *, uint8_t, char error[256]);
  int (*resume_pause)(void *, char error[256]);
} BkSavePreviewOps;
/* Actual flow28: files, images, font and system audio. No constructor step.
 * Files and process objects must outlive the scene. Caller presents the last
 * snapshot before destroying logically released resources. */
BkScene *bk_save_preview_create(const BkSceneServices *, BkCheckpointFiles *,
                                BkSavePreviewState *, const BkPauseBindings *,
                                const BkSavePreviewGame *,
                                const BkSavePreviewOps *, double elapsed,
                                char error[256]);
/* Explicit wall time immediately before the following scene step. */
void bk_save_preview_clock(BkScene *, double elapsed);
#endif
