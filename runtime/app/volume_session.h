#ifndef BK_APP_VOLUME_SESSION_H
#define BK_APP_VOLUME_SESSION_H
#include "scene/scene.h"
#include "scene/curtain_render.h"
#include "save/volume_file.h"
typedef struct BkVolumeSession BkVolumeSession;
typedef struct {
  BkSceneServices services;
  BkVolumeFile *file;
  BkViewport viewport;
  BkCommonHudState *common;
  BkCurtainRender *curtain;
  uint32_t *random;
  uint8_t previous;
  float pointer[2];
  void *context;
  int (*schedule)(void *, uint8_t target, uint8_t mode, char[256]);
} BkVolumeSessionConfig;
BkVolumeSession *bk_volume_session_create(const BkVolumeSessionConfig *, char[256]);
void bk_volume_session_destroy(BkVolumeSession *);
/* Logical stop silences private samples; GPU snapshot survives until destroy. */
int bk_volume_session_stop(BkVolumeSession *, char[256]);
int bk_volume_session_step(BkVolumeSession *, double seconds, const BkInput *, char[256]);
int bk_volume_session_draw(BkVolumeSession *, char[256]);
const BkVirtualPointer *bk_volume_session_pointer(const BkVolumeSession *);
#endif
