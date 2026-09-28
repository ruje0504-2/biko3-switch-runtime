#include "ui/zoom_sprite.h"
#include <math.h>
#include <string.h>
void bk_zoom_sprite_initialize(BkZoomSprite *s) {
  if (s) {
    *s = (BkZoomSprite){.scale = {1, 1}, .pivot = {.5f, .5f}};
    bk_fade_sprite_initialize(&s->fade);
  }
}
int bk_zoom_sprite_advance(BkZoomSprite *s, float seconds) {
  if (!s || !isfinite(seconds) || seconds < 0 || !isfinite(s->fade.alpha) ||
      s->fade.alpha < 0 || s->fade.alpha > 1 || !isfinite(s->fade.speed) ||
      s->fade.speed < 0 || s->fade.stage > 5)
    return 0;
  for (unsigned i = 0; i < 2; ++i)
    if (!isfinite(s->scale[i]) || s->scale[i] < 0 || !isfinite(s->pivot[i]))
      return 0;
  BkZoomSprite n = *s;
  switch (n.fade.stage) {
  case 0:
    n.fade.alpha = n.scale[0] = n.scale[1] = 0;
    n.pivot[0] = n.pivot[1] = .5f;
    break;
  case 1:
    n.fade.alpha =
        (float)((double)n.fade.alpha + (double)n.fade.speed * seconds);
    n.scale[0] = (float)((double)n.scale[0] + seconds);
    n.scale[1] = (float)((double)n.scale[1] + seconds);
    /* Native50fcd0/50fce6 tests X twice; Y is not a completion gate. */
    if (n.fade.alpha >= 1 || n.scale[0] >= 1) {
      n.fade.alpha = n.scale[0] = n.scale[1] = 1;
      n.fade.stage = 2;
    }
    break;
  case 2:
    n.fade.stage = 3;
    /* fall through */
  case 3:
    n.fade.alpha = 1;
    break;
  case 4:
    n.scale[0] = n.scale[1] = 1;
    /* fall through */
  case 5:
    n.fade.alpha = 0;
    n.fade.stage = 0;
    break;
  }
  if (!isfinite(n.fade.alpha) || !isfinite(n.scale[0]) || !isfinite(n.scale[1]))
    return 0;
  *s = n;
  return 1;
}
int bk_zoom_sprite_rect(const BkZoomSprite *s, const float p[4], float r[4]) {
  if (!s || !p || !r)
    return 0;
  float x = p[2] * s->pivot[0], y = p[3] * s->pivot[1];
  float w = p[2], h = p[3];
  if (s->scale[0] != 1 || s->scale[1] != 1) {
    x *= s->scale[0];
    y *= s->scale[1];
    w *= s->scale[0];
    h *= s->scale[1];
  }
  float right = -x + w, bottom = -y + h;
  const float v[4] = {-x + p[0], -y + p[1], right + p[0], bottom + p[1]};
  for (unsigned i = 0; i < 4; ++i)
    if (!isfinite(v[i]))
      return 0;
  memcpy(r, v, sizeof(v));
  return 1;
}
