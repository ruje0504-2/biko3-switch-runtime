#ifndef BK_SCENE_FAILURE_SESSION_H
#define BK_SCENE_FAILURE_SESSION_H
#include "scene/opening_session.h"
/*4eb0ee packed resources. Native language=1 pack names and all five profile
 * tables; loose localization is resolved by the resource store separately. */
typedef struct {
  const char *speech, *message;
  int32_t label;
} BkFailureResources;
int bk_failure_resources(uint32_t group, uint8_t outcome, BkFailureResources *);
typedef struct {
  BkResourceStore *store;
  BkEntryAssets *entry;
  BkNpcAudio *npc_audio;
  BkPlayerAudio *player_audio;
  BkDialogueAssets *dialogue;
  BkItemFeedback *items;
  BkNoticeTextOps text;
  int32_t speech_volume;
} BkFailureServices;
/* Caller removes forest/player GPU borrowers first. Reload player, retain
 * outer outcome, clear NPC fade flags, load silent NPC speech, close existing
 * dialogue/item metadata, recreate font and bind actual selected message.
 * Shared panel/prompt/visible/advance count are deliberately retained.
 * Callback/resource failure is fatal, with original prefix committed. */
int bk_failure_session_load(const BkFailureServices *, BkGameFrameState *,
                            BkTextFlow *, char error[256]);
int bk_failure_session_message(const BkFailureServices *, uint32_t label,
                               char error[256]);
#endif
