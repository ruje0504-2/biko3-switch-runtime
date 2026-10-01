#include "platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <sys/stat.h>
#include <time.h>
struct BkPlatform {
  PadState pad;
  int romfs_mounted;
  int save_mounted;
#ifdef BK_SWITCH_FILE_LOG
  FILE *log;
#endif
};
static uint64_t runtime_origin_ms;
static int runtime_clock_started;
int bk_platform_runtime_clock(int timer, uint32_t *out, char error[256]) {
  if (!out || timer < 0 || timer > 2) {
    snprintf(error, 256, "platform: invalid runtime clock domain"); return 0;
  }
  uint64_t now = armTicksToNs(armGetSystemTick()) / 1000000;
  if (!runtime_clock_started) { runtime_origin_ms = now; runtime_clock_started = 1; }
  *out = (uint32_t)(timer == 2 ? now - runtime_origin_ms : now);
  return 1;
}
BkPlatform *bk_platform_open(int argc, char **argv, BkLaunchConfig *config,
                             char error[256]) {
  uint32_t initial_clock;
  if (!bk_platform_runtime_clock(2, &initial_clock, error)) return NULL;
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
#ifdef BK_SWITCH_FILE_LOG
  mkdir("sdmc:/switch", 0777);
  mkdir("sdmc:/switch/biko3", 0777);
  struct stat previous;
  if (stat("sdmc:/switch/biko3/nvk.log", &previous) == 0) {
    remove("sdmc:/switch/biko3/nvk.previous.log");
    rename("sdmc:/switch/biko3/nvk.log", "sdmc:/switch/biko3/nvk.previous.log");
  }
  p->log = fopen("sdmc:/switch/biko3/nvk.log", "w");
  if (p->log) {
    setvbuf(p->log, NULL, _IOLBF, 0);
    fprintf(p->log, "Diagnostic file logging enabled; sidebar monologue and pose state included.\n");
  }
#endif
  const char *game_root = argc > 1 ? argv[1] : "sdmc:/switch/biko3/game";
  const char *capture_root = "sdmc:/switch/biko3/captures";
  if (envIsNso()) {
    Result rc = romfsMountSelf("romfs");
    if (R_FAILED(rc)) {
      snprintf(error, 256, "Cannot mount installed game data: %08x", rc);
      goto failed;
    }
    p->romfs_mounted = 1;
    AccountUid user = {0};
    rc = accountInitialize(AccountServiceType_Application);
    if (R_SUCCEEDED(rc)) {
      rc = accountGetPreselectedUser(&user);
      accountExit();
    }
    if (R_FAILED(rc) || !accountUidIsValid(&user)) {
      snprintf(error, 256, "Cannot get the selected Switch user: %08x", rc);
      goto failed;
    }
    /* IApplicationFunctions::EnsureSaveData (20): create the selected user's
     * save on first launch, using the installed NACP's size/journal fields. */
    u64 required_space = 0;
    rc = serviceDispatchImpl(appletGetServiceSession_Functions(), 20,
                             &user, sizeof(user), &required_space,
                             sizeof(required_space), (SfDispatchParams){0});
    if (R_FAILED(rc)) {
      snprintf(error, 256, "Cannot prepare user save data: %08x", rc);
      goto failed;
    }
    rc = fsdevMountSaveData("save", FS_SAVEDATA_CURRENT_APPLICATIONID, user);
    if (R_FAILED(rc)) {
      snprintf(error, 256, "Cannot mount user save data: %08x", rc);
      goto failed;
    }
    p->save_mounted = 1;
    game_root = "romfs:";
    capture_root = "save:/biko3";
  } else {
    mkdir("sdmc:/switch/biko3", 0777);
  }
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&p->pad);
  hidInitializeTouchScreen();
  *config = (BkLaunchConfig){game_root,
                             NULL,
                             "game",
                             1,
                             capture_root,
                             0};
  return p;
failed:
  bk_platform_close(p);
  return NULL;
}
FILE *bk_platform_log(BkPlatform *p) {
#ifdef BK_SWITCH_FILE_LOG
  if (p && p->log)
    return p->log;
#endif
  (void)p;
  /* Keep the shared diagnostic stream API without creating SD log files. */
  return stderr;
}
int bk_platform_commit_save(const char *path, char error[256]) {
  if (path && !strncmp(path, "save:/", 6)) {
    Result rc = fsdevCommitDevice("save");
    if (R_FAILED(rc)) {
      snprintf(error, 256, "Save data commit failed: %08x", rc);
      return 0;
    }
  }
  return 1;
}
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
  if (raw & HidNpadButton_L)
    result |= BK_BUTTON_CAMERA_ORBIT;
  if (raw & HidNpadButton_R)
    result |= BK_BUTTON_CAMERA_ADJUST;
  if (raw & HidNpadButton_Minus)
    result |= BK_BUTTON_FPS;
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
  printf("Biko3 development preview\n\n%s\n\nPress B to return.\n",
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
  if (p->save_mounted) {
    Result rc = fsdevCommitDevice("save");
    if (R_FAILED(rc))
      fprintf(stderr, "Save data final commit failed: %08x\n", rc);
    fsdevUnmountDevice("save");
  }
  if (p->romfs_mounted)
    romfsUnmount("romfs");
#ifdef BK_SWITCH_FILE_LOG
  if (p->log)
    fclose(p->log);
#endif
  free(p);
}
