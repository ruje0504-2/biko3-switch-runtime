#ifndef BK_APP_FRONT_END_H
#define BK_APP_FRONT_END_H
#include "scene/dialogue_session.h"
#include "scene/scene.h"
#include "scene/selection_session.h"
#include "scene/title_menu_render.h"
#include "scene/gallery_menu_render.h"
#include "save/unlock_file.h"
typedef struct BkFrontEnd BkFrontEnd;
typedef struct {
  BkSceneServices services;
  BkViewport viewport;
  BkCommonHudState *common;
  BkCurtainRender *curtain;
  BkFlowTransition *flow;
  BkMenuCursor *cursor;
  uint8_t *hover;
  uint32_t *group, *area, *random;
  int32_t *photos, *photo_count;
  void *context;
  int (*schedule)(void *, uint8_t, uint8_t, char error[256]);
  BkUnlockFile *unlock_file; /* optional borrowed port storage, never assets */
  /* Row produced by the preceding real flow16 ending frame. The flag is
   * borrowed until the dialogue callback executes; an absent/invalid row is
   * an explicit storage boundary, never a zero-filled unlock. */
  const uint8_t *ending_flags;
  const int *ending_flags_valid;
  const unsigned *ending_flags_group;
} BkFrontEndConfig;
/* Original retail title1/selection38/dialogue8/gallery18 owner. Retains menu globals
 * between entries; collects released GPU snapshots on the following step.
 * Unimplemented destination flows still fail in the application's loader.
 */
BkFrontEnd *bk_front_end_create(const BkFrontEndConfig *, char error[256]);
void bk_front_end_destroy(BkFrontEnd *);
int bk_front_end_load(BkFrontEnd *, uint8_t flow, uint8_t previous,
                      uint8_t response, double wall_seconds, float game_seconds,
                      char error[256]);
int bk_front_end_stop(BkFrontEnd *, uint8_t flow, char error[256]);
void bk_front_end_collect(BkFrontEnd *);
int bk_front_end_step(BkFrontEnd *, double game_seconds, double wall_seconds,
                      const BkInput *, char error[256]);
int bk_front_end_draw(BkFrontEnd *, char error[256]);
int bk_front_end_after_present(BkFrontEnd *, char error[256]);
int bk_front_end_active(const BkFrontEnd *);
int bk_front_end_result(const BkFrontEnd *, BkDialogueResult *,
                        char error[256]);
/* Retained after logical menu release until the next gallery entry. Rejects
 * before an actual unlocked selection has dispatched flow10. */
int bk_front_end_gallery_result(const BkFrontEnd *, BkGalleryMenuSelection *,
                                char error[256]);
/* Copy the latest successfully persisted table for ending entry50ca48.
 * The scene receives bytes only and owns its independent working copy. */
int bk_front_end_unlocks(const BkFrontEnd *, BkUnlockTable *, char error[256]);
#endif
