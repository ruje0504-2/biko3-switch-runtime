#ifndef BK_PLATFORM_H
#define BK_PLATFORM_H
#include "core/input.h"
#include <stdio.h>
typedef struct BkPlatform BkPlatform;
typedef struct {
  const char *game_root;
  const char *capture_path; /* NULL on Switch; RGBA readback path on host. */
  const char
      *scene;       /* "title" or "office"; application resolves the factory. */
  int audio_device; /* Native output; offscreen host previews remain silent. */
  const char *capture_root; /* Pause screenshot root; app owns the adapter. */
  int show_fps; /* Original FPS counter enabled by default on Switch. */
} BkLaunchConfig;
/* Options borrow argv strings until close. Platform owns its input/log handles.
 */
BkPlatform *bk_platform_open(int argc, char **argv, BkLaunchConfig *config,
                             char error[256]);
FILE *bk_platform_log(BkPlatform *platform);
int bk_platform_poll(BkPlatform *platform, BkInput *input);
double bk_platform_seconds(BkPlatform *platform);
/* Actual monotonic wall time, also during deterministic host input replays. */
double bk_platform_performance_seconds(BkPlatform *platform);
typedef struct {
  unsigned year, month, day, hour, minute, second;
  uint32_t ticks_ms;
} BkCalendarTime;
/* Local wall-clock date plus low32 monotonic milliseconds for photo names. */
int bk_platform_calendar_time(BkCalendarTime *, char error[256]);
/* Must only be called after the renderer releases its native surface. */
void bk_platform_report_error(BkPlatform *platform, const char *error);
void bk_platform_close(BkPlatform *platform);
#endif
