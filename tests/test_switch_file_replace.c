#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "save/capture_file.h"
#include "save/checkpoint_file.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/* This target alone redirects rename. Model fsdev's no-overwrite contract
 * and fault each step without changing production filesystem behavior. */
#undef rename
int rename(const char *, const char *);
static unsigned calls, fail_mask;
int bk_test_rename(const char *from, const char *to) {
  unsigned call = calls++;
  if (call < 32 && (fail_mask & (1u << call))) {
    errno = EIO;
    return -1;
  }
  struct stat st;
  if (!stat(to, &st)) {
    errno = EEXIST;
    return -1;
  }
  return rename(from, to);
}
static void reset(unsigned failures) {
  calls = 0;
  fail_mask = failures;
}
int main(void) {
  char root[] = "/tmp/bk-switch-save-XXXXXX", e[256], path[1024], backup[1040];
  assert(mkdtemp(root));
  BkCheckpointFiles *f = bk_checkpoint_files_create(root, e);
  assert(f);
  BkCheckpointBank bank = {0}, got, old;
  BkCheckpointTime time = {2026, 9, 28, 12, 34, 56};
  const uint8_t inventory[8] = {1, 0, 1, 0, 1, 2, 3, 4};
  for (unsigned i = 0; i < 100; ++i) {
    reset(0);
    assert(bk_checkpoint_file_store(f, i % 5, i % 10, i % 9, inventory, &time,
                                    i, &bank, e));
    assert(calls == (i < 5 ? 1 : 3));
    assert(bk_checkpoint_file_read(f, i % 5, &got, e) == BK_RESOURCE_OK);
    assert(!memcmp(&got, &bank, sizeof(bank)));
  }
  snprintf(path, sizeof(path), "%s/save/checkpoint-4.bks", root);
  snprintf(backup, sizeof(backup), "%s.bak", path);
  old = bank;
  /* Failure before preservation, at install (with rollback), and at install
   * plus rollback. Last case leaves .bak recoverable after process restart. */
  const unsigned failures[] = {1u, 2u, 4u, 4u | 8u};
  for (unsigned i = 0; i < 4; ++i) {
    reset(failures[i]);
    got = old;
    assert(
        !bk_checkpoint_file_store(f, 4, 0, 1, inventory, &time, 42, &got, e));
    assert(!memcmp(&got, &old, sizeof(got)));
    reset(0);
    bk_checkpoint_files_destroy(f);
    f = bk_checkpoint_files_create(root, e);
    assert(f && bk_checkpoint_file_read(f, 4, &got, e) == BK_RESOURCE_OK);
    assert(!memcmp(&got, &old, sizeof(got)));
  }
  /* Interrupted before installing new name: old .bak is authoritative. */
  assert(!rename(path, backup));
  assert(bk_checkpoint_file_read(f, 4, &got, e) == BK_RESOURCE_OK);
  assert(!memcmp(&got, &old, sizeof(got)));
  /* Interrupted after installing new name: new primary wins; subsequent
   * successful store safely retires stale .bak. */
  assert(!rename(path, backup));
  uint8_t encoded[592];
  assert(bk_checkpoint_encode(&old, 4, encoded, e));
  FILE *out = fopen(path, "wb");
  assert(out);
  assert(fwrite(encoded, 1, 592, out) == 592 && !fclose(out));
  reset(0);
  assert(bk_checkpoint_file_store(f, 4, 0, 2, inventory, &time, 42, &got, e));
  assert(access(backup, F_OK) && errno == ENOENT);
  /* Shared helper also fixes repeated pause screenshots on this filesystem. */
  BkCaptureFiles *capture = bk_capture_files_create(root, e);
  assert(capture);
  uint8_t content[16] = {0};
  BkBlob blob = {content, sizeof(content)}, read = {0};
  for (unsigned i = 0; i < 10; ++i) {
    content[0] = i;
    reset(0);
    assert(bk_capture_file_write(capture, 0, "sy_99.bmp", &blob, e));
    assert(bk_capture_file_read_pause(capture, 32, &read, e) == BK_RESOURCE_OK);
    assert(read.size == 16 && !memcmp(read.data, content, 16));
    bk_blob_free(&read);
  }
  assert(bk_capture_file_remove_pause(capture, e));
  bk_capture_files_destroy(capture);
  bk_checkpoint_files_destroy(f);
  for (unsigned i = 0; i < 5; ++i) {
    snprintf(path, sizeof(path), "%s/save/checkpoint-%u.bks", root, i);
    assert(!remove(path));
  }
  snprintf(path, sizeof(path), "%s/save", root);
  assert(!rmdir(path));
  snprintf(path, sizeof(path), "%s/album", root);
  assert(!rmdir(path));
  assert(!rmdir(root));
  puts("PASS Switch no-replace FS:100 saves,4 rename "
       "failures/rollback/restart,2 interruptions,10 pause captures");
}
