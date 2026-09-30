#include "scene/special_ui_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct { BkTexture *texture; unsigned references; } Image;
struct BkSpecialUiRender {
  BkRenderer *renderer;
  BkResourceStore *store;
  BkCurtainRender *curtain;
  Image *images[BK_SPECIAL_UI_SPRITES], *snapshot[BK_SPECIAL_UI_SPRITES];
  BkSpecialUiFrame frame;
  unsigned width, height;
  int begun, ready;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "special UI render: %s", why); return 0;
}
static void release(BkSpecialUiRender *r, Image *image) {
  if (image && !--image->references) {
    bk_texture_destroy(r->renderer, image->texture); free(image);
  }
}
BkSpecialUiRender *bk_special_ui_render_create(BkRenderer *renderer,
    BkResourceStore *store, BkCurtainRender *curtain, char e[256]) {
  if (!renderer || !store || !curtain) { fail(e, "missing services"); return NULL; }
  BkSpecialUiRender *r = calloc(1, sizeof *r);
  if (!r) { fail(e, "allocation failed"); return NULL; }
  r->renderer = renderer; r->store = store; r->curtain = curtain; return r;
}
void bk_special_ui_render_destroy(BkSpecialUiRender *r) {
  if (!r) return;
  for (unsigned i = 0; i < BK_SPECIAL_UI_SPRITES; ++i) {
    release(r, r->images[i]); release(r, r->snapshot[i]);
  }
  free(r);
}
int bk_special_ui_render_image(BkSpecialUiRender *r, unsigned slot,
    const char *name, char e[256]) {
  if (!r || slot >= BK_SPECIAL_UI_SPRITES) return fail(e, "invalid image slot");
  Image *next = NULL;
  if (name) {
    BkBlob raw = {0}; BkImage image = {0};
    BkResourceResult result = bk_resources_read(r->store, "bk3_00", name, &raw, e);
    if (result != BK_RESOURCE_OK) {
      if (result == BK_RESOURCE_MISSING) snprintf(e, 256, "special UI render: missing %s", name);
      return 0;
    }
    int ok = bk_image_decode(raw.data, raw.size, &image, e); bk_blob_free(&raw);
    if (!ok) return 0;
    next = calloc(1, sizeof *next);
    if (next) next->texture = bk_texture_create_sampled(r->renderer, &image, BK_WRAP_REPEAT, e);
    bk_image_free(&image);
    if (!next) return fail(e, "image allocation failed");
    if (!next->texture) { free(next); return 0; }
    next->references = 1;
  }
  release(r, r->images[slot]); r->images[slot] = next; return 1;
}
int bk_special_ui_render_begin(BkSpecialUiRender *r, char e[256]) {
  if (!r) return fail(e, "missing owner");
  for (unsigned i = 0; i < BK_SPECIAL_UI_SPRITES; ++i) {
    release(r, r->snapshot[i]); r->snapshot[i] = r->images[i];
    if (r->snapshot[i]) ++r->snapshot[i]->references;
  }
  r->ready = 0; r->begun = 1; return 1;
}
int bk_special_ui_render_prepare(BkSpecialUiRender *r, const BkSpecialUiFrame *f,
    unsigned width, unsigned height, char e[256]) {
  if (!r || !r->begun || !f || !f->complete || !width || !height ||
      f->sprites.count > BK_ENDING_UI_DRAWS || !isfinite(f->curtain.curtain_alpha) ||
      f->curtain.curtain_alpha < 0 || f->curtain.curtain_alpha > 1)
    return fail(e, "invalid frame or no begin");
  unsigned w, h; bk_renderer_extent(r->renderer, &w, &h);
  if (width > w || height > h) return fail(e, "viewport exceeds target");
  for (unsigned i = 0; i < f->sprites.count; ++i) {
    const BkEndingUiDraw *d = &f->sprites.draws[i];
    if (d->slot >= BK_SPECIAL_UI_SPRITES || !r->snapshot[d->slot] ||
        !isfinite(d->alpha) || d->alpha < 0 || d->alpha > 1)
      return fail(e, "invalid draw or missing image");
    for (unsigned j = 0; j < 8; ++j)
      if (!isfinite(d->xy[j])) return fail(e, "invalid corner");
    for (unsigned j = 0; j < 4; ++j)
      if (!isfinite(d->uv[j])) return fail(e, "invalid UV");
  }
  r->frame = *f; r->width = width; r->height = height;
  r->ready = 1; r->begun = 0; return 1;
}
int bk_special_ui_render_draw(BkSpecialUiRender *r, char e[256]) {
  if (!r || !r->ready) return fail(e, "no prepared snapshot");
  float m[16]; memcpy(m, bk_identity, sizeof m);
  m[0] = 2.f / r->width; m[5] = 2.f / r->height;
  m[12] = (float)(1.0 / r->width - 1); m[13] = (float)(1.0 / r->height - 1);
  for (unsigned i = 0; i < r->frame.sprites.count; ++i) {
    const BkEndingUiDraw *d = &r->frame.sprites.draws[i];
    float a = (unsigned)((double)d->alpha * 255) / 255.f;
    float red = ((d->rgb >> 16) & 255) / 255.f, green = ((d->rgb >> 8) & 255) / 255.f,
          blue = (d->rgb & 255) / 255.f;
    BkVertex v[6]; static const unsigned corners[6] = {0, 1, 2, 3, 0, 2};
    for (unsigned j = 0; j < 6; ++j) {
      unsigned k = corners[j];
      v[j] = (BkVertex){d->xy[k * 2], d->xy[k * 2 + 1], 0,
          d->uv[k == 1 || k == 2 ? 2 : 0], d->uv[k >= 2 ? 3 : 1], red, green, blue, a};
    }
    if (!bk_renderer_draw_vertices(r->renderer, r->snapshot[d->slot]->texture,
          v, 6, m, (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, e)) return 0;
  }
  return bk_curtain_render_draw_frame(r->curtain, &r->frame.curtain,
                                      r->width, r->height, e);
}
