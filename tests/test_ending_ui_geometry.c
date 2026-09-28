#include "scene/ending_ui_geometry.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  BkEndingUi base = {0};
  BkEndingStageUi stage = {0};
  BkEndingUiFrame frame = {0};
  char e[256];
  const int32_t origin[2] = {0, 0}, target[2] = {30, 40};
  assert(bk_ending_stage_ui_initialize(&base, &stage, BK_ENDING_UI_SECONDARY, 0,
                                       0, 1280, e));
  float scroll = .998f;
  assert(bk_ending_ui_line(&stage, origin, target, 400, .1f, &scroll, e));
  BkEndingUiSprite *p = &stage.sprites[0];
  assert(scroll == .001f && p->uv[0] == .998f && p->uv[1] == 0 &&
         p->uv[2] == (float)(400. / 48 + .998f) && p->uv[3] == 1);
  assert(p->rect[0] == 30 && p->rect[1] == 40 && p->transform.degrees == 0);
  assert(bk_ending_ui_dispatch_sprite(&base, &stage, 63, .1f, &frame, e));
  BkEndingUiDraw first = frame.draws[0];
  assert(bk_ending_ui_line(&stage, origin, origin, 240, 0, &scroll, e));
  assert(p->transform.radians == 6.2831854820251465f);
  assert(bk_ending_ui_dispatch_sprite(&base, &stage, 63, .1f, &frame, e));
  assert(frame.count == 2 && !memcmp(&first, &frame.draws[0], sizeof(first)) &&
         memcmp(first.xy, frame.draws[1].xy, sizeof(first.xy)));
  for (unsigned i = 0; i < 8; ++i) {
    BkEndingStageUi s = stage;
    float v = scroll, length = 20, dt = .1f;
    if (i == 0)
      s.loaded = 0;
    if (i == 1)
      s.images[1] = NULL;
    if (i == 2)
      s.sprites[0].rect[2] = 0;
    if (i == 3)
      length = -1;
    if (i == 4)
      dt = NAN;
    if (i == 5)
      v = NAN;
    if (i == 6) {
      s.sprites[0].rect[2] = FLT_MIN;
      length = FLT_MAX;
    }
    if (i == 7)
      dt = -1;
    BkEndingStageUi before = s;
    float old_scroll = v;
    assert(!bk_ending_ui_line(&s, origin, target, length, dt, &v, e));
    assert(!memcmp(&s, &before, sizeof(s)) &&
           !memcmp(&v, &old_scroll, sizeof(v)));
  }
  const float center[2] = {0, 0}, point[2] = {3, 4};
  int hit = -1;
  float distance = 17;
  assert(bk_ending_ui_circle_hit(center, 5, point, &hit, &distance));
  assert(hit == 0 && distance == 17);
  assert(bk_ending_ui_circle_hit(center, nextafterf(5, 6), point, &hit,
                                 &distance));
  assert(hit == 1 && distance == 5);
  assert(bk_ending_ui_circle_hit(center, 0, center, &hit, &distance));
  assert(hit == 0 && distance == 5);
  assert(!bk_ending_ui_circle_hit(center, NAN, point, &hit, &distance));
  assert(!bk_ending_ui_circle_hit(center, 1, (float[2]){FLT_MAX, FLT_MAX}, &hit,
                                  &distance));
  assert(hit == 0 && distance == 5);
  puts("PASS ending UI line/circle: immutable repeated draws, scroll wrap, "
       "strict hit boundary, retained miss distance, atomic rejection");
  return 0;
}
