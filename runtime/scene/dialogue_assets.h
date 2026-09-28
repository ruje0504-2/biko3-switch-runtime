#ifndef BK_SCENE_DIALOGUE_ASSETS_H
#define BK_SCENE_DIALOGUE_ASSETS_H
#include "resource/dialogue.h"
#include "resource/store.h"
/* Zero once at process/session creation, then retain across reloads. Owns
 * raw bytes; state/text address remains stable for font pointer binding.
 * Does not consume pending media directives or own a playback handle. */
typedef struct {
  BkBlob raw;
  BkDialogue state;
} BkDialogueAssets;
int bk_dialogue_assets_load(BkDialogueAssets *, BkResourceStore *,
                            const char *filename, char error[256]);
int bk_dialogue_assets_next(BkDialogueAssets *, int *done, char error[256]);
void bk_dialogue_assets_close(BkDialogueAssets *);
#endif
