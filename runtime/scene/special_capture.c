#include "scene/special_capture.h"
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "special capture: %s", why); return 0;
}
int bk_special_capture_call(BkSpecialCaptureService *s, BkSpecialCapture op,
    char e[256]) {
  if (!s || !s->screenshot) return fail(e, "missing screenshot owner");
  if (op == BK_SPECIAL_CAPTURE_FLUSH) {
    BkPlayerHudCapture service = bk_screenshot_service(s->screenshot);
    return service.capture(service.context, e);
  }
  if (op == BK_SPECIAL_CAPTURE_REQUEST) return bk_screenshot_trigger(s->screenshot, e);
  if (op != BK_SPECIAL_CAPTURE_CONFIGURE || !s->group || !s->album_group ||
      !s->photo_count || !s->photos || *s->group < 0 || *s->group >= 5)
    return fail(e, "invalid operation or shared inventory");
  if (!bk_screenshot_configure(s->screenshot, 1, *s->album_group, &s->crop, e)) return 0;
  uint32_t bits = (uint32_t)*s->photo_count + 1;
  memcpy(s->photo_count, &bits, sizeof bits);
  if (*s->photo_count >= 100) *s->photo_count = 100;
  s->photos[*s->group] = *s->photo_count;
  return 1;
}
