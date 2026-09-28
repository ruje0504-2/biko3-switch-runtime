#ifndef BK_SAVE_UNLOCK_FILE_H
#define BK_SAVE_UNLOCK_FILE_H
#include "resource/store.h"
#include "save/unlock.h"
typedef struct BkUnlockFile BkUnlockFile;
/* Own output_root/save/unlocks.bku, independently of checkpoint banks.
 * Missing returns MISSING and preserves the caller's table; boot may then
 * explicitly choose the empty default. Corrupt/short files are errors. */
BkUnlockFile *bk_unlock_file_create(const char *output_root, char error[256]);
void bk_unlock_file_destroy(BkUnlockFile *);
BkResourceResult bk_unlock_file_read(BkUnlockFile *, BkUnlockTable *,
                                     char error[256]);
/* Reread disk to preserve other rows, replace this group's eight bytes,
 * flush/fsync and install using the same rollback protocol as checkpoints.
 * Update caller's table only after successful installation. Not thread safe. */
int bk_unlock_file_store(BkUnlockFile *, unsigned group, const uint8_t flags[8],
                         BkUnlockTable *result, char error[256]);
#endif
