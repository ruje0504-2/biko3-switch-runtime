#define _POSIX_C_SOURCE 200809L
#include "save/record_file.h"
#include "save/file_replace_internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct BkRecordFile {
  char path[1100];
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "record file: %s", why);
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
BkRecordFile *bk_record_file_create(const char *root, char e[256]) {
  if (!root || !*root || strlen(root) > 980) {
    fail(e, "invalid output root");
    return NULL;
  }
  BkRecordFile *f = calloc(1, sizeof(*f));
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
  snprintf(f->path, sizeof(f->path), "%s/records.bkr", folder);
  return f;
}
void bk_record_file_destroy(BkRecordFile *f) { free(f); }
static BkResourceResult read_bytes(BkRecordFile *f, uint8_t *bytes, char e[256]) {
  if (!bk_save_file_recover(f->path, e)) return BK_RESOURCE_ERROR;
  FILE *in = fopen(f->path, "rb");
  if (!in) {
    if (errno == ENOENT) return BK_RESOURCE_MISSING;
    fail(e, "cannot open progress"); return BK_RESOURCE_ERROR;
  }
  int ok = fread(bytes, 1, BK_RECORD_FILE_BYTES, in) == BK_RECORD_FILE_BYTES;
  if (fgetc(in) != EOF || ferror(in)) ok = 0;
  if (fclose(in)) ok = 0;
  if (!ok) { fail(e, "short/oversized/unreadable progress"); return BK_RESOURCE_ERROR; }
  return bk_record_validate(bytes, BK_RECORD_FILE_BYTES, e) ? BK_RESOURCE_OK : BK_RESOURCE_ERROR;
}
BkResourceResult bk_record_file_read(BkRecordFile *f, const BkRecordView v[5], char e[256]) {
  if (!f || !v) { fail(e, "invalid read"); return BK_RESOURCE_ERROR; }
  uint8_t *bytes = malloc(BK_RECORD_FILE_BYTES);
  if (!bytes) { fail(e, "allocation failed"); return BK_RESOURCE_ERROR; }
  BkResourceResult r = read_bytes(f, bytes, e);
  if (r == BK_RESOURCE_OK && !bk_record_decode(v, bytes, BK_RECORD_FILE_BYTES, e)) r = BK_RESOURCE_ERROR;
  free(bytes); return r;
}
int bk_record_file_store(BkRecordFile *f, const BkRecordView v[5], char e[256]) {
  if (!f || !v) return fail(e, "invalid write");
  uint8_t *bytes = malloc(BK_RECORD_FILE_BYTES);
  if (!bytes) return fail(e, "allocation failed");
  int ok = read_bytes(f, bytes, e) != BK_RESOURCE_ERROR && bk_record_encode(v, bytes, e);
  if (!ok) { free(bytes); return 0; }
  char temporary[1120];
  snprintf(temporary, sizeof(temporary), "%s.part", f->path);
  FILE *file = fopen(temporary, "wb");
  if (!file) { free(bytes); return fail(e, "cannot open temporary progress"); }
  ok = fwrite(bytes, 1, BK_RECORD_FILE_BYTES, file) == BK_RECORD_FILE_BYTES;
  free(bytes);
  if (ok && fflush(file)) ok = 0;
  if (ok && fsync(fileno(file))) ok = 0;
  if (fclose(file)) ok = 0;
  if (!ok) { remove(temporary); return fail(e, "write/flush/close failed"); }
  if (!bk_save_file_replace(temporary, f->path, e)) { remove(temporary); return 0; }
  return 1;
}
