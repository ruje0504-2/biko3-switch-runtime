#include "resource/message.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static int fail(char *error, const char *message) {
  snprintf(error, 256, "message: %s", message);
  return 0;
}
static int match(const uint8_t *raw, size_t size, size_t pos,
                 const void *pattern, size_t n) {
  return n <= size - pos && !memcmp(raw + pos, pattern, n);
}
static int clear(BkMessage *out) {
  memset(out->bytes, 0, sizeof(out->bytes));
  out->length = 0;
  return 1;
}
int bk_message_lookup(const void *data, size_t size, int32_t id, BkMessage *out,
                      char error[256]) {
  if (!data || !size || !out)
    return fail(error, "missing input/output");
  const uint8_t *raw = data;
  char key[16];
  snprintf(key, sizeof(key), "#%05" PRId32, id);
  size_t pos = 0;
  for (; pos < size; ++pos) {
    if (match(raw, size, pos, key, 6))
      break;
    if (match(raw, size, pos, "#end", 4))
      return clear(out);
  }
  if (pos == size)
    return fail(error, "key scan has no end marker");
  for (; pos < size && raw[pos] != '\n'; ++pos)
    if (match(raw, size, pos, "#end", 4))
      return clear(out);
  if (pos == size)
    return fail(error, "unterminated header");
  size_t start = ++pos, span = 0;
  uint32_t lines = 0;
  uint8_t closing = 0;
  for (; pos < size; ++pos) {
    if (raw[pos] == '\r')
      ++lines;
    if (size - pos >= 2 && raw[pos] == 0x81) {
      uint8_t next = raw[pos + 1];
      if (!closing && (next == 0x75 || next == 0x69 || next == 0x67))
        closing = next + 1;
      if (closing && next == closing) {
        span = pos - start + 2;
        break;
      }
    }
    if (raw[pos] == '#') {
      span = pos - start;
      break;
    }
    if (pos - start >= BK_MESSAGE_CAPACITY)
      return fail(error, "body exceeds output capacity");
  }
  if (pos == size)
    return fail(error, "unterminated body");
  if (span > BK_MESSAGE_CAPACITY)
    return fail(error, "body exceeds output capacity");
  /* Native strncpy pads after the first NUL, even though its delimiter scan
   * continued beyond that NUL and counted any subsequent CR bytes. */
  const uint8_t *nul = memchr(raw + start, 0, span);
  size_t length = nul ? (size_t)(nul - raw - start) : span;
  BkMessage next = {.length = length, .carriage_returns = lines};
  memcpy(next.bytes, raw + start, length);
  *out = next;
  return 1;
}
