#ifndef BK_SAVE_CHECKPOINT_FILE_H
#define BK_SAVE_CHECKPOINT_FILE_H
#include "resource/store.h"
#include "save/checkpoint.h"
typedef struct BkCheckpointFiles BkCheckpointFiles;
/* Explicit writable port root, never inferred from the original resource
 * directory. Creates root/save; existing original .b3f files are not touched.
 * One app owner, single writer. No automatic import or corruption fallback. */
BkCheckpointFiles *bk_checkpoint_files_create(const char *root,
                                              char error[256]);
void bk_checkpoint_files_destroy(BkCheckpointFiles *);
/* Missing is distinct from corrupt/unreadable; output unchanged on either. */
BkResourceResult bk_checkpoint_file_read(BkCheckpointFiles *, unsigned group,
                                         BkCheckpointBank *, char error[256]);
/* Reads and validates existing bank, changes one slot, writes same-directory
 * temp, flushes+fsyncs+closes, then renames. Existing corrupt banks are
 * rejected. Failure before rename retains old file and caller result. Success
 * publishes the complete new bank. Directory fsync/power-loss durability is not
 * claimed. */
int bk_checkpoint_file_store(BkCheckpointFiles *, unsigned group, unsigned slot,
                             unsigned area, const uint8_t inventory[8],
                             const BkCheckpointTime *, int32_t random_value,
                             BkCheckpointBank *result, char error[256]);
#endif
