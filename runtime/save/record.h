#ifndef BK_SAVE_RECORD_H
#define BK_SAVE_RECORD_H
#include <stddef.h>
#include <stdint.h>
enum {
  BK_RECORD_GROUPS = 5, BK_RECORD_CAPACITY = 10000,
  BK_RECORD_NATIVE_BYTES = 600020, BK_RECORD_FILE_BYTES = 600052
};
/* Borrowed views of the process records. Save has no dependency on game.
 * All five groups, both retained lanes, unused actions and signed count bit
 * patterns are preserved; no sentinel, action or unlock is manufactured. */
typedef struct {
  uint32_t *retained[2];
  int32_t *actions, *count;
} BkRecordView;
/* Explicit in-memory compatibility with bk3_Gray.b3f /50CAA2/50CC47.
 * No automatic import. Heap scratch (never a 600KB stack frame) permits
 * overlapping input/output and preserves outputs on allocation/size failure. */
int bk_record_native_encode(const BkRecordView[5], void *, char error[256]);
int bk_record_native_decode(const BkRecordView[5], const void *, size_t,
                            char error[256]);
/* Port format:32-byte version/dimensions/CRC header + little-endian words.
 * Native invalid counts remain data; game consumers enforce their bounds. */
int bk_record_encode(const BkRecordView[5], void *, char error[256]);
int bk_record_decode(const BkRecordView[5], const void *, size_t, char error[256]);
int bk_record_validate(const void *, size_t, char error[256]);
#endif
