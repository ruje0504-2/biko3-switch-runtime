#ifndef BK_TOOLS_ENDING_UI_RENDER_ORACLE_H
#define BK_TOOLS_ENDING_UI_RENDER_ORACLE_H
/* Independent CPU triangle interpolation, repeat/bilinear sampling and
 * UNORM blending for the ending UI render probes. */
#include "resource/assets.h"
#include "scene/ending_ui.h"
#include <math.h>
#include <stdlib.h>
static double texel(const BkImage *im, int x, int y, unsigned c) {
  int w = (int)im->width, h = (int)im->height;
  x = (x % w + w) % w;
  y = (y % h + h) % h;
  return im->rgba[((size_t)y * w + x) * 4 + c] / 255.;
}
static double sample(const BkImage *im, double x, double y, unsigned c) {
  int l = (int)floor(x), t = (int)floor(y);
  double fx = x - l, fy = y - t;
  return (texel(im, l, t, c) * (1 - fx) + texel(im, l + 1, t, c) * fx) *
             (1 - fy) +
         (texel(im, l, t + 1, c) * (1 - fx) + texel(im, l + 1, t + 1, c) * fx) *
             fy;
}
static int coords(const BkEndingUiDraw *d, unsigned x, unsigned y,
                  double out[2]) {
  const unsigned triangles[2][3] = {{0, 1, 2}, {3, 0, 2}};
  const double uv[4][2] = {{d->uv[0], d->uv[1]},
                           {d->uv[2], d->uv[1]},
                           {d->uv[2], d->uv[3]},
                           {d->uv[0], d->uv[3]}};
  for (unsigned i = 0; i < 2; ++i) {
    const unsigned *ix = triangles[i];
    double ax = d->xy[2 * ix[0]], ay = d->xy[2 * ix[0] + 1],
           bx = d->xy[2 * ix[1]] - ax, by = d->xy[2 * ix[1] + 1] - ay,
           cx = d->xy[2 * ix[2]] - ax, cy = d->xy[2 * ix[2] + 1] - ay;
    double det = bx * cy - by * cx;
    if (fabs(det) < 1e-12)
      continue;
    double b = ((x - ax) * cy - (y - ay) * cx) / det,
           c = (bx * (y - ay) - by * (x - ax)) / det, a = 1 - b - c;
    if (a < -.001 || b < -.001 || c < -.001)
      continue;
    /* Leave subpixel raster edge inclusivity to the renderer tests. */
    if (a < .001 || b < .001 || c < .001)
      return -1;
    for (unsigned k = 0; k < 2; ++k)
      out[k] = a * uv[ix[0]][k] + b * uv[ix[1]][k] + c * uv[ix[2]][k];
    return 1;
  }
  return 0;
}
static int expected(const BkImage im[75], const BkEndingUiFrame *f, unsigned x,
                    unsigned y, int rgb[3]) {
  rgb[0] = 70;
  rgb[1] = 110;
  rgb[2] = 160;
  for (unsigned i = 0; i < f->count; ++i) {
    const BkEndingUiDraw *d = &f->draws[i];
    double uv[2];
    int hit = coords(d, x, y, uv);
    if (hit < 0)
      return 0;
    if (!hit)
      continue;
    const BkImage *p = &im[d->slot];
    double u = uv[0] * p->width - .5, v = uv[1] * p->height - .5;
    double alpha = sample(p, u, v, 3) * (unsigned)(d->alpha * 255.) / 255.;
    for (unsigned c = 0; c < 3; ++c) {
      unsigned tint = (d->rgb >> (16 - 8 * c)) & 255;
      rgb[c] =
          (int)lround(sample(p, u, v, c) * tint * alpha + rgb[c] * (1 - alpha));
    }
  }
  return 1;
}
#endif
