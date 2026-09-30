#ifndef BK_APP_CAPTURE_OUTPUT_H
#define BK_APP_CAPTURE_OUTPUT_H
#include "save/capture_file.h"
#include "scene/screenshot.h"
typedef struct {
  void *context;
  int (*read)(void *, BkCaptureTime *, char error[256]);
} BkCaptureClock;
typedef struct {
  BkCaptureFiles *files;
  BkCaptureClock clock;
} BkCaptureOutput;
/* Borrowed adapter must outlive screenshots using its service. Photos query
 * the platform clock after pixel conversion as49c7e3, then use original group
 * prefix/timestamp; pauses atomically replace sy_99.bmp without clock access.
 */
BkScreenshotOutput bk_capture_output_service(BkCaptureOutput *);
/* Actual platform calendar/timeGetTime source; context is NULL. */
BkCaptureClock bk_capture_output_platform_clock(void);
#endif
