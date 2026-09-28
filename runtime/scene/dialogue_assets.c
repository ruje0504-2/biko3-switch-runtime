#include "scene/dialogue_assets.h"
int bk_dialogue_assets_load(BkDialogueAssets *a, BkResourceStore *store,
                            const char *filename, char error[256]) {
  if (!a || !store || !filename) {
    snprintf(error, 256, "dialogue assets: missing arguments");
    return 0;
  }
  BkBlob raw = {0};
  BkResourceResult r =
      bk_resources_read(store, "bk3_05", filename, &raw, error);
  if (r != BK_RESOURCE_OK) {
    if (r == BK_RESOURCE_MISSING)
      snprintf(error, 256, "dialogue assets: missing %s", filename);
    return 0;
  }
  BkDialogue state = a->state;
  if (!bk_dialogue_open(&state, raw.data, raw.size, filename, error)) {
    bk_blob_free(&raw);
    return 0;
  }
  bk_blob_free(&a->raw);
  a->raw = raw;
  a->state = state;
  return 1;
}
int bk_dialogue_assets_next(BkDialogueAssets *a, int *done, char error[256]) {
  if (!a) {
    snprintf(error, 256, "dialogue assets: missing instance");
    return 0;
  }
  return bk_dialogue_next(&a->state, a->raw.data, a->raw.size, done, error);
}
void bk_dialogue_assets_close(BkDialogueAssets *a) {
  if (a) {
    bk_dialogue_close(&a->state);
    bk_blob_free(&a->raw);
  }
}
