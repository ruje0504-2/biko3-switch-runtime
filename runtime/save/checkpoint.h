#ifndef BK_SAVE_CHECKPOINT_H
#define BK_SAVE_CHECKPOINT_H
#include <stddef.h>
#include <stdint.h>
#define BK_CHECKPOINT_GROUPS 5
#define BK_CHECKPOINT_SLOTS 10
#define BK_CHECKPOINT_NATIVE_BYTES 560
#define BK_CHECKPOINT_FILE_BYTES 592
/* Native509c25/509fd2 checkpoint records. Only area and eight inventory bytes
 * are gameplay state: this resumes an area entry, not an arbitrary live frame.
 * Explicit serialization never writes C padding, pointers or resource handles.
 * Opaque native bytes and timestamp suffix are retained across slot updates. */
typedef struct {
  uint32_t nonce, area;
  uint8_t inventory[8], opaque[8];
  char stamp[32];
} BkCheckpoint;
typedef struct {
  BkCheckpoint slots[BK_CHECKPOINT_SLOTS];
} BkCheckpointBank;
typedef struct {
  unsigned year, month, day, hour, minute, second;
} BkCheckpointTime;
/* Valid Gregorian time, exactly YYYY/MM/DD-HH:MM:SS. No tick suffix:4af3dc
 * computes that suffix but does not append it. Failure leaves out unchanged. */
int bk_checkpoint_stamp(char out[32], const BkCheckpointTime *);
/* Native signed remainder(rand,65536),19-byte stamp strcpy,area and inventory
 * stores. Does not consume RNG or time itself; owner supplies their values. */
int bk_checkpoint_update(BkCheckpointBank *, unsigned slot, unsigned area,
                         const uint8_t inventory[8], const BkCheckpointTime *,
                         int32_t random_value, char error[256]);
/* Explicit compatibility codec for five .b3f banks,50d245 xor0x31cb1.
 * No file I/O or automatic importing. Native format has no integrity checksum;
 * reject malformed lengths/occupied entries. Output is atomic on failure. */
int bk_checkpoint_native_encode(const BkCheckpointBank *, uint8_t out[560],
                                char error[256]);
int bk_checkpoint_native_decode(BkCheckpointBank *, const void *, size_t,
                                char error[256]);
/* Port envelope: magic BK3SAVE\0,version1,group,10 slots,payload length560,
 * CRC32(header[0..23]+payload),reserved0,then explicit unencrypted native
 * records. Group binding prevents a file rename from changing the character.
 * CRC detects accidental corruption, not malicious modification. */
int bk_checkpoint_encode(const BkCheckpointBank *, unsigned group,
                         uint8_t out[BK_CHECKPOINT_FILE_BYTES],
                         char error[256]);
int bk_checkpoint_decode(BkCheckpointBank *, unsigned group, const void *,
                         size_t, char error[256]);
#endif
