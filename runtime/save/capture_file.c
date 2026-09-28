#include "save/capture_file.h"
#include "save/file_replace_internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct BkCaptureFiles {
  char root[1024];
};
static int directory(const char *p, char *e) {
  struct stat st;
  if (!stat(p, &st)) {
    if (S_ISDIR(st.st_mode))
      return 1;
    snprintf(e, 256, "capture files: path is not a directory");
    return 0;
  }
  if (errno == ENOENT && !mkdir(p, 0777))
    return 1;
  snprintf(e, 256, "capture files: create directory: %s", strerror(errno));
  return 0;
}
BkCaptureFiles *bk_capture_files_create(const char *root, char e[256]) {
  if (!root || !*root || strlen(root) > 980) {
    snprintf(e, 256, "capture files: invalid root");
    return NULL;
  }
  BkCaptureFiles *f = calloc(1, sizeof(*f));
  if (!f) {
    snprintf(e, 256, "capture files: allocation failed");
    return NULL;
  }
  snprintf(f->root, sizeof(f->root), "%s", root);
  char album[1024];
  snprintf(album, sizeof(album), "%s/album", root);
  if (!directory(root, e) || !directory(album, e)) {
    free(f);
    return NULL;
  }
  return f;
}
void bk_capture_files_destroy(BkCaptureFiles *f) { free(f); }
int bk_capture_photo_name(char out[128], unsigned group,
                          const BkCaptureTime *t) {
  if (!out || !t || group >= 5 || t->year < 1 || t->year > 9999 ||
      t->month < 1 || t->month > 12 || t->day < 1 || t->day > 31 ||
      t->hour > 23 || t->minute > 59 || t->second > 59)
    return 0;
  static const char *const prefix[] = {"ri_", "re_", "cr_", "ma_", "mi_"};
  int64_t signed_ticks = t->ticks_ms >= 0x80000000u
                             ? (int64_t)t->ticks_ms - 4294967296LL
                             : t->ticks_ms;
  int remainder = (int)(signed_ticks % 100), tens = remainder / 10,
      ones = remainder - tens * 10;
  snprintf(out, 128, "%s%04u_%02u%02u_%02u%02u_%02u%d%d.bmp", prefix[group],
           t->year, t->month, t->day, t->hour, t->minute, t->second, tens,
           ones);
  return 1;
}
int bk_capture_file_write(BkCaptureFiles *f, int photo, const char *name,
                          const BkBlob *b, char e[256]) {
  if (!f || !name || !*name || strlen(name) > 127 || !b || !b->data ||
      !b->size || strchr(name, '/') || strchr(name, '\\') ||
      strchr(name, ':') || strstr(name, "..") ||
      (!photo && strcmp(name, "sy_99.bmp"))) {
    snprintf(e, 256, "capture files: invalid destination/blob");
    return 0;
  }
  char path[1280], tmp[1288];
  snprintf(path, sizeof(path), "%s/%s%s", f->root, photo ? "album/" : "", name);
  snprintf(tmp, sizeof(tmp), "%s.part", path);
  FILE *out = fopen(tmp, "wb");
  if (!out) {
    snprintf(e, 256, "capture files: open: %s", strerror(errno));
    return 0;
  }
  int ok = fwrite(b->data, 1, b->size, out) == b->size;
  if (fclose(out))
    ok = 0;
  if (!ok) {
    remove(tmp);
    snprintf(e, 256, "capture files: write failed");
    return 0;
  }
  if (!bk_save_file_replace(tmp, path, e)) {
    remove(tmp);
    return 0;
  }
  return 1;
}
BkResourceResult bk_capture_file_read_pause(BkCaptureFiles *f, size_t limit,
                                            BkBlob *out, char e[256]) {
  if (!f || !limit || !out || out->data || out->size) {
    snprintf(e, 256, "capture files: invalid read/output/limit");
    return BK_RESOURCE_ERROR;
  }
  char path[1200];
  if (snprintf(path, sizeof(path), "%s/sy_99.bmp", f->root) >=
      (int)sizeof(path)) {
    snprintf(e, 256, "capture files: pause path too long");
    return BK_RESOURCE_ERROR;
  }
  if (!bk_save_file_recover(path, e))
    return BK_RESOURCE_ERROR;
  FILE *in = fopen(path, "rb");
  if (!in) {
    if (errno == ENOENT)
      return BK_RESOURCE_MISSING;
    snprintf(e, 256, "capture files: read open: %s", strerror(errno));
    return BK_RESOURCE_ERROR;
  }
  long size = -1;
  if (!fseek(in, 0, SEEK_END))
    size = ftell(in);
  if (size <= 0 || (uintmax_t)size > limit || fseek(in, 0, SEEK_SET)) {
    fclose(in);
    snprintf(e, 256, "capture files: empty, oversized or unreadable pause");
    return BK_RESOURCE_ERROR;
  }
  BkBlob b = {malloc((size_t)size), (size_t)size};
  if (!b.data) {
    fclose(in);
    snprintf(e, 256, "capture files: allocation failed");
    return BK_RESOURCE_ERROR;
  }
  int ok = fread(b.data, 1, b.size, in) == b.size;
  if (fgetc(in) != EOF || ferror(in))
    ok = 0;
  if (fclose(in))
    ok = 0;
  if (!ok) {
    bk_blob_free(&b);
    snprintf(e, 256, "capture files: pause read failed/changed");
    return BK_RESOURCE_ERROR;
  }
  *out = b;
  return BK_RESOURCE_OK;
}
int bk_capture_file_remove_pause(BkCaptureFiles *f, char e[256]) {
  if (!f) {
    snprintf(e, 256, "capture files: missing output root");
    return 0;
  }
  char path[1200];
  if (snprintf(path, sizeof(path), "%s/sy_99.bmp", f->root) >=
      (int)sizeof(path)) {
    snprintf(e, 256, "capture files: pause path too long");
    return 0;
  }
  char backup[1210];
  snprintf(backup, sizeof(backup), "%s.bak", path);
  if (unlink(backup) && errno != ENOENT) {
    snprintf(e, 256, "capture files: remove pause backup: %s", strerror(errno));
    return 0;
  }
  if (unlink(path) && errno != ENOENT) {
    snprintf(e, 256, "capture files: remove pause: %s", strerror(errno));
    return 0;
  }
  return 1;
}
