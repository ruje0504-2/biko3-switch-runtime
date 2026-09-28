#include "ui/effect_sprite.h"
#include <math.h>
#include <string.h>
static int modes(uint8_t enter, uint8_t exit, uint8_t idle) {
  return enter <= 3 && exit <= 3 && (idle == 0 || idle == 3 || idle == 5);
}
static int rectangle(const float r[4]) {
  if (!r)
    return 0;
  for (unsigned i = 0; i < 4; ++i)
    if (!isfinite(r[i]))
      return 0;
  /* Avoid the original undefined float-to-int conversion in50e57a. */
  return r[2] >= 0 && r[3] >= 0 && (double)r[2] * .5 < 2147483648.0 &&
         (double)r[3] * .5 < 2147483648.0;
}
static void half(BkEffectSprite *s, const float r[4]) {
  s->half_extent[0] = (int32_t)((double)r[2] * .5);
  s->half_extent[1] = (int32_t)((double)r[3] * .5);
}
static int valid(const BkEffectSprite *s) {
  if (!s || !modes(s->enter, s->exit, s->idle) || s->fade.stage > 5 ||
      !isfinite(s->fade.alpha) || s->fade.alpha < 0 || s->fade.alpha > 1 ||
      !isfinite(s->fade.speed) || s->fade.speed < 0 || !isfinite(s->degrees) ||
      !isfinite(s->radians))
    return 0;
  for (unsigned i = 0; i < 2; ++i)
    if (!isfinite(s->scale[i]) || !isfinite(s->pivot[i]) ||
        !isfinite(s->motion[i]))
      return 0;
  return 1;
}
int bk_effect_sprite_initialize(BkEffectSprite *s, float r[4], uint8_t enter,
                                uint8_t exit, uint8_t idle) {
  if (!s || !rectangle(r) || !modes(enter, exit, idle))
    return 0;
  BkEffectSprite n = *s;
  bk_fade_sprite_initialize(&n.fade);
  n.enter = enter;
  n.exit = exit;
  n.idle = idle;
  n.scale[0] = n.scale[1] = 1;
  n.pivot[0] = n.pivot[1] = enter == 2 ? .5f : 0;
  n.degrees = n.radians = 0;
  half(&n, r);
  float p[4];
  memcpy(p, r, sizeof(p));
  if (enter == 2) {
    p[0] = (float)((double)p[0] + n.half_extent[0]);
    p[1] = (float)((double)p[1] + n.half_extent[1]);
  }
  if (!rectangle(p))
    return 0;
  *s = n;
  memcpy(r, p, sizeof(p));
  return 1;
}
static int advance(BkEffectSprite *s, const float r[4], float dt, int image) {
  if (!valid(s) || !rectangle(r) || !isfinite(dt) || dt < 0)
    return 0;
  BkEffectSprite n = *s;
  switch (n.fade.stage) {
  case 0:
    n.fade.alpha = 0;
    half(&n, r);
    if (n.enter >= 2) {
      n.scale[0] = n.scale[1] = n.enter == 2 ? 0 : 5;
      n.pivot[0] = n.pivot[1] = .5f;
    }
    break;
  case 1:
    n.motion[0] = n.motion[1] = 0;
    if (n.enter == 0) {
      n.fade.alpha = 1;
      n.fade.stage = 2;
      break;
    }
    n.fade.alpha = (float)(n.fade.alpha +
                           (double)n.fade.speed * (n.enter == 1 ? .5 : 1) * dt);
    if (n.enter >= 2)
      for (unsigned i = 0; i < 2; ++i)
        n.scale[i] = (float)(n.scale[i] + (n.enter == 2 ? 1.0 : -7.0) * dt);
    /* Native tests X twice, even when Y is different. */
    if (n.fade.alpha >= 1 || (n.enter == 2 && n.scale[0] >= 1) ||
        (n.enter == 3 && n.scale[0] <= 1)) {
      n.fade.alpha = 1;
      if (n.enter >= 2)
        n.scale[0] = n.scale[1] = 1;
      n.fade.stage = 2;
    }
    break;
  case 2:
    n.fade.alpha = 1;
    if (n.idle != 0) {
      n.scale[0] = n.scale[1] = 1;
      half(&n, r);
      if (n.idle == 5)
        n.pivot[0] = n.pivot[1] = .5f;
    }
    n.fade.stage = 3;
    /* fall through */
  case 3:
    if (n.idle == 0)
      n.fade.alpha = 1;
    else if (n.idle == 3) {
      n.degrees = (float)(n.degrees + 120.0 * dt);
      if (n.degrees >= 360)
        n.degrees = 0;
      n.pivot[0] = n.pivot[1] = .5f;
      if (image)
        n.radians = (float)(n.degrees * .01745);
    } else {
      if (n.direction == 0 || n.direction == 1) {
        double target = n.direction == 0 ? 2 : -2;
        n.motion[0] = (float)(n.motion[0] + (target - n.motion[0]) * (dt * .5));
      }
      n.degrees = (float)((double)n.degrees + n.motion[0]);
      if (n.degrees >= 1)
        n.direction = 1;
      else if (n.degrees <= -1)
        n.direction = 0;
      if (image)
        n.radians = (float)(n.degrees * .01745);
    }
    break;
  case 4:
    n.fade.alpha = 1;
    half(&n, r);
    if (n.exit != 1)
      n.scale[0] = n.scale[1] = 1;
    if (n.exit >= 2)
      n.pivot[0] = n.pivot[1] = .5f;
    n.fade.stage = 5;
    /* fall through */
  case 5:
    n.motion[0] = n.motion[1] = 0;
    if (n.exit == 0) {
      n.fade.alpha = 0;
      n.fade.stage = 0;
      break;
    }
    n.fade.alpha = (float)(n.fade.alpha -
                           (double)n.fade.speed * (n.exit == 1 ? .5 : 1) * dt);
    if (n.exit != 1)
      for (unsigned i = 0; i < 2; ++i)
        n.scale[i] = (float)(n.scale[i] + (n.exit == 2 ? -.5 : 2.0) * dt);
    if (n.fade.alpha <= 0 ||
        (n.exit == 2 && n.scale[0] <= 0 && n.scale[1] <= 0) ||
        (n.exit == 3 && n.scale[0] >= 5 && n.scale[1] >= 5)) {
      n.fade.alpha = 0;
      if (n.exit != 1)
        n.scale[0] = n.scale[1] = 0;
      n.fade.stage = 0;
    }
    break;
  }
  if (!valid(&n))
    return 0;
  *s = n;
  return 1;
}
int bk_effect_sprite_advance(BkEffectSprite *s, const float r[4], float dt) {
  return advance(s, r, dt, 1);
}
int bk_effect_sprite_advance_detached(BkEffectSprite *s, const float r[4],
                                      float dt) {
  /* The original may call43f230/43f2f7 with a null handle. Keep its CPU
   * animation, omit that invalid device access, retain the last GPU angle. */
  return advance(s, r, dt, 0);
}
int bk_effect_sprite_quad(const BkEffectSprite *s, const float r[4],
                          float xy[8]) {
  if (!s || !r || !xy || !rectangle(r) || !isfinite(s->radians))
    return 0;
  float w = r[2], h = r[3], x = w * s->pivot[0], y = h * s->pivot[1];
  if (s->scale[0] != 1 || s->scale[1] != 1) {
    x *= s->scale[0];
    y *= s->scale[1];
    w *= s->scale[0];
    h *= s->scale[1];
  }
  float q[8] = {-x, -y, -x + w, -y, -x + w, -y + h, -x, -y + h};
  /*43f230's5234e5 for axis(0,0,1), then52287b. Stored float sin/cos,
   * double products/sums and a float store before translation. */
  float c = 1, sn = 0;
  if (s->radians != 0) {
    c = (float)cos((double)s->radians);
    sn = (float)sin((double)s->radians);
  }
  for (unsigned i = 0; i < 4; ++i) {
    if (s->radians != 0) {
      float px = q[2 * i], py = q[2 * i + 1];
      q[2 * i] = (float)((double)px * c - (double)py * sn);
      q[2 * i + 1] = (float)((double)px * sn + (double)py * c);
    }
    q[2 * i] += r[0];
    q[2 * i + 1] += r[1];
    if (!isfinite(q[2 * i]) || !isfinite(q[2 * i + 1]))
      return 0;
  }
  memcpy(xy, q, sizeof(q));
  return 1;
}
