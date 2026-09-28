#define _POSIX_C_SOURCE 200809L
#include "save/checkpoint_file.h"
#include "save/file_replace_internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct BkCheckpointFiles {
  char root[1024];
};
static int fail(char *e, const char *s) {
  snprintf(e, 256, "checkpoint files: %s", s);
  return 0;
}
static int directory(const char *p, char *e) {
  struct stat st;
  if (!stat(p, &st))
    return S_ISDIR(st.st_mode) ? 1 : fail(e, "path is not a directory");
  if (errno == ENOENT && !mkdir(p, 0777))
    return 1;
  return fail(e, "cannot create directory");
}
BkCheckpointFiles *bk_checkpoint_files_create(const char *root, char e[256]) {
  if (!root || !*root || strlen(root) > 980) {
    fail(e, "invalid output root");
    return NULL;
  }
  BkCheckpointFiles *f = calloc(1, sizeof(*f));
  if (!f) {
    fail(e, "allocation failed");
    return NULL;
  }
  snprintf(f->root, sizeof(f->root), "%s/save", root);
  if (!directory(root, e) || !directory(f->root, e)) {
    free(f);
    return NULL;
  }
  return f;
}
void bk_checkpoint_files_destroy(BkCheckpointFiles *f) { free(f); }
static void path(const BkCheckpointFiles *f, unsigned group, char out[1100]) {
  snprintf(out, 1100, "%s/checkpoint-%u.bks", f->root, group);
}
BkResourceResult bk_checkpoint_file_read(BkCheckpointFiles *f, unsigned group,
                                         BkCheckpointBank *out, char e[256]) {
  if (!f || group >= 5 || !out) {
    fail(e, "invalid read");
    return BK_RESOURCE_ERROR;
  }
  char p[1100];
  path(f, group, p);
  if (!bk_save_file_recover(p, e))
    return BK_RESOURCE_ERROR;
  FILE *in = fopen(p, "rb");
  if (!in) {
    if (errno == ENOENT)
      return BK_RESOURCE_MISSING;
    fail(e, "cannot open bank");
    return BK_RESOURCE_ERROR;
  }
  uint8_t bytes[592];
  int ok = fread(bytes, 1, sizeof(bytes), in) == sizeof(bytes);
  if (fgetc(in) != EOF || ferror(in))
    ok = 0;
  if (fclose(in))
    ok = 0;
  if (!ok) {
    fail(e, "short/oversized/unreadable bank");
    return BK_RESOURCE_ERROR;
  }
  return bk_checkpoint_decode(out, group, bytes, sizeof(bytes), e)
             ? BK_RESOURCE_OK
             : BK_RESOURCE_ERROR;
}
int bk_checkpoint_file_store(BkCheckpointFiles *f, unsigned group,
                             unsigned slot, unsigned area, const uint8_t inv[8],
                             const BkCheckpointTime *t, int32_t random_value,
                             BkCheckpointBank *result, char e[256]) {
  if (!result)
    return fail(e, "missing result");
  BkCheckpointBank bank = {0};
  if (bk_checkpoint_file_read(f, group, &bank, e) == BK_RESOURCE_ERROR ||
      !bk_checkpoint_update(&bank, slot, area, inv, t, random_value, e))
    return 0;
  uint8_t bytes[592];
  if (!bk_checkpoint_encode(&bank, group, bytes, e))
    return 0;
  char p[1100], tmp[1110];
  path(f, group, p);
  snprintf(tmp, sizeof(tmp), "%s.part", p);
  FILE *out = fopen(tmp, "wb");
  if (!out)
    return fail(e, "cannot open temporary bank");
  int ok = fwrite(bytes, 1, sizeof(bytes), out) == sizeof(bytes);
  if (ok && fflush(out))
    ok = 0;
  if (ok && fsync(fileno(out)))
    ok = 0;
  if (fclose(out))
    ok = 0;
  if (!ok) {
    remove(tmp);
    return fail(e, "write/flush/close failed");
  }
  if (!bk_save_file_replace(tmp, p, e)) {
    remove(tmp);
    return 0;
  }
  *result = bank;
  return 1;
}
