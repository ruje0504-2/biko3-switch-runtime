#ifndef BK_SCENE_DIALOGUE_ENTRY_H
#define BK_SCENE_DIALOGUE_ENTRY_H
#include "game/dialogue_entry.h"
#include "scene/dialogue_assets.h"
#include "scene/dialogue_backdrop.h"
#include "ui/text_flow.h"
/* Owns the new raw script on success; retains parser's unassigned metadata.
 * Executes initial next, strlen scroll target and initial image fade/type.
 * Does not reset shared phase, actor state or either external curtain.
 * Failure preserves old bytes and all caller state. GPU/font/media owners
 * are created by the enclosing scene in their original loading order.
 */
int bk_dialogue_entry_open(BkDialogueAssets *, BkTextFlow *,
                           BkDialogueBackdrop *, BkResourceStore *,
                           const BkDialogueEntry *, char error[256]);
#endif
