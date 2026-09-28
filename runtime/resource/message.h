#ifndef BK_RESOURCE_MESSAGE_H
#define BK_RESOURCE_MESSAGE_H
#include <stddef.h>
#include <stdint.h>
#define BK_MESSAGE_CAPACITY 1024
typedef struct {
  uint8_t bytes[BK_MESSAGE_CAPACITY];
  size_t length;
  uint32_t carriage_returns;
} BkMessage;
/*51876a: bytewise, case-sensitive key/header/body scan of the original text.
 * The first six bytes of #%05d select a key, including negative/large IDs.
 * Stop after the first matching Shift-JIS closing quote or before '#'.
 * This returns raw text, not UTF-8. Length excludes strncpy's NUL padding;
 * a full1024-byte result need not have a terminating NUL.
 * Missing key/header ending at #end clears bytes/length but preserves the
 * previous CR count. A body counts CR, not LF. Malformed/truncated/oversized
 * input fails without changing out, instead of reading/writing out of bounds.
 */
int bk_message_lookup(const void *raw, size_t size, int32_t id, BkMessage *out,
                      char error[256]);
#endif
