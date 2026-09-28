#ifndef BK_SCENE_ENDING_NORMAL_SESSION_H
#define BK_SCENE_ENDING_NORMAL_SESSION_H
#include "scene/scene.h"
#include "scene/ending_state.h"
#include "game/ending_record.h"
/* Explicit normal-ending scene owner. The default diagnostic entry is the
 * gallery normal branch (previous flow 0x18, selection 0/1); it owns the
 * actual CPU/GPU/audio resources and exposes no synthetic success callback. */
BkScene *bk_ending_normal_scene_create(const BkSceneServices *, unsigned group,
                                       unsigned variant, char error[256]);
/* Must be called after the renderer has presented the prepared snapshot. */
int bk_ending_normal_scene_after_present(BkScene *, char error[256]);
const BkEndingState *bk_ending_normal_scene_state(const BkScene *);
BkScene *bk_ending_normal_scene_create_story(const BkSceneServices *,
                                             unsigned group, unsigned variant,
                                             BkEndingRecords *, char error[256]);
#endif
