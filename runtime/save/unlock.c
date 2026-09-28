#include "save/unlock.h"
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "unlock table: %s", why);
  return 0;
}
static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (i * 8));
}
int bk_unlock_update(BkUnlockTable *s, unsigned group, const uint8_t flags[8],
                     char e[256]) {
  if (!s || !flags || group >= BK_UNLOCK_GROUPS)
    return fail(e, "invalid table/group/row");
  memmove(s->flags[group], flags, BK_UNLOCK_FLAGS);
  return 1;
}
static void native_xor(uint8_t *p) {
  for (unsigned i = 0; i < BK_UNLOCK_NATIVE_BYTES; i += 4)
    put32(p + i, get32(p + i) ^ 0x31cb1u);
}
int bk_unlock_native_encode(const BkUnlockTable *s, uint8_t out[40],
                            char e[256]) {
  if (!s || !out)
    return fail(e, "missing native input/output");
  uint8_t tmp[40];
  memcpy(tmp, s->flags, sizeof(tmp));
  native_xor(tmp);
  memcpy(out, tmp, sizeof(tmp));
  return 1;
}
int bk_unlock_native_decode(BkUnlockTable *s, const void *in, size_t n,
                            char e[256]) {
  if (!s || !in || n != BK_UNLOCK_NATIVE_BYTES)
    return fail(e, "invalid native input/length");
  uint8_t tmp[40];
  memcpy(tmp, in, sizeof(tmp));
  native_xor(tmp);
  memcpy(s->flags, tmp, sizeof(tmp));
  return 1;
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
  return ~crc32(crc32(0xffffffffu, p, 24), p + 32, 40);
}
int bk_unlock_encode(const BkUnlockTable *s, uint8_t out[72], char e[256]) {
  if (!s || !out)
    return fail(e, "missing port input/output");
  uint8_t tmp[72] = {0};
  memcpy(tmp, "BK3UNLK", 8);
  put32(tmp + 8, 1);
  put32(tmp + 12, BK_UNLOCK_GROUPS);
  put32(tmp + 16, BK_UNLOCK_FLAGS);
  put32(tmp + 20, BK_UNLOCK_NATIVE_BYTES);
  memcpy(tmp + 32, s->flags, 40);
  put32(tmp + 24, checksum(tmp));
  memcpy(out, tmp, sizeof(tmp));
  return 1;
}
int bk_unlock_decode(BkUnlockTable *s, const void *in, size_t n, char e[256]) {
  if (!s || !in || n != BK_UNLOCK_FILE_BYTES)
    return fail(e, "invalid port input/length");
  const uint8_t *p = in;
  if (memcmp(p, "BK3UNLK", 8) || get32(p + 8) != 1 ||
      get32(p + 12) != BK_UNLOCK_GROUPS || get32(p + 16) != BK_UNLOCK_FLAGS ||
      get32(p + 20) != BK_UNLOCK_NATIVE_BYTES || get32(p + 28))
    return fail(e, "unsupported header/version/dimensions");
  if (get32(p + 24) != checksum(p))
    return fail(e, "checksum mismatch");
  memmove(s->flags, p + 32, 40);
  return 1;
}
