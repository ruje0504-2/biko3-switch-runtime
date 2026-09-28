#include "app/pause_resources.h"
BkPauseRender *bk_pause_resources_load(BkRenderer *r, BkResourceStore *store,
                                       BkCaptureFiles *files, size_t limit,
                                       char e[256]) {
  BkBlob raw = {0};
  BkImage image = {0};
  BkResourceResult read = bk_capture_file_read_pause(files, limit, &raw, e);
  if (read != BK_RESOURCE_OK) {
    if (read == BK_RESOURCE_MISSING)
      snprintf(e, 256, "pause resources: sy_99.bmp is missing");
    return NULL;
  }
  BkPauseRender *result = NULL;
  if (bk_image_decode(raw.data, raw.size, &image, e))
    result = bk_pause_render_create(r, store, &image, e);
  bk_image_free(&image);
  bk_blob_free(&raw);
  return result;
}
int bk_pause_capture_remove(void *context, char e[256]) {
  return bk_capture_file_remove_pause(context, e);
}
