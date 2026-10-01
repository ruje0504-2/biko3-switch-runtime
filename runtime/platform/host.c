#include "platform/platform.h"
int bk_platform_commit_save(const char *path, char error[256]) {
  (void)path;
  (void)error;
  return 1;
}
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {
  unsigned frame;
  uint32_t held, pressed;
} ReplayEvent;
struct BkPlatform {
  unsigned frames_remaining;
  unsigned frame_index, pause_at;
  ReplayEvent *events;
  unsigned event_count, event_next;
  uint32_t held;
  int replay;
  unsigned replay_hz;
};
static int load_replay(BkPlatform *p, const char *path, char error[256]) {
  FILE *in = fopen(path, "r");
  if (!in) {
    snprintf(error, 256, "cannot open host input replay");
    return 0;
  }
  char line[256];
  while (fgets(line, sizeof(line), in)) {
    if (line[0] == '#' || line[0] == '\n')
      continue;
    ReplayEvent event;
    char extra;
    if (sscanf(line, "%u %x %x %c", &event.frame, &event.held, &event.pressed,
               &extra) != 3 ||
        event.frame >= 1000000 || p->event_count >= 100000 ||
        (p->event_count &&
         event.frame <= p->events[p->event_count - 1].frame)) {
      snprintf(error, 256,
               "replay requires ordered rows: frame held-hex pressed-hex");
      fclose(in);
      return 0;
    }
    ReplayEvent *events =
        realloc(p->events, (p->event_count + 1) * sizeof(*events));
    if (!events) {
      fclose(in);
      snprintf(error, 256, "replay allocation failed");
      return 0;
    }
    p->events = events;
    p->events[p->event_count++] = event;
  }
  int ok = !ferror(in);
  if (fclose(in))
    ok = 0;
  if (!ok)
    snprintf(error, 256, "replay read failed");
  p->replay = 1;
  return ok;
}
static uint64_t runtime_origin_ms;
static int runtime_clock_started;
int bk_platform_runtime_clock(int timer, uint32_t *out, char error[256]) {
  if (!out || timer < 0 || timer > 2) {
    snprintf(error, 256, "platform: invalid runtime clock domain"); return 0;
  }
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
    snprintf(error, 256, "platform: monotonic clock unavailable"); return 0;
  }
  uint64_t now = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
  if (!runtime_clock_started) { runtime_origin_ms = now; runtime_clock_started = 1; }
  *out = (uint32_t)(timer == 2 ? now - runtime_origin_ms : now);
  return 1;
}
BkPlatform *bk_platform_open(int argc, char **argv, BkLaunchConfig *config,
                             char error[256]) {
  uint32_t initial_clock;
  if (!bk_platform_runtime_clock(2, &initial_clock, error)) return NULL;
  unsigned frames = 1;
  const char *scene = "game";
  const char *pause_root = NULL;
  const char *replay = NULL;
  unsigned pause_at = UINT_MAX;
  if (argc < 3 || !(argc % 2)) {
    snprintf(error, 256,
             "usage: biko3-preview game-directory output.rgba [--frames N] "
             "[--scene title|office|camera-track|actor|pause|game|ending] "
             "[--pause-root DIR] [--pause-at N] [--replay FILE]");
    return NULL;
  }
  for (int i = 3; i < argc; i += 2) {
    if (!strcmp(argv[i], "--replay")) {
      replay = argv[i + 1];
      continue;
    }
    if (!strcmp(argv[i], "--scene") &&
        (!strcmp(argv[i + 1], "title") || !strcmp(argv[i + 1], "office") ||
         !strcmp(argv[i + 1], "camera-track") ||
         !strcmp(argv[i + 1], "actor") || !strcmp(argv[i + 1], "pause") ||
         !strcmp(argv[i + 1], "game") || !strcmp(argv[i + 1], "ending"))) {
      scene = argv[i + 1];
      continue;
    }
    if (strcmp(argv[i], "--frames") && strcmp(argv[i], "--pause-root") &&
        strcmp(argv[i], "--pause-at")) {
      snprintf(error, 256, "invalid preview option: %.80s", argv[i]);
      return NULL;
    }
    if (!strcmp(argv[i], "--pause-root")) {
      pause_root = argv[i + 1];
      continue;
    }
    if (!strcmp(argv[i], "--pause-at")) {
      char *end;
      errno = 0;
      unsigned long n = strtoul(argv[i + 1], &end, 10);
      if (errno || !*argv[i + 1] || *end || n >= 1000000) {
        snprintf(error, 256, "pause-at must be between 0 and 999999");
        return NULL;
      }
      pause_at = (unsigned)n;
      continue;
    }
    char *end;
    errno = 0;
    unsigned long n = strtoul(argv[i + 1], &end, 10);
    if (errno || !*argv[i + 1] || *end || n < 1 || n > 1000000) {
      snprintf(error, 256, "frames must be between 1 and 1000000");
      return NULL;
    }
    frames = (unsigned)n;
  }
  BkPlatform *p = calloc(1, sizeof(*p));
  if (!p) {
    snprintf(error, 256, "platform allocation failed");
    return NULL;
  }
  p->frames_remaining = frames;
  p->replay_hz = 60;
  /* Deliberate host test clock; it never changes Switch runtime timing. */
  const char *hz_text = getenv("BK_REPLAY_HZ");
  if (hz_text) {
    char *end;
    unsigned long hz = strtoul(hz_text, &end, 10);
    if (!*hz_text || *end || hz < 5 || hz > 240) {
      snprintf(error, 256, "BK_REPLAY_HZ must be5..240");
      bk_platform_close(p);
      return NULL;
    }
    p->replay_hz = (unsigned)hz;
  }
  p->pause_at = pause_at;
  if (replay && !load_replay(p, replay, error)) {
    bk_platform_close(p);
    return NULL;
  }
  *config = (BkLaunchConfig){
      argv[1], argv[2], scene, 0, pause_root, getenv("BK_SHOW_FPS") != NULL};
  return p;
}
FILE *bk_platform_log(BkPlatform *p) {
  (void)p;
  return stderr;
}
int bk_platform_poll(BkPlatform *p, BkInput *input) {
  *input = (BkInput){0};
  if (!p->frames_remaining)
    return 0;
  p->frames_remaining--;
  input->held = p->held;
  if (p->event_next < p->event_count &&
      p->events[p->event_next].frame == p->frame_index) {
    ReplayEvent *event = &p->events[p->event_next++];
    input->held = event->held;
    input->pressed = event->pressed;
    input->released = p->held & ~event->held;
    p->held = event->held;
  }
  if (p->frame_index++ == p->pause_at)
    input->pressed |= BK_BUTTON_PAUSE;
  input->pointer_x = 320;
  input->pointer_y = 240;
  return 1;
}
double bk_platform_seconds(BkPlatform *p) {
  if (p->replay)
    return (double)p->frame_index / p->replay_hz;
  return bk_platform_performance_seconds(p);
}
double bk_platform_performance_seconds(BkPlatform *p) {
  (void)p;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
int bk_platform_calendar_time(BkCalendarTime *out, char error[256]) {
  time_t now = time(NULL);
  struct tm local;
  struct timespec ticks;
  if (!out || now == (time_t)-1 || !localtime_r(&now, &local) ||
      clock_gettime(CLOCK_MONOTONIC, &ticks)) {
    snprintf(error, 256, "platform: capture clock unavailable");
    return 0;
  }
  *out = (BkCalendarTime){(unsigned)local.tm_year + 1900,
                          (unsigned)local.tm_mon + 1,
                          (unsigned)local.tm_mday,
                          (unsigned)local.tm_hour,
                          (unsigned)local.tm_min,
                          (unsigned)local.tm_sec,
                          (uint32_t)((uint64_t)ticks.tv_sec * 1000 +
                                     (uint64_t)ticks.tv_nsec / 1000000)};
  return 1;
}
void bk_platform_report_error(BkPlatform *p, const char *error) {
  (void)p;
  fprintf(stderr, "FAILED: %s\n", error);
}
void bk_platform_close(BkPlatform *p) {
  if (p)
    free(p->events);
  free(p);
}
