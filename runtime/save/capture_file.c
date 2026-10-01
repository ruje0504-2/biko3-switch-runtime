#include "save/capture_file.h"
#include "save/file_replace_internal.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
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
static int photo_group(const char *name) {
  static const char *const prefixes[] = {"ri_", "re_", "cr_", "ma_", "mi_"};
  size_t n = strlen(name);
  if (n < 7 || name[n - 4] != '.' ||
      (name[n - 3] != 'b' && name[n - 3] != 'B') ||
      (name[n - 2] != 'm' && name[n - 2] != 'M') ||
      (name[n - 1] != 'p' && name[n - 1] != 'P')) return -1;
  for (unsigned group=0; group<5; ++group)
    if (!strncmp(name,prefixes[group],3)) return (int)group;
  return -1;
}
int bk_capture_files_count_photos(BkCaptureFiles *f, int32_t counts[5],
                                   char e[256]) {
  if (!f || !counts) {
    snprintf(e, 256, "capture files: missing inventory owner/output");
    return 0;
  }
  char path[sizeof(f->root) + sizeof("/album")];
  snprintf(path, sizeof(path), "%s/album", f->root);
  int32_t found[5] = {0};
  DIR *dir = opendir(path);
  if (!dir) {
    if (errno == ENOENT) {
      memcpy(counts, found, sizeof(found));
      return 1;
    }
    snprintf(e, 256, "capture files: scan album: %s", strerror(errno));
    return 0;
  }
  int scan_error;
  for (;;) {
    errno = 0;
    struct dirent *entry = readdir(dir);
    if (!entry) {
      scan_error = errno;
      break;
    }
    int group=photo_group(entry->d_name);
    if (group>=0 && found[group]<100) ++found[group];
  }
  int close_error = closedir(dir) ? errno : 0;
  if (scan_error || close_error) {
    snprintf(e, 256, "capture files: scan album: %s",
             strerror(scan_error ? scan_error : close_error));
    return 0;
  }
  memcpy(counts, found, sizeof(found));
  return 1;
}
void bk_photo_list_free(BkPhotoList *list) {
  if (list) { free(list->names); *list=(BkPhotoList){0}; }
}
int bk_capture_files_list_photos(BkCaptureFiles *f,unsigned group,size_t limit,
                                  BkPhotoList *out,char e[256]) {
  if (!f || group>=5 || !limit || limit>INT32_MAX || !out || out->names || out->count) {
    snprintf(e,256,"capture files: invalid photo list request");return 0;
  }
  char path[sizeof(f->root)+sizeof("/album")];
  snprintf(path,sizeof(path),"%s/album",f->root);
  DIR *dir=opendir(path);
  if (!dir) {
    if (errno==ENOENT) return 1;
    snprintf(e,256,"capture files: list album: %s",strerror(errno));return 0;
  }
  BkPhotoList list={0};size_t capacity=0;int failure=0;
  while (list.count<limit) {
    errno=0;struct dirent *entry=readdir(dir);
    if (!entry) { failure=errno;break; }
    if (photo_group(entry->d_name)!=(int)group) continue;
    size_t length=strlen(entry->d_name);
    if (length>=sizeof(*list.names)) { failure=ENAMETOOLONG;break; }
    if (list.count==capacity) {
      size_t next=capacity?capacity*2:16;
      if (next>limit)next=limit;
      void *names=realloc(list.names,next*sizeof(*list.names));
      if (!names) { failure=ENOMEM;break; }
      list.names=names;capacity=next;
    }
    memcpy(list.names[list.count++],entry->d_name,length+1);
  }
  if (closedir(dir) && !failure)failure=errno;
  if (failure) {
    bk_photo_list_free(&list);snprintf(e,256,"capture files: list album: %s",strerror(failure));return 0;
  }
  *out=list;return 1;
}
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
static BkResourceResult read_image_file(const char *path,size_t limit,
                                          BkBlob *out,char e[256]) {
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
    snprintf(e, 256, "capture files: empty, oversized or unreadable image");
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
    snprintf(e, 256, "capture files: image read failed/changed");
    return BK_RESOURCE_ERROR;
  }
  *out = b;
  return BK_RESOURCE_OK;
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
  return read_image_file(path,limit,out,e);
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

static int photo_path(BkCaptureFiles *f,const char *name,char path[1288],char e[256]) {
  if (!f || !name || strlen(name)>255 || photo_group(name)<0 ||
      strchr(name,'/') || strchr(name,'\\') || strchr(name,':') || strstr(name,"..")) {
    snprintf(e,256,"capture files: invalid photo basename");return 0;
  }
  snprintf(path,1288,"%s/album/%s",f->root,name);return 1;
}
BkResourceResult bk_capture_file_read_photo(BkCaptureFiles *f,const char *name,
                                            size_t limit,BkBlob *out,char e[256]) {
  char path[1288];
  if (!limit || !out || out->data || out->size) {
    snprintf(e,256,"capture files: invalid photo read/output/limit");return BK_RESOURCE_ERROR;
  }
  if (!photo_path(f,name,path,e) || !bk_save_file_recover(path,e))return BK_RESOURCE_ERROR;
  return read_image_file(path,limit,out,e);
}
int bk_capture_file_remove_photo(BkCaptureFiles *f,const char *name,char e[256]) {
  char path[1288];
  if (!photo_path(f,name,path,e) || !bk_save_file_recover(path,e))return 0;
  if (unlink(path) && errno!=ENOENT) {
    snprintf(e,256,"capture files: remove photo: %s",strerror(errno));return 0;
  }
  return 1;
}
