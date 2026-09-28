#include "scene/avi_texture.h"
#include "media/avi_surface.h"
#include <stdio.h>
#include <stdlib.h>
struct BkAviTexture {
  BkRenderer *renderer;
  BkAvi *avi;
  BkAviDecoder *decoder;
  BkAviClock clock;
  BkImage image;
  BkTexture *texture;
};
BkAviTexture *bk_avi_texture_create(BkRenderer *renderer, const void *bytes,
                                    size_t size, int32_t now, char error[256]) {
  if (!renderer) {
    snprintf(error, 256, "AVI texture: missing renderer");
    return NULL;
  }
  BkAviTexture *v = calloc(1, sizeof(*v));
  if (!v) {
    snprintf(error, 256, "AVI texture: allocation failed");
    return NULL;
  }
  v->renderer = renderer;
  v->avi = bk_avi_open(bytes, size, error);
  if (!v->avi)
    goto fail;
  const BkAviInfo *info = bk_avi_info(v->avi);
  if (info->frames < 2) {
    snprintf(error, 256,
             "AVI texture: native looping requires at least2 frames");
    goto fail;
  }
  v->decoder = bk_avi_decoder_create(v->avi, error);
  v->image.width = info->width;
  v->image.height = info->height;
  size_t pixels = (size_t)info->width * info->height;
  v->image.rgba = calloc(pixels, 4);
  if (!v->decoder || !v->image.rgba) {
    snprintf(error, 256, "AVI texture: decoder/pixel allocation failed");
    goto fail;
  }
  for (size_t i = 0; i < pixels; i++)
    v->image.rgba[4 * i + 3] = 255;
  v->texture =
      bk_texture_create_sampled(renderer, &v->image, BK_WRAP_REPEAT, error);
  if (!v->texture)
    goto fail;
  bk_avi_clock_init(&v->clock, now);
  return v;
fail:
  bk_avi_texture_destroy(v);
  return NULL;
}
void bk_avi_texture_destroy(BkAviTexture *v) {
  if (!v)
    return;
  bk_texture_destroy(v->renderer, v->texture);
  bk_image_free(&v->image);
  bk_avi_decoder_destroy(v->decoder);
  bk_avi_destroy(v->avi);
  free(v);
}
int bk_avi_texture_step(BkAviTexture *v, int32_t now, int32_t restart,
                        char error[256]) {
  if (!v) {
    snprintf(error, 256, "AVI texture: missing owner");
    return 0;
  }
  uint32_t index;
  int request = bk_avi_clock_select(&v->clock, bk_avi_info(v->avi), now,
                                    restart, &index, error);
  if (request < 0)
    return 0;
  if (!request)
    return 1;
  if (!bk_avi_decoder_frame(v->decoder, index, error) ||
      !bk_avi_surface_rgba(bk_avi_decoder_pixels(v->decoder), v->image.width,
                           v->image.height, 0, v->image.rgba,
                           (size_t)v->image.width * v->image.height * 4, error))
    return 0;
  return bk_texture_update(v->renderer, v->texture, &v->image, error);
}
BkTexture *bk_avi_texture_gpu(const BkAviTexture *v) {
  return v ? v->texture : NULL;
}
const BkImage *bk_avi_texture_image(const BkAviTexture *v) {
  return v ? &v->image : NULL;
}
uint32_t bk_avi_texture_frame(const BkAviTexture *v) {
  return v ? bk_avi_decoder_index(v->decoder) : UINT32_MAX;
}
