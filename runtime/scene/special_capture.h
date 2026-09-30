#ifndef BK_SCENE_SPECIAL_CAPTURE_H
#define BK_SCENE_SPECIAL_CAPTURE_H
#include "scene/screenshot.h"
#include "scene/special_ui.h"
typedef struct {
  BkScreenshot *screenshot; /*process owner, may retain requests across flows*/
  BkViewport crop;
  const int32_t *group; /*7219a8*/
  const unsigned *album_group; /*B53954, not inferred from active character*/
  int32_t *photo_count, *photos; /*734050 and five721b14 entries*/
} BkSpecialCaptureService;
/*49d0eb consumes pending image inside the active GPU frame, BEFORE counter
 * draws.49d085 only marks pending;4affb8 configures the actual screenshot,
 * then wraps/increments/clamps the shared counter and saves its group entry.
 * No filesystem implementation in scene: BkScreenshotOutput remains the
 * required real app storage service. Errors retain executed prefixes. */
int bk_special_capture_call(BkSpecialCaptureService *, BkSpecialCapture,
                              char error[256]);
#endif
