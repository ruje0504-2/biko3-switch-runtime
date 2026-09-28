#ifndef BK_SAVE_UNLOCK_H
#define BK_SAVE_UNLOCK_H
#include <stddef.h>
#include <stdint.h>
enum {
  BK_UNLOCK_GROUPS = 5,
  BK_UNLOCK_FLAGS = 8,
  BK_UNLOCK_NATIVE_BYTES = 40,
  BK_UNLOCK_FILE_BYTES = 72
};
/* B54738: saved gallery flags. The separate721dc6 working copy is made at
 * ending entry50ca48; it is NOT checkpoint inventory/route progress. Retain
 * all byte values, since native selection tests nonzero rather than==1. */
typedef struct {
  uint8_t flags[BK_UNLOCK_GROUPS][BK_UNLOCK_FLAGS];
} BkUnlockTable;
/*50c6bc replaces exactly one row; it does not OR flags or normalize bytes. */
int bk_unlock_update(BkUnlockTable *, unsigned group, const uint8_t flags[8],
                     char error[256]);
/* Explicit compatibility with40-byte bk3_Yellow.b3f /50d245 xor. No automatic
 * import or write into the original game directory. */
int bk_unlock_native_encode(const BkUnlockTable *, uint8_t out[40],
                            char error[256]);
int bk_unlock_native_decode(BkUnlockTable *, const void *, size_t,
                            char error[256]);
/* Port file: versioned header, dimensions, CRC32 and unmodified40-byte table.
 * Failure leaves outputs untouched; supports overlapping input/output. */
int bk_unlock_encode(const BkUnlockTable *, uint8_t out[72], char error[256]);
int bk_unlock_decode(BkUnlockTable *, const void *, size_t, char error[256]);
#endif
