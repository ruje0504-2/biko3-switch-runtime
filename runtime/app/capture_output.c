#include "app/capture_output.h"
#include "platform/platform.h"
static int read_clock(void *context, BkCaptureTime *out, char e[256]) {
  (void)context;
  BkCalendarTime t;
  if (!bk_platform_calendar_time(&t, e))
    return 0;
  *out = (BkCaptureTime){t.year, t.month, t.day, t.hour, t.minute, t.second,
                         t.ticks_ms};
  return 1;
}
BkCaptureClock bk_capture_output_platform_clock(void) {
  return (BkCaptureClock){NULL, read_clock};
}
static int write_capture(void *ctx, int photo, unsigned group,
                         const BkBlob *bmp, char e[256]) {
  BkCaptureOutput *out = ctx;
  if (!out || !out->files || !out->clock.read) {
    snprintf(e, 256, "capture output: missing files/clock");
    return 0;
  }
  char name[128] = "sy_99.bmp";
  if (photo) {
    BkCaptureTime time;
    if (!out->clock.read(out->clock.context, &time, e))
      return 0;
    if (!bk_capture_photo_name(name, group, &time)) {
      snprintf(e, 256, "capture output: invalid time/group");
      return 0;
    }
  }
  return bk_capture_file_write(out->files, photo, name, bmp, e);
}
BkScreenshotOutput bk_capture_output_service(BkCaptureOutput *out) {
  return (BkScreenshotOutput){out, write_capture};
}
