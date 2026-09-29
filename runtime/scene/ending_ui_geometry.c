#include "scene/ending_ui_geometry.h"
#include <math.h>
#include <stdio.h>
/*472420 rounds the squared sum, but its sqrt wrapper returns the unrounded
 * double. Do not substitute hypotf or round the root before its next use. */
static double length2(float x, float y) {
  float squared = (float)((double)x * x + (double)y * y);
  return sqrt((double)squared);
}
static int angle(const float first[2], const float last[2], float *out) {
  float pivot[2], offset;
  if (last[0] >= first[0]) {
    if (last[1] <= first[1]) {
      pivot[0] = first[0];
      pivot[1] = last[1];
      offset = 4.71238899230957f;
    } else {
      pivot[0] = last[0];
      pivot[1] = first[1];
      offset = 0;
    }
  } else if (last[1] <= first[1]) {
    pivot[0] = last[0];
    pivot[1] = first[1];
    offset = 3.1415927410125732f;
  } else {
    pivot[0] = first[0];
    pivot[1] = last[1];
    offset = 1.5707963705062866f;
  }
  float rx = (float)((double)pivot[0] - first[0]);
  float ry = (float)((double)pivot[1] - first[1]);
  float dx = (float)((double)last[0] - first[0]);
  float dy = (float)((double)last[1] - first[1]);
  float reference_length = (float)length2(rx, ry);
  float denominator = (float)(length2(dx, dy) * reference_length);
  if (!isfinite(denominator))
    return 0;
  float ratio =
      denominator == 0
          ? 0
          : (float)(((double)rx * dx + (double)ry * dy) / denominator);
  if (ratio < -1)
    ratio = -1;
  if (ratio > 1)
    ratio = 1;
  /*4a7908 has deliberate axis/coincident-point behavior; atan2 differs. */
  float value = (float)(acos((double)ratio) + offset);
  if (!isfinite(value))
    return 0;
  *out = value;
  return 1;
}
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending UI geometry: %s", why);
  return 0;
}
int bk_ending_ui_line(BkEndingStageUi *stage, const int32_t anchor[2],
                      const int32_t pointer[2], float length, float seconds,
                      float *scroll, char e[256]) {
  if (!stage || !anchor || !pointer || !scroll ||
      !bk_ending_stage_ui_image(stage, 63) || !isfinite(length) || length < 0 ||
      !isfinite(seconds) || seconds < 0 || !isfinite(*scroll) ||
      !isfinite(stage->sprites[0].rect[2]) || stage->sprites[0].rect[2] <= 0)
    return fail(e, "invalid line/owner");
  float first[2] = {(float)anchor[0], (float)anchor[1]};
  float last[2] = {(float)pointer[0], (float)pointer[1]};
  BkEndingUiSprite next = stage->sprites[0];
  if (!angle(first, last, &next.transform.radians))
    return fail(e, "nonfinite line angle");
  next.rect[0] = last[0];
  next.rect[1] = last[1];
  double ratio = (double)length / next.rect[2];
  next.transform.scale[0] = (float)ratio;
  next.transform.scale[1] = 1;
  next.uv[0] = *scroll;
  next.uv[1] = 0;
  next.uv[2] = (float)(ratio + *scroll);
  next.uv[3] = 1;
  float next_scroll = (float)((double).1f * seconds + *scroll);
  if (!isfinite(next.transform.scale[0]) || !isfinite(next.uv[2]) ||
      !isfinite(next_scroll))
    return fail(e, "nonfinite line extent");
  if (next_scroll >= .999f)
    next_scroll = .001f;
  stage->sprites[0] = next;
  *scroll = next_scroll;
  return 1;
}
int bk_ending_ui_circle_hit(const float center[2], float radius,
                            const float point[2], int *hit, float *distance) {
  if (!center || !point || !hit || !distance || isnan(radius) || radius < 0)
    return 0;
  for (unsigned i = 0; i < 2; ++i)
    if (!isfinite(center[i]) || !isfinite(point[i]))
      return 0;
  float dx = (float)((double)point[0] - center[0]);
  float dy = (float)((double)point[1] - center[1]);
  float d = (float)length2(dx, dy);
  if (!isfinite(d))
    return 0;
  *hit = radius > d;
  if (*hit)
    *distance = d;
  return 1;
}
