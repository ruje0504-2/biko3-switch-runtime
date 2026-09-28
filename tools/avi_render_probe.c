#include "scene/avi_texture.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
static const BkVertex quad[6] = {
    {-1, -1, 0, 0, 0, 1, 1, 1, 1}, {1, -1, 0, 1, 0, 1, 1, 1, 1},
    {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, -1, 0, 0, 0, 1, 1, 1, 1},
    {1, 1, 0, 1, 1, 1, 1, 1, 1},   {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0};
  BkArchive pack = {0};
  BkRenderer *r = NULL;
  BkAviTexture *video = NULL;
  uint8_t *source = NULL, *pixels = NULL;
  unsigned frames = 0, updates = 0, repeats = 0, max_error = 0;
  uint64_t hash = UINT64_C(14695981039346656037);
  if (!bk_archive_open(&pack, argv[1], error))
    goto fail;
  r = bk_renderer_create(256, 256, stderr, error);
  pixels = malloc(256 * 256 * 4);
  if (!r || !pixels)
    goto fail;
  const char *names[] = {"poi.avi", "D_moza.avi"};
  for (unsigned file = 0; file < 2; file++) {
    const BkEntry *entry = bk_archive_find(&pack, names[file]);
    if (!entry || !bk_archive_read(&pack, entry, &source, error))
      goto fail;
    video = bk_avi_texture_create(r, source, entry->size, 1234, error);
    free(source);
    source = NULL;
    if (!video)
      goto fail;
    const BkImage *image = bk_avi_texture_image(video);
    if (image->width != 256 || image->height != 256)
      goto fail;
    uint32_t key = bk_texture_sort_key(bk_avi_texture_gpu(video));
    uint32_t previous = UINT32_MAX;
    for (unsigned frame = 0; frame < 240; frame++) {
      int32_t now = 1234 + (int32_t)(frame * 37);
      BkRenderStats before = bk_renderer_stats(r);
      if (!bk_avi_texture_step(video, now, now, error))
        goto fail;
      uint32_t at = bk_avi_texture_frame(video);
      if (at != previous)
        updates++;
      previous = at;
      BkRenderStats after = bk_renderer_stats(r);
      if (!frame &&
          (at != UINT32_MAX || after.uploaded_bytes != before.uploaded_bytes)) {
        snprintf(error, 256, "initial frame0 incorrectly decoded/uploaded");
        goto fail;
      }
      if (!frame)
        for (size_t i = 0; i < 256 * 256 * 4; i++)
          if (image->rgba[i] != (i % 4 == 3 ? 255 : 0)) {
            snprintf(error, 256, "initial black policy mismatch");
            goto fail;
          }
      if (!bk_renderer_begin(r, error) ||
          !bk_renderer_draw(r, bk_avi_texture_gpu(video), quad, 6, bk_identity,
                            error) ||
          !bk_renderer_end(r, error) ||
          !bk_renderer_readback(r, pixels, 256 * 256 * 4, error))
        goto fail;
      for (size_t i = 0; i < 256 * 256 * 4; i++) {
        unsigned delta = (unsigned)abs((int)pixels[i] - image->rgba[i]);
        if (delta > max_error)
          max_error = delta;
        if (delta > 1) {
          snprintf(error, 256, "AVI Vulkan pixel mismatch file%u/frame%u", file,
                   frame);
          goto fail;
        }
        hash = (hash ^ pixels[i]) * UINT64_C(1099511628211);
      }
      if (bk_texture_sort_key(bk_avi_texture_gpu(video)) != key) {
        snprintf(error, 256, "movie texture identity changed");
        goto fail;
      }
      if (frame % 17 == 0 && frame % 81 != 0) {
        /* Following a loop the SAME clock can select0; don't suppress that.
         * Only count true unchanged requests using the original clock state. */
        before = bk_renderer_stats(r);
        uint32_t prior = bk_avi_texture_frame(video);
        if (!bk_avi_texture_step(video, now, now, error))
          goto fail;
        after = bk_renderer_stats(r);
        if (bk_avi_texture_frame(video) == prior) {
          if (before.uploaded_bytes != after.uploaded_bytes) {
            snprintf(error, 256, "unchanged movie reuploaded");
            goto fail;
          }
          repeats++;
        }
      }
      frames++;
    }
    bk_avi_texture_destroy(video);
    video = NULL;
  }
  bk_renderer_destroy(r);
  bk_archive_close(&pack);
  free(pixels);
  printf("PASS AVI Vulkan %uframes %uchanged requests %uheld requests "
         "pixel_error%u RGBAhash%016" PRIx64 "\n",
         frames, updates, repeats, max_error, hash);
  return 0;
fail:
  fprintf(stderr, "AVI render probe FAILED: %s\n", error);
  bk_avi_texture_destroy(video);
  bk_renderer_destroy(r);
  bk_archive_close(&pack);
  free(source);
  free(pixels);
  return 1;
}
