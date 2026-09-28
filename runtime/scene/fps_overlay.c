#include "scene/fps_overlay.h"
#include "ui/debug_overlay.h"
#include <stdlib.h>
struct BkFpsOverlay {
  BkRenderer *renderer;
  BkTexture *font;
};
BkFpsOverlay *bk_fps_overlay_create(BkRenderer *r, char error[256]) {
  BkFpsOverlay *s = calloc(1, sizeof(*s));
  if (!s) {
    snprintf(error, 256, "FPS overlay allocation failed");
    return NULL;
  }
  s->renderer = r;
  BkImage image = {0};
  if (bk_debug_font_atlas(&image, error))
    s->font = bk_texture_create(r, &image, error);
  bk_image_free(&image);
  if (!s->font) {
    bk_fps_overlay_destroy(s);
    return NULL;
  }
  return s;
}
int bk_fps_overlay_draw(BkFpsOverlay *s, unsigned fps, int inspection,
                        char error[256]) {
  char text[32];
  snprintf(text, sizeof(text), "FPS [%u]", fps);
  unsigned width, height;
  bk_renderer_extent(s->renderer, &width, &height);
  BkViewport view;
  if (!bk_camera_fit(&view, width, height, 4, 3) ||
      !bk_renderer_viewport(s->renderer, NULL, error))
    return 0;
  float scale = view.width / 1280.f, cell = 24 * scale, size = 32 * scale;
  BkVertex v[32 * 24];
  unsigned count = 0;
  /* Native51c9ec/4504bd: x=0,y=928*scale,32*scale font.
   * Two horizontally repeated glyphs per black/color pass (COLORREF).
   * Windows GDI glyphs are replaced by the existing port bitmap alphabet. */
  for (unsigned pass = 0; pass < 4; ++pass)
    for (unsigned i = 0; text[i]; ++i) {
      unsigned g = text[i] >= 'A' && text[i] <= 'Z'   ? text[i] - 'A'
                   : text[i] >= '0' && text[i] <= '9' ? text[i] - '0' + 26
                   : text[i] == '['                   ? 37
                   : text[i] == ']'                   ? 38
                                                      : 40;
      if (g == 40)
        continue;
      float x = view.x + i * cell + (pass < 2 ? 1 + pass : pass - 2);
      float y = view.y + (inspection ? 2 : 928 * scale);
      float x0 = 2 * x / width - 1, x1 = 2 * (x + cell) / width - 1;
      float y0 = 2 * y / height - 1, y1 = 2 * (y + size) / height - 1;
      float u0 = g * 6 / 256.f, u1 = (g + 1) * 6 / 256.f;
      float red = pass >= 2 ? 128 / 255.f : 0,
            green = pass >= 2 ? 216 / 255.f : 0;
      float blue = pass >= 2 ? 1 : 0;
      BkVertex q[6] = {{x0, y0, 0, u0, 0, red, green, blue, 1},
                       {x1, y0, 0, u1, 0, red, green, blue, 1},
                       {x1, y1, 0, u1, 1, red, green, blue, 1},
                       {x0, y0, 0, u0, 0, red, green, blue, 1},
                       {x1, y1, 0, u1, 1, red, green, blue, 1},
                       {x0, y1, 0, u0, 1, red, green, blue, 1}};
      for (unsigned j = 0; j < 6; ++j)
        v[count++] = q[j];
    }
  return bk_renderer_draw(s->renderer, s->font, v, count, bk_identity, error);
}
void bk_fps_overlay_destroy(BkFpsOverlay *s) {
  if (!s)
    return;
  bk_texture_destroy(s->renderer, s->font);
  free(s);
}
