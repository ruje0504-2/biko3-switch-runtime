#ifndef BK_SAVE_RECORD_FILE_H
#define BK_SAVE_RECORD_FILE_H
#include "resource/store.h"
#include "save/record.h"
typedef struct BkRecordFile BkRecordFile;
/* Own root/save/records.bkr; independent of unlocks and checkpoints.
 * Missing preserves caller state; corrupt/short files are explicit errors.
 * Store snapshots all five process groups as native50CAA2 does, validates
 * existing progress, then flushes/fsyncs and uses save's rollback replacement.
 * Single process owner; no cross-process merging or power-loss guarantee. */
BkRecordFile *bk_record_file_create(const char *output_root, char error[256]);
void bk_record_file_destroy(BkRecordFile *);
BkResourceResult bk_record_file_read(BkRecordFile *, const BkRecordView[5], char[256]);
int bk_record_file_store(BkRecordFile *, const BkRecordView[5], char[256]);
#endif
