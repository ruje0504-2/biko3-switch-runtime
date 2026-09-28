#define _POSIX_C_SOURCE 200809L
#include "save/unlock_file.h"
#include "save/file_replace_internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct BkUnlockFile {
  char path[1100];
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "unlock file: %s", why);
  return 0;
}
static int directory(const char *p, char e[256]) {
  struct stat st;
  if (!stat(p, &st))
    return S_ISDIR(st.st_mode) ? 1 : fail(e, "path is not a directory");
  if (errno == ENOENT && !mkdir(p, 0777))
    return 1;
  return fail(e, "cannot create directory");
}
BkUnlockFile *bk_unlock_file_create(const char *root, char e[256]) {
  if (!root || !*root || strlen(root) > 980) {
    fail(e, "invalid output root");
    return NULL;
  }
  BkUnlockFile *f = calloc(1, sizeof(*f));
  if (!f) {
    fail(e, "allocation failed");
    return NULL;
  }
  char folder[1024];
  snprintf(folder, sizeof(folder), "%s/save", root);
  if (!directory(root, e) || !directory(folder, e)) {
    free(f);
    return NULL;
  }
  snprintf(f->path, sizeof(f->path), "%s/unlocks.bku", folder);
  return f;
}
void bk_unlock_file_destroy(BkUnlockFile *f) { free(f); }
BkResourceResult bk_unlock_file_read(BkUnlockFile *f, BkUnlockTable *out,
                                     char e[256]) {
  if (!f || !out) {
    fail(e, "invalid read");
    return BK_RESOURCE_ERROR;
  }
  if (!bk_save_file_recover(f->path, e))
    return BK_RESOURCE_ERROR;
  FILE *in = fopen(f->path, "rb");
  if (!in) {
    if (errno == ENOENT)
      return BK_RESOURCE_MISSING;
    fail(e, "cannot open progress");
    return BK_RESOURCE_ERROR;
  }
  uint8_t bytes[BK_UNLOCK_FILE_BYTES];
  int ok = fread(bytes, 1, sizeof(bytes), in) == sizeof(bytes);
  if (fgetc(in) != EOF || ferror(in))
    ok = 0;
  if (fclose(in))
    ok = 0;
  if (!ok) {
    fail(e, "short/oversized/unreadable progress");
    return BK_RESOURCE_ERROR;
  }
  return bk_unlock_decode(out, bytes, sizeof(bytes), e) ? BK_RESOURCE_OK
                                                        : BK_RESOURCE_ERROR;
}
int bk_unlock_file_store(BkUnlockFile *f, unsigned group,
                         const uint8_t flags[8], BkUnlockTable *out,
                         char e[256]) {
  if (!f || !out || !flags || group >= BK_UNLOCK_GROUPS)
    return fail(e, "invalid write/group/row");
  BkUnlockTable next = {0};
  if (bk_unlock_file_read(f, &next, e) == BK_RESOURCE_ERROR ||
      !bk_unlock_update(&next, group, flags, e))
    return 0;
  uint8_t bytes[BK_UNLOCK_FILE_BYTES];
  if (!bk_unlock_encode(&next, bytes, e))
    return 0;
  char temporary[1120];
  snprintf(temporary, sizeof(temporary), "%s.part", f->path);
  FILE *file = fopen(temporary, "wb");
  if (!file)
    return fail(e, "cannot open temporary progress");
  int ok = fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes);
  if (ok && fflush(file))
    ok = 0;
  if (ok && fsync(fileno(file)))
    ok = 0;
  if (fclose(file))
    ok = 0;
  if (!ok) {
    remove(temporary);
    return fail(e, "write/flush/close failed");
  }
  if (!bk_save_file_replace(temporary, f->path, e)) {
    remove(temporary);
    return 0;
  }
  *out = next;
  return 1;
}
