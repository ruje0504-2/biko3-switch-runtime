#ifndef BK_SCENE_SCREENSHOT_H
#define BK_SCENE_SCREENSHOT_H
#include "scene/player_hud_render.h"
typedef struct BkScreenshot BkScreenshot;
typedef struct {
  void *context;
  int (*write)(void *, int photo, unsigned album_group, const BkBlob *,
               char error[256]);
} BkScreenshotOutput;
/* Borrows renderer/resource store/output service. app maps output to the
 * separately owned port capture files; scene never writes a game/save path. */
BkScreenshot *bk_screenshot_create(BkRenderer *, BkResourceStore *,
                                   const BkScreenshotOutput *, char error[256]);
void bk_screenshot_destroy(BkScreenshot *);
/*4afeb0/4affb8's capture configuration and49d085 request; latest request wins,
 * even pause+photo on one frame. Caller handles se000/se099, counters/menu.
 * Native photo source is bk3_15/cp.bmp with RED COLORREF0xff key,128x32 at
 * (width-136,height-40), no alpha blend/stretch. Crop is the game's viewport
 * in physical target coordinates; its width/height are original window size.
 * album_group is original B53954, not guessed from the active mission. */
int bk_screenshot_request(BkScreenshot *, int photo, unsigned album_group,
                          const BkViewport *crop, char error[256]);
/* Separate original49d085 flag and49c6fe configuration. Trigger retains the
 * previous configuration; configure retains the pending flag. A triggered
 * object without any valid configuration fails when capture is reached.
 * request() above remains the atomic configure-and-trigger convenience. */
int bk_screenshot_trigger(BkScreenshot *, char error[256]);
int bk_screenshot_configure(BkScreenshot *, int photo, unsigned album_group,
                            const BkViewport *crop, char error[256]);
void bk_screenshot_cancel(BkScreenshot *); /*49d094*/
typedef struct {
  BkScreenshot *screenshot;
  BkViewport crop;
} BkScreenshotRequest;
/* Borrowed request binding, suitable for BkPlayerHotkeyServices.capture.
 * Copies the current crop and album group into the pending screenshot. */
int bk_screenshot_request_service(void *, int photo, unsigned album_group,
                                  char error[256]);
/* Concrete49d0eb callback: if pending, synchronous mid-frame GPU capture,
 * crop, optional keyed watermark, canonical BMP, actual atomic file output.
 * Resource/output failure is fatal; pending remains until successful.
 * Service is a legitimate no-op only when native pending flag is zero. */
BkPlayerHudCapture bk_screenshot_service(BkScreenshot *);
#endif
