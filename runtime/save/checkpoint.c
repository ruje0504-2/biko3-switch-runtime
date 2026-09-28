#include "save/checkpoint.h"
#include <stdio.h>
#include <string.h>
static int fail(char *e, const char *s) {
  snprintf(e, 256, "checkpoint: %s", s);
  return 0;
}
static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; ++i)
    p[i] = (uint8_t)(n >> (i * 8));
}
static int valid_time(const BkCheckpointTime *t) {
  static const unsigned days[] = {31, 28, 31, 30, 31, 30,
                                  31, 31, 30, 31, 30, 31};
  if (!t || !t->year || t->year > 9999 || !t->month || t->month > 12 ||
      !t->day || t->hour > 23 || t->minute > 59 || t->second > 59)
    return 0;
  unsigned limit = days[t->month - 1];
  if (t->month == 2 && t->year % 4 == 0 &&
      (t->year % 100 != 0 || t->year % 400 == 0))
    ++limit;
  return t->day <= limit;
}
int bk_checkpoint_stamp(char out[32], const BkCheckpointTime *t) {
  if (!out || !valid_time(t))
    return 0;
  char tmp[32] = {0};
  snprintf(tmp, sizeof(tmp), "%04u/%02u/%02u-%02u:%02u:%02u", t->year, t->month,
           t->day, t->hour, t->minute, t->second);
  memcpy(out, tmp, sizeof(tmp));
  return 1;
}
static unsigned digits(const char *s, unsigned n) {
  unsigned v = 0;
  for (unsigned i = 0; i < n; ++i)
    v = v * 10 + (unsigned)(s[i] - '0');
  return v;
}
static int valid(const BkCheckpointBank *b) {
  if (!b)
    return 0;
  for (unsigned i = 0; i < BK_CHECKPOINT_SLOTS; ++i) {
    const BkCheckpoint *r = &b->slots[i];
    const char *s = r->stamp;
    /* Native presence is stamp[0], not nonce/area/inventory. */
    if (!s[0])
      continue;
    if (r->area > 8 || s[19] || s[4] != '/' || s[7] != '/' || s[10] != '-' ||
        s[13] != ':' || s[16] != ':')
      return 0;
    for (unsigned k = 0; k < 19; ++k)
      if (k != 4 && k != 7 && k != 10 && k != 13 && k != 16 &&
          (s[k] < '0' || s[k] > '9'))
        return 0;
    BkCheckpointTime t = {digits(s, 4),      digits(s + 5, 2),
                          digits(s + 8, 2),  digits(s + 11, 2),
                          digits(s + 14, 2), digits(s + 17, 2)};
    if (!valid_time(&t))
      return 0;
  }
  return 1;
}
int bk_checkpoint_update(BkCheckpointBank *b, unsigned slot, unsigned area,
                         const uint8_t inv[8], const BkCheckpointTime *t,
                         int32_t random_value, char e[256]) {
  char stamp[32];
  if (!b || slot >= BK_CHECKPOINT_SLOTS || area > 8 || !inv ||
      !bk_checkpoint_stamp(stamp, t) || !valid(b))
    return fail(e, "invalid slot/state/time");
  BkCheckpoint tmp = b->slots[slot];
  tmp.nonce = (uint32_t)(random_value % 65536);
  tmp.area = area;
  memcpy(tmp.inventory, inv, 8);
  memcpy(tmp.stamp, stamp, 20);
  b->slots[slot] = tmp;
  return 1;
}
static void records_write(const BkCheckpointBank *b, uint8_t *p) {
  for (unsigned i = 0; i < BK_CHECKPOINT_SLOTS; ++i, p += 56) {
    const BkCheckpoint *r = &b->slots[i];
    put32(p, r->nonce);
    put32(p + 4, r->area);
    memcpy(p + 8, r->inventory, 8);
    memcpy(p + 16, r->opaque, 8);
    memcpy(p + 24, r->stamp, 32);
  }
}
static int records_read(BkCheckpointBank *b, const uint8_t *p, char *e) {
  BkCheckpointBank tmp = {0};
  for (unsigned i = 0; i < BK_CHECKPOINT_SLOTS; ++i, p += 56) {
    BkCheckpoint *r = &tmp.slots[i];
    r->nonce = get32(p);
    r->area = get32(p + 4);
    memcpy(r->inventory, p + 8, 8);
    memcpy(r->opaque, p + 16, 8);
    memcpy(r->stamp, p + 24, 32);
  }
  if (!valid(&tmp))
    return fail(e, "malformed occupied record");
  *b = tmp;
  return 1;
}
static void native_xor(uint8_t *p) {
  for (unsigned i = 0; i < BK_CHECKPOINT_NATIVE_BYTES; i += 4)
    put32(p + i, get32(p + i) ^ 0x31cb1u);
}
int bk_checkpoint_native_encode(const BkCheckpointBank *b, uint8_t out[560],
                                char e[256]) {
  if (!out || !valid(b))
    return fail(e, "invalid native bank/output");
  uint8_t tmp[560];
  records_write(b, tmp);
  native_xor(tmp);
  memcpy(out, tmp, sizeof(tmp));
  return 1;
}
int bk_checkpoint_native_decode(BkCheckpointBank *b, const void *in, size_t n,
                                char e[256]) {
  if (!b || !in || n != 560)
    return fail(e, "invalid native length/input");
  uint8_t tmp[560];
  memcpy(tmp, in, 560);
  native_xor(tmp);
  return records_read(b, tmp, e);
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
  return ~crc32(crc32(0xffffffffu, p, 24), p + 32, 560);
}
int bk_checkpoint_encode(const BkCheckpointBank *b, unsigned group,
                         uint8_t out[592], char e[256]) {
  if (!out || group >= 5 || !valid(b))
    return fail(e, "invalid bank/group/output");
  uint8_t tmp[592] = {0};
  memcpy(tmp, "BK3SAVE", 8);
  put32(tmp + 8, 1);
  put32(tmp + 12, group);
  put32(tmp + 16, 10);
  put32(tmp + 20, 560);
  records_write(b, tmp + 32);
  put32(tmp + 24, checksum(tmp));
  memcpy(out, tmp, sizeof(tmp));
  return 1;
}
int bk_checkpoint_decode(BkCheckpointBank *b, unsigned group, const void *in,
                         size_t n, char e[256]) {
  if (!b || !in || n != 592 || group >= 5)
    return fail(e, "invalid file length/group/input");
  const uint8_t *p = in;
  if (memcmp(p, "BK3SAVE", 8) || get32(p + 8) != 1 || get32(p + 12) != group ||
      get32(p + 16) != 10 || get32(p + 20) != 560 || get32(p + 28) != 0)
    return fail(e, "unsupported file header/version/group");
  if (get32(p + 24) != checksum(p))
    return fail(e, "file checksum mismatch");
  return records_read(b, p + 32, e);
}
