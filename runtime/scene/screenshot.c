#include "scene/screenshot.h"
#include "resource/bitmap.h"
#include <stdlib.h>
#include <string.h>
struct BkScreenshot {
  BkRenderer *renderer;
  BkResourceStore *store;
  BkScreenshotOutput output;
  uint8_t *pixels;
  unsigned width, height, group;
  const uint32_t *live_group;
  BkViewport crop;
  int pending, photo, configured;
};
static int fail(char *e, const char *why) {
  snprintf(e, 256, "screenshot: %s", why);
  return 0;
}
BkScreenshot *bk_screenshot_create(BkRenderer *r, BkResourceStore *store,
                                   const BkScreenshotOutput *output,
                                   char e[256]) {
  if (!r || !store || !output || !output->write) {
    fail(e, "missing services");
    return NULL;
  }
  BkScreenshot *s = calloc(1, sizeof(*s));
  if (!s) {
    fail(e, "allocation failed");
    return NULL;
  }
  s->renderer = r;
  s->store = store;
  s->output = *output;
  bk_renderer_extent(r, &s->width, &s->height);
  if (!s->width || !s->height || s->width > 16384 || s->height > 16384) {
    fail(e, "invalid target extent");
    goto bad;
  }
  s->pixels = malloc((size_t)s->width * s->height * 4);
  if (!s->pixels) {
    fail(e, "pixel allocation failed");
    goto bad;
  }
  return s;
bad:
  bk_screenshot_destroy(s);
  return NULL;
}
void bk_screenshot_destroy(BkScreenshot *s) {
  if (s) {
    free(s->pixels);
    free(s);
  }
}
int bk_screenshot_bind_album_group(BkScreenshot *s, const uint32_t *group,
                                    char e[256]) {
  if (!s) return fail(e, "missing owner");
  s->live_group = group;
  return 1;
}
int bk_screenshot_configure(BkScreenshot *s, int photo, unsigned group,
                          const BkViewport *v, char e[256]) {
  if (!s || !v || group >= 5 || !v->width || !v->height || v->x > s->width ||
      v->y > s->height || v->width > s->width - v->x ||
      v->height > s->height - v->y ||
      (photo && (v->width < 136 || v->height < 40)))
    return fail(e, "invalid capture request/rectangle");
  s->crop = *v;
  s->photo = !!photo;
  s->group = group;
  s->configured = 1;
  return 1;
}
int bk_screenshot_trigger(BkScreenshot *s, char e[256]) {
  if (!s) return fail(e, "missing owner");
  s->pending = 1; return 1;
}
int bk_screenshot_request(BkScreenshot *s, int photo, unsigned group,
    const BkViewport *v, char e[256]) {
  return bk_screenshot_configure(s, photo, group, v, e) &&
         bk_screenshot_trigger(s, e);
}
void bk_screenshot_cancel(BkScreenshot *s) {
  if (s)
    s->pending = 0;
}
int bk_screenshot_request_service(void *context, int photo, unsigned group,
                                  char error[256]) {
  BkScreenshotRequest *request = context;
  if (!request)
    return fail(error, "missing request binding");
  return bk_screenshot_request(request->screenshot, photo, group,
                               &request->crop, error);
}
static int capture(void *ctx, char e[256]) {
  BkScreenshot *s = ctx;
  if (!s)
    return fail(e, "missing owner");
  if (!s->pending)
    return 1;
  if (!s->configured) return fail(e, "pending capture has no configuration");
  BkImage watermark = {0}, output = {0};
  BkBlob raw = {0}, bmp = {0};
  int ok = 0;
  if (s->photo) {
    BkResourceResult read =
        bk_resources_read(s->store, "bk3_15", "cp.bmp", &raw, e);
    if (read != BK_RESOURCE_OK) {
      if (read == BK_RESOURCE_MISSING)
        fail(e, "missing bk3_15/cp.bmp watermark");
      goto done;
    }
    if (!bk_image_decode(raw.data, raw.size, &watermark, e))
      goto done;
    if (watermark.width != 128 || watermark.height != 32) {
      fail(e, "changed watermark extent");
      goto done;
    }
  }
  output.width = s->crop.width;
  output.height = s->crop.height;
  output.rgba = malloc((size_t)output.width * output.height * 4);
  if (!output.rgba) {
    fail(e, "crop allocation failed");
    goto done;
  }
  if (!bk_renderer_capture(s->renderer, s->pixels,
                           (size_t)s->width * s->height * 4, e))
    goto done;
  for (unsigned y = 0; y < output.height; ++y)
    memcpy(output.rgba + (size_t)y * output.width * 4,
           s->pixels + ((size_t)(s->crop.y + y) * s->width + s->crop.x) * 4,
           (size_t)output.width * 4);
  if (s->photo) {
    for (unsigned y = 0; y < 32; ++y)
      for (unsigned x = 0; x < 128; ++x) {
        const uint8_t *src = watermark.rgba + (y * 128 + x) * 4;
        if (src[0] == 255 && src[1] == 0 && src[2] == 0)
          continue;
        uint8_t *dest =
            output.rgba + ((size_t)(output.height - 40 + y) * output.width +
                           output.width - 136 + x) *
                              4;
        memcpy(dest, src, 3);
        dest[3] = 255;
      }
  }
  if (!bk_bitmap_encode(&output, &bmp, e))
    goto done;
  unsigned group = s->photo && s->live_group ? *s->live_group : s->group;
  if (s->photo && group >= 5) {
    fail(e, "invalid live album group");
    goto done;
  }
  if (!s->output.write(s->output.context, s->photo, group, &bmp, e))
    goto done;
  s->pending = 0;
  ok = 1;
done:
  bk_image_free(&watermark);
  bk_image_free(&output);
  bk_blob_free(&raw);
  bk_blob_free(&bmp);
  return ok;
}
BkPlayerHudCapture bk_screenshot_service(BkScreenshot *s) {
  return (BkPlayerHudCapture){s, capture};
}
