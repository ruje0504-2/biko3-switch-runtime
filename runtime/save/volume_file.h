#ifndef BK_SAVE_VOLUME_FILE_H
#define BK_SAVE_VOLUME_FILE_H
#include <stdint.h>
typedef struct BkVolumeFile BkVolumeFile;
/* Owns three process values: voice/BGM/effect. Reads port_root/save/volume.cfg
 * first, otherwise read-only original game_root/Data/volsetting.cfg. Missing
 * files use the original zero initial values. NULL roots omit that path;
 * without port_root saving fails. Both formats are12 little-endian bytes. */
BkVolumeFile *bk_volume_file_create(const char *port_root, const char *game_root,
                                    char error[256]);
void bk_volume_file_destroy(BkVolumeFile *);
/* Stable borrowed address until destroy. Only a successful store changes it. */
const int32_t *bk_volume_file_values(const BkVolumeFile *);
int bk_volume_file_store(BkVolumeFile *, const int32_t values[3], char error[256]);
#endif
