#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "save/capture_file.h"
#include "save/checkpoint_file.h"
#include "save/file_commit.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned commits;
static int fail_commit, expect_present;
static int commit(const char *path, char error[256]) {
  struct stat st;
  int present = !stat(path, &st);
  assert(present == expect_present);
  if (present) {
    FILE *file = fopen(path, "rb");
    assert(file && fgetc(file) != EOF && !fclose(file));
  } else assert(errno == ENOENT);
  ++commits;
  if (fail_commit) {
    snprintf(error, 256, "injected HOS commit failure");
    return 0;
  }
  return 1;
}
int main(void) {
  char root[] = "/tmp/bk-save-commit-XXXXXX", error[256], path[1024], backup[1040];
  assert(mkdtemp(root));
  BkCaptureFiles *photos = bk_capture_files_create(root, error);
  BkCheckpointFiles *saves = bk_checkpoint_files_create(root, error);
  assert(photos && saves);
  bk_save_set_commit(commit);
  BkBlob image = {(uint8_t *)"photo", 5}, read = {0};
  expect_present = 1;
  assert(bk_capture_file_write(photos, 1, "ri_test.bmp", &image, error));
  assert(commits == 1);
  bk_capture_files_destroy(photos);
  photos = bk_capture_files_create(root, error);
  assert(photos && bk_capture_file_read_photo(photos, "ri_test.bmp", 5, &read, error) == BK_RESOURCE_OK);
  assert(!memcmp(read.data, image.data, 5) && commits == 1);
  bk_blob_free(&read);
  expect_present = 0;
  assert(bk_capture_file_remove_photo(photos, "ri_test.bmp", error) && commits == 2);
  expect_present = 1;
  fail_commit = 1;
  assert(!bk_capture_file_write(photos, 1, "ri_test.bmp", &image, error));
  assert(strstr(error, "commit failure") && commits == 3);
  fail_commit = 0;
  assert(bk_capture_file_write(photos, 0, "sy_99.bmp", &image, error) && commits == 4);
  snprintf(path, sizeof(path), "%s/sy_99.bmp", root);
  snprintf(backup, sizeof(backup), "%s.bak", path);
  assert(!rename(path, backup));
  assert(bk_capture_file_read_pause(photos, 5, &read, error) == BK_RESOURCE_OK && commits == 5);
  bk_blob_free(&read);
  expect_present = 0;
  assert(bk_capture_file_remove_pause(photos, error) && commits == 6);
  fail_commit = 1;
  assert(!bk_capture_file_remove_photo(photos, "ri_test.bmp", error) && commits == 7);
  assert(strstr(error, "commit failure"));
  fail_commit = 0;
  expect_present = 1;
  BkCheckpointBank bank = {0};
  BkCheckpointTime time = {2026, 10, 2, 1, 0, 0};
  uint8_t inventory[8] = {0};
  assert(bk_checkpoint_file_store(saves, 0, 0, 1, inventory, &time, 0, &bank, error));
  assert(commits == 8);
  bk_save_set_commit(NULL);
  assert(bk_capture_file_write(photos, 0, "sy_99.bmp", &image, error) && commits == 8);
  assert(bk_capture_file_remove_pause(photos, error));
  bk_capture_files_destroy(photos);
  bk_checkpoint_files_destroy(saves);
  snprintf(path, sizeof(path), "%s/save/checkpoint-0.bks", root); assert(!unlink(path));
  snprintf(path, sizeof(path), "%s/save", root); assert(!rmdir(path));
  snprintf(path, sizeof(path), "%s/album", root); assert(!rmdir(path));
  assert(!rmdir(root));
  puts("PASS save commit ordering: photo/reopen/delete, failure propagation, recovery, checkpoint and SD no-op");
}
