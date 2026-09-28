#include "platform/platform.h"
#include <errno.h>
#include <stdlib.h>
#include <switch.h>
#include <sys/stat.h>
#include <time.h>
struct BkPlatform {
  FILE *log;
  PadState pad;
};
BkPlatform *bk_platform_open(int argc, char **argv, BkLaunchConfig *config,
                             char error[256]) {
  if (appletGetAppletType() != AppletType_Application &&
      appletGetAppletType() != AppletType_SystemApplication) {
    snprintf(error, 256, "Full-memory application mode is required.");
    return NULL;
  }
  BkPlatform *p = calloc(1, sizeof(*p));
  if (!p) {
    snprintf(error, 256, "platform allocation failed");
    return NULL;
  }
  mkdir("sdmc:/switch/biko3", 0777);
  /* Keep the preceding session's error across one relaunch. fsdev rename
   * cannot replace a destination, so retire only this owned log slot first.
   * If rotation fails, append instead of truncating the useful old log. */
  const char *current_log = "sdmc:/switch/biko3/nvk.log";
  const char *previous_log = "sdmc:/switch/biko3/nvk.previous.log";
  const char *log_mode = "a";
  struct stat log_info;
  if (!stat(current_log, &log_info)) {
    if (S_ISREG(log_info.st_mode) &&
        (!remove(previous_log) || errno == ENOENT) &&
        !rename(current_log, previous_log))
      log_mode = "w";
  } else if (errno == ENOENT)
    log_mode = "w";
  p->log = fopen(current_log, log_mode);
  if (!p->log)
    p->log = stderr;
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&p->pad);
  hidInitializeTouchScreen();
  *config = (BkLaunchConfig){argc > 1 ? argv[1] : "sdmc:/switch/biko3/game",
                             NULL,
                             "game",
                             1,
                             "sdmc:/switch/biko3/captures",
                             1};
  return p;
}
FILE *bk_platform_log(BkPlatform *p) { return p ? p->log : stderr; }
static uint32_t buttons(uint64_t raw) {
  uint32_t result = 0;
  const uint64_t native[] = {HidNpadButton_A,     HidNpadButton_B,
                             HidNpadButton_Plus,  HidNpadButton_Up,
                             HidNpadButton_Down,  HidNpadButton_Left,
                             HidNpadButton_Right, HidNpadButton_X,
                             HidNpadButton_Y,     HidNpadButton_R};
  for (unsigned i = 0; i < sizeof(native) / sizeof(native[0]); i++)
    if (raw & native[i])
      result |= 1u << i;
  if (raw & HidNpadButton_X)
    result |= BK_BUTTON_GAME_CAMERA;
  if (raw & HidNpadButton_Y)
    result |= BK_BUTTON_PHOTO;
  if (raw & HidNpadButton_A)
    result |= BK_BUTTON_INTERACT;
  if (raw & HidNpadButton_B)
    result |= BK_BUTTON_STANCE;
  if (raw & HidNpadButton_ZL)
    result |= BK_BUTTON_SLOW;
  return result;
}
int bk_platform_poll(BkPlatform *p, BkInput *input) {
  *input = (BkInput){0};
  if (!appletMainLoop())
    return 0;
  padUpdate(&p->pad);
  input->held = buttons(padGetButtons(&p->pad));
  input->pressed = buttons(padGetButtonsDown(&p->pad));
  input->released = buttons(padGetButtonsUp(&p->pad));
  HidAnalogStickState move = padGetStickPos(&p->pad, 0);
  HidAnalogStickState look = padGetStickPos(&p->pad, 1);
  input->move_x = (float)move.x / 32768;
  input->move_y = (float)move.y / 32768;
  input->look_x = (float)look.x / 32768;
  input->look_y = (float)look.y / 32768;
  HidTouchScreenState touch = {0};
  hidGetTouchScreenStates(&touch, 1);
  input->pointer_active = touch.count != 0;
  input->pointer_x = touch.count ? (float)touch.touches[0].x : 320;
  input->pointer_y = touch.count ? (float)touch.touches[0].y : 240;
  return 1;
}
double bk_platform_seconds(BkPlatform *p) {
  (void)p;
  return (double)armTicksToNs(armGetSystemTick()) * 1e-9;
}
double bk_platform_performance_seconds(BkPlatform *p) {
  return bk_platform_seconds(p);
}
int bk_platform_calendar_time(BkCalendarTime *out, char error[256]) {
  time_t now = time(NULL);
  struct tm local;
  if (!out || now == (time_t)-1 || !localtime_r(&now, &local)) {
    snprintf(error, 256, "platform: capture clock unavailable");
    return 0;
  }
  *out =
      (BkCalendarTime){(unsigned)local.tm_year + 1900,
                       (unsigned)local.tm_mon + 1,
                       (unsigned)local.tm_mday,
                       (unsigned)local.tm_hour,
                       (unsigned)local.tm_min,
                       (unsigned)local.tm_sec,
                       (uint32_t)(armTicksToNs(armGetSystemTick()) / 1000000)};
  return 1;
}
void bk_platform_report_error(BkPlatform *p, const char *error) {
  fprintf(bk_platform_log(p), "FAILED: %s\n", error);
  fflush(bk_platform_log(p));
  PadState pad;
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&pad);
  consoleInit(NULL);
  printf("Biko3 development preview\n\n%s\n\nLog: "
         "sdmc:/switch/biko3/nvk.log\nPress B to return.\n",
         error);
  while (appletMainLoop()) {
    padUpdate(&pad);
    if (padGetButtonsDown(&pad) & HidNpadButton_B)
      break;
    consoleUpdate(NULL);
  }
  consoleExit(NULL);
}
void bk_platform_close(BkPlatform *p) {
  if (!p)
    return;
  if (p->log != stderr)
    fclose(p->log);
  free(p);
}
