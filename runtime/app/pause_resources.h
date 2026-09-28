#ifndef BK_APP_PAUSE_RESOURCES_H
#define BK_APP_PAUSE_RESOURCES_H
#include "save/capture_file.h"
#include "scene/pause_render.h"
/* Load the port's actual pause capture; missing/corrupt is explicit failure.
 * No read from the original game directory or substitution with a still. */
BkPauseRender *bk_pause_resources_load(BkRenderer *, BkResourceStore *,
                                       BkCaptureFiles *, size_t capture_limit,
                                       char error[256]);
/* BkPauseOps.remove_capture adapter; context is the borrowed BkCaptureFiles. */
int bk_pause_capture_remove(void *context, char error[256]);
#endif
