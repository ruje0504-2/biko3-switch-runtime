#ifndef BK_RESOURCE_FACE_CONFIG_H
#define BK_RESOURCE_FACE_CONFIG_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
  char target[256], source[256], selection[256];
} BkFaceSlot;
typedef struct {
  uint32_t texture_mode;
  /* Row0 is metadata; native 0x4a6289 uses the caller's actor instead. */
  char actor_clip[256], source_clip[256];
  char eye_materials[2][256], eye_textures[2][256];
  uint32_t counts[2];
  BkFaceSlot slots[2][4];
} BkFaceConfig;
/* Fresh configuration, unlike native's accumulating output counters.
 * Four slots/group, stopping at the first empty target. Length-prefixed
 * strings may contain bytes after NUL. '-' remains a literal filename.
 * Unsafe/truncated strings fail without changing out; trailing data ignored. */
int bk_face_config_decode(const uint8_t *data, size_t size, BkFaceConfig *out,
                          char error[256]);
#endif
