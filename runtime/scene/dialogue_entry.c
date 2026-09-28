#include "scene/dialogue_entry.h"
#include "scene/dialogue_ui.h"
#include <string.h>
int bk_dialogue_entry_open(BkDialogueAssets *assets, BkTextFlow *text,
                           BkDialogueBackdrop *backdrop, BkResourceStore *store,
                           const BkDialogueEntry *entry, char e[256]) {
  if (!assets || !text || !backdrop || !store || !entry ||
      !memchr(entry->file, 0, sizeof(entry->file)) || !entry->file[0] ||
      !memchr(entry->font, 0, sizeof(entry->font)) ||
      strcmp(entry->font, "Type_S.FTT")) {
    snprintf(e, 256, "dialogue entry: invalid script/font or missing owner");
    return 0;
  }
  BkDialogueAssets next = {.state = assets->state};
  next.state.first_label = entry->first;
  next.state.last_label = entry->last;
  int done;
  BkTextFlow flow = {.enabled = 1};
  if (!bk_dialogue_assets_load(&next, store, entry->file, e) ||
      !bk_dialogue_assets_next(&next, &done, e) ||
      !bk_dialogue_text_target(&next.state.text, 27, 4, 16, 1, &flow.target,
                               e)) {
    bk_dialogue_assets_close(&next);
    return 0;
  }
  /* The native loader ignores the initial next return value. */
  BkDialogueBackdrop background = *backdrop;
  bk_fade_sprite_initialize(&background.image);
  bk_fade_sprite_request(&background.image, 1);
  background.image_wanted = 1;
  if (next.state.image_kind == 1 || next.state.image_kind == 2) {
    background.image_kind = next.state.image_kind == 1 ? 0 : 2;
    next.state.image_kind = 0;
  }
  bk_blob_free(&assets->raw);
  *assets = next;
  *text = flow;
  *backdrop = background;
  return 1;
}
