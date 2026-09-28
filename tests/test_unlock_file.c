#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "save/unlock_file.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef BK_TEST_NO_REPLACE
#undef rename
int rename(const char *, const char *);
static unsigned calls, fail_mask;
int bk_test_rename(const char *a, const char *b) {
  unsigned call = calls++;
  if (call < 32 && (fail_mask & (1u << call))) {
    errno = EIO;
    return -1;
  }
  struct stat st;
  if (!stat(b, &st)) {
    errno = EEXIST;
    return -1;
  }
  return rename(a, b);
}
static void reset(unsigned faults) {
  calls = 0;
  fail_mask = faults;
}
#endif
static void write_bytes(const char *p, const void *data, size_t n) {
  FILE *f = fopen(p, "wb");
  assert(f && fwrite(data, 1, n, f) == n && !fclose(f));
}
static void same(const BkUnlockTable *a, const BkUnlockTable *b) {
  assert(!memcmp(a, b, sizeof(*a)));
}
int main(void) {
  char e[256], root[] = "/tmp/bk-unlocks-XXXXXX", path[1100], backup[1120],
               part[1120];
  assert(mkdtemp(root));
  BkUnlockFile *f = bk_unlock_file_create(root, e);
  assert(f);
  snprintf(path, sizeof(path), "%s/save/unlocks.bku", root);
  snprintf(backup, sizeof(backup), "%s.bak", path);
  snprintf(part, sizeof(part), "%s.part", path);
  BkUnlockTable old, got, expected = {0};
  memset(&old, 0xd7, sizeof(old));
  got = old;
  assert(bk_unlock_file_read(f, &got, e) == BK_RESOURCE_MISSING);
  same(&got, &old);
  uint8_t row[8], bytes[73], native[40];
  for (unsigned i = 0; i < 100; i++) {
    for (unsigned j = 0; j < 8; j++)
      row[j] = (uint8_t)(i * 31 + j * 67);
#ifdef BK_TEST_NO_REPLACE
    reset(0);
#endif
    assert(bk_unlock_file_store(f, i % 5, row, &got, e));
#ifdef BK_TEST_NO_REPLACE
    assert(calls == (i == 0 ? 1 : 3));
#endif
    memcpy(expected.flags[i % 5], row, 8);
    same(&got, &expected);
    assert(bk_unlock_file_read(f, &got, e) == BK_RESOURCE_OK);
    same(&got, &expected);
  }
  /* A second owner may have written another row. Never erase it with a
   * caller's stale in-memory snapshot; real native app is single-process. */
  BkUnlockFile *other = bk_unlock_file_create(root, e);
  assert(other);
  memset(row, 1, 8);
  assert(bk_unlock_file_store(other, 0, row, &got, e));
  memcpy(expected.flags[0], row, 8);
  memset(row, 0, 8);
  got = old;
  assert(bk_unlock_file_store(f, 1, row, &got, e));
  memcpy(expected.flags[1], row, 8);
  same(&got, &expected);
  bk_unlock_file_destroy(other);
  /* An alias into the output is allowed and must be captured before commit. */
  assert(bk_unlock_file_store(f, 3, got.flags[0], &got, e));
  memcpy(expected.flags[3], expected.flags[0], 8);
  same(&got, &expected);
  assert(bk_unlock_native_encode(&got, native, e));
  got = old;
  assert(bk_unlock_native_decode(&got, native, 40, e));
  same(&got, &expected);
  assert(bk_unlock_encode(&expected, bytes, e));
  old = expected;
  for (unsigned i = 0; i < 72; ++i) {
    bytes[i] ^= 0x80;
    assert(!bk_unlock_decode(&got, bytes, 72, e));
    same(&got, &old);
    bytes[i] ^= 0x80;
  }
  /* Overlap and invalid output/group/length are checked before mutation. */
  uint8_t overlap[72];
  memcpy(overlap, &expected, 40);
  assert(bk_unlock_encode((BkUnlockTable *)overlap, overlap, e));
  assert(bk_unlock_decode((BkUnlockTable *)overlap, overlap, 72, e));
  assert(!memcmp(overlap, &expected, 40));
  assert(!bk_unlock_update(&got, 5, row, e));
  assert(!bk_unlock_file_store(f, 5, row, &got, e));
  assert(!bk_unlock_file_store(f, 0, NULL, &got, e));
  assert(!bk_unlock_native_decode(&got, native, 39, e));
  same(&got, &old);
  /* Invalid files are not replaced by a fresh empty table. */
  bytes[35] ^= 1;
  write_bytes(path, bytes, 72);
  assert(bk_unlock_file_read(f, &got, e) == BK_RESOURCE_ERROR);
  assert(!bk_unlock_file_store(f, 0, row, &got, e));
  same(&got, &old);
  bytes[35] ^= 1;
  write_bytes(path, bytes, 71);
  assert(bk_unlock_file_read(f, &got, e) == BK_RESOURCE_ERROR);
  bytes[72] = 0;
  write_bytes(path, bytes, 73);
  assert(bk_unlock_file_read(f, &got, e) == BK_RESOURCE_ERROR);
  same(&got, &old);
  write_bytes(path, bytes, 72);
  /* Failure opening temporary output leaves primary and caller intact. */
  assert(!mkdir(part, 0777));
  assert(!bk_unlock_file_store(f, 2, row, &got, e));
  same(&got, &old);
  assert(!rmdir(part));
#ifdef BK_TEST_NO_REPLACE
  const unsigned faults[] = {1u, 2u, 4u, 4u | 8u};
  for (unsigned i = 0; i < 4; i++) {
    reset(faults[i]);
    assert(!bk_unlock_file_store(f, 2, row, &got, e));
    same(&got, &old);
    reset(0);
    bk_unlock_file_destroy(f);
    f = bk_unlock_file_create(root, e);
    assert(f && bk_unlock_file_read(f, &got, e) == BK_RESOURCE_OK);
    same(&got, &old);
  }
#endif
  assert(!rename(path, backup));
  assert(bk_unlock_file_read(f, &got, e) == BK_RESOURCE_OK);
  same(&got, &old);
  assert(!rename(path, backup));
  write_bytes(path, bytes, 72);
#ifdef BK_TEST_NO_REPLACE
  reset(0);
#endif
  assert(bk_unlock_file_store(f, 2, row, &got, e));
  assert(access(backup, F_OK) && errno == ENOENT);
  assert(access(part, F_OK) && errno == ENOENT);
  bk_unlock_file_destroy(f);
  assert(!remove(path));
  snprintf(path, sizeof(path), "%s/save", root);
  assert(!rmdir(path) && !rmdir(root));
  puts("PASS unlock storage:100 row writes,other rows/byte values "
       "retained,CRC72 mutations,missing/corrupt/size/alias,temporary "
       "failure,2 restart points");
#ifdef BK_TEST_NO_REPLACE
  puts("PASS unlock Switch no-overwrite FS:4 injected rename failures/restart");
#endif
}
