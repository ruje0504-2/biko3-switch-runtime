#ifndef BK_SAVE_VOLUME_FILE_H
#define BK_SAVE_VOLUME_FILE_H
#include <stdint.h>
typedef struct BkVolumeFile BkVolumeFile;
/* Owns three process values: voice/BGM/effect. Reads port_root/save/volume.cfg;
 * missing settings start all three at MAX (0 dB). The original game config
 * is not imported. NULL root omits storage and saving fails. The port format
 * is12 little-endian bytes. Existing saved values are preserved. */
BkVolumeFile *bk_volume_file_create(const char *port_root, char error[256]);
void bk_volume_file_destroy(BkVolumeFile *);
/* Stable borrowed address until destroy. Only a successful store changes it. */
const int32_t *bk_volume_file_values(const BkVolumeFile *);
int bk_volume_file_store(BkVolumeFile *, const int32_t values[3], char error[256]);
#endif
