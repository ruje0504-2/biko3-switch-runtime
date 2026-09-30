#include "save/record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending record storage: %s", why); return 0;
}
static int views(const BkRecordView *v, char e[256]) {
  if (!v) return fail(e, "missing views");
  for (unsigned g = 0; g < BK_RECORD_GROUPS; ++g)
    if (!v[g].retained[0] || !v[g].retained[1] || !v[g].actions || !v[g].count)
      return fail(e, "missing record lane/count");
  return 1;
}
static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (i * 8));
}
static void pack(const BkRecordView *v, uint8_t *p, uint32_t mask) {
  for (unsigned g = 0; g < BK_RECORD_GROUPS; ++g) {
    for (unsigned lane = 0; lane < 3; ++lane)
      for (unsigned i = 0; i < BK_RECORD_CAPACITY; ++i, p += 4) {
        uint32_t word;
        const void *src = lane < 2 ? (void *)&v[g].retained[lane][i]
                                   : (void *)&v[g].actions[i];
        memcpy(&word, src, 4); put32(p, word ^ mask);
      }
    uint32_t word; memcpy(&word, v[g].count, 4); put32(p, word ^ mask); p += 4;
  }
}
static void unpack(const BkRecordView *v, const uint8_t *p, uint32_t mask) {
  for (unsigned g = 0; g < BK_RECORD_GROUPS; ++g) {
    for (unsigned lane = 0; lane < 3; ++lane)
      for (unsigned i = 0; i < BK_RECORD_CAPACITY; ++i, p += 4) {
        uint32_t word = get32(p) ^ mask;
        void *dst = lane < 2 ? (void *)&v[g].retained[lane][i]
                             : (void *)&v[g].actions[i];
        memcpy(dst, &word, 4);
      }
    uint32_t word = get32(p) ^ mask; memcpy(v[g].count, &word, 4); p += 4;
  }
}
static uint32_t crc32(uint32_t crc, const uint8_t *p, size_t n) {
  while (n--) {
    crc ^= *p++;
    for (unsigned j = 0; j < 8; ++j)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return crc;
}
static uint32_t checksum(const uint8_t *p) {
  return ~crc32(crc32(0xffffffffu, p, 24), p + 32, BK_RECORD_NATIVE_BYTES);
}
int bk_record_validate(const void *in, size_t n, char e[256]) {
  if (!in || n != BK_RECORD_FILE_BYTES) return fail(e, "invalid file length");
  const uint8_t *p = in;
  if (memcmp(p, "BK3RECD", 8) || get32(p + 8) != 1 ||
      get32(p + 12) != BK_RECORD_GROUPS || get32(p + 16) != BK_RECORD_CAPACITY ||
      get32(p + 20) != BK_RECORD_NATIVE_BYTES || get32(p + 28))
    return fail(e, "unsupported header/version/dimensions");
  return get32(p + 24) == checksum(p) ? 1 : fail(e, "checksum mismatch");
}
static int encode(const BkRecordView *v, void *out, int native, char e[256]) {
  if (!views(v, e)) return 0;
  if (!out) return fail(e, "missing output");
  size_t n = native ? BK_RECORD_NATIVE_BYTES : BK_RECORD_FILE_BYTES;
  uint8_t *tmp = calloc(1, n);
  if (!tmp) return fail(e, "allocation failed");
  pack(v, tmp + (native ? 0 : 32), native ? 0x31cb1u : 0);
  if (!native) {
    memcpy(tmp, "BK3RECD", 8); put32(tmp + 8, 1);
    put32(tmp + 12, BK_RECORD_GROUPS); put32(tmp + 16, BK_RECORD_CAPACITY);
    put32(tmp + 20, BK_RECORD_NATIVE_BYTES); put32(tmp + 24, checksum(tmp));
  }
  memcpy(out, tmp, n); free(tmp); return 1;
}
static int decode(const BkRecordView *v, const void *in, size_t n,
                  int native, char e[256]) {
  if (!views(v, e)) return 0;
  if (!in || n != (size_t)(native ? BK_RECORD_NATIVE_BYTES : BK_RECORD_FILE_BYTES))
    return fail(e, "invalid input/length");
  if (!native && !bk_record_validate(in, n, e)) return 0;
  uint8_t *tmp = malloc(BK_RECORD_NATIVE_BYTES);
  if (!tmp) return fail(e, "allocation failed");
  memcpy(tmp, (const uint8_t *)in + (native ? 0 : 32), BK_RECORD_NATIVE_BYTES);
  unpack(v, tmp, native ? 0x31cb1u : 0); free(tmp); return 1;
}
int bk_record_native_encode(const BkRecordView v[5], void *out, char e[256]) {
  return encode(v, out, 1, e);
}
int bk_record_native_decode(const BkRecordView v[5], const void *in, size_t n, char e[256]) {
  return decode(v, in, n, 1, e);
}
int bk_record_encode(const BkRecordView v[5], void *out, char e[256]) {
  return encode(v, out, 0, e);
}
int bk_record_decode(const BkRecordView v[5], const void *in, size_t n, char e[256]) {
  return decode(v, in, n, 0, e);
}
