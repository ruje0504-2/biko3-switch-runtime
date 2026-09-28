#include "resource/store.h"
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#define BK_MAX_MOUNTS 64
typedef struct {
  char pack[33];
  BkArchive archive;
  char *directory;
  size_t size_limit;
} Mount;
struct BkResourceStore {
  Mount mounts[BK_MAX_MOUNTS];
  unsigned count;
};
static int valid_pack(const char *pack) {
  if (!pack || !*pack || strlen(pack) > 32)
    return 0;
  for (const unsigned char *p = (const unsigned char *)pack; *p; p++)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
          (*p >= '0' && *p <= '9') || *p == '_' || *p == '-'))
      return 0;
  return 1;
}
BkResourceStore *bk_resources_create(char error[256]) {
  BkResourceStore *store = calloc(1, sizeof(*store));
  if (!store)
    snprintf(error, 256, "resource store allocation failed");
  return store;
}
void bk_resources_destroy(BkResourceStore *store) {
  if (!store)
    return;
  for (unsigned i = 0; i < store->count; i++) {
    bk_archive_close(&store->mounts[i].archive);
    free(store->mounts[i].directory);
  }
  free(store);
}
int bk_resources_mount(BkResourceStore *store, const char *pack,
                       const char *archive_path, char error[256]) {
  if (!store || !valid_pack(pack) || !archive_path ||
      store->count >= BK_MAX_MOUNTS) {
    snprintf(error, 256, "invalid resource mount or mount limit exceeded");
    return 0;
  }
  BkArchive archive = {0};
  if (!bk_archive_open(&archive, archive_path, error))
    return 0;
  Mount *m = &store->mounts[store->count++];
  strcpy(m->pack, pack);
  m->archive = archive;
  return 1;
}
int bk_resources_mount_directory(BkResourceStore *store, const char *pack,
                                 const char *directory, size_t size_limit,
                                 char error[256]) {
  if (!store || !valid_pack(pack) || !directory || !*directory || !size_limit ||
      size_limit == SIZE_MAX || store->count >= BK_MAX_MOUNTS) {
    snprintf(error, 256, "invalid loose-resource mount or size limit");
    return 0;
  }
  DIR *dir = opendir(directory);
  if (!dir) {
    snprintf(error, 256, "cannot open resource directory: %.160s", directory);
    return 0;
  }
  closedir(dir);
  size_t length = strlen(directory);
  if (length > 4096) {
    snprintf(error, 256, "resource directory path too long");
    return 0;
  }
  char *copy = malloc(length + 1);
  if (!copy) {
    snprintf(error, 256, "resource directory allocation failed");
    return 0;
  }
  memcpy(copy, directory, length + 1);
  Mount *mount = &store->mounts[store->count++];
  strcpy(mount->pack, pack);
  mount->directory = copy;
  mount->size_limit = size_limit;
  return 1;
}
static BkResourceResult read_loose(const Mount *mount, const char *name,
                                   BkBlob *out, char error[256]) {
  DIR *dir = opendir(mount->directory);
  if (!dir) {
    snprintf(error, 256, "mounted resource directory unavailable");
    return BK_RESOURCE_ERROR;
  }
  char found[256] = {0};
  int invalid = 0;
  struct dirent *entry;
  errno = 0;
  while ((entry = readdir(dir))) {
    if (strcasecmp(name, entry->d_name))
      continue;
    size_t length = strlen(entry->d_name);
    if (found[0] || length >= sizeof(found)) {
      invalid = 1;
      break;
    }
    memcpy(found, entry->d_name, length + 1);
  }
  if (errno)
    invalid = 1;
  closedir(dir);
  if (invalid) {
    snprintf(error, 256, "ambiguous name or resource directory read error");
    return BK_RESOURCE_ERROR;
  }
  if (!found[0])
    return BK_RESOURCE_MISSING;
  size_t length = strlen(mount->directory) + strlen(found) + 2;
  char *path = malloc(length);
  if (!path) {
    snprintf(error, 256, "resource path allocation failed");
    return BK_RESOURCE_ERROR;
  }
  snprintf(path, length, "%s/%s", mount->directory, found);
  struct stat info;
  if (stat(path, &info) || !S_ISREG(info.st_mode)) {
    free(path);
    snprintf(error, 256, "loose resource is not a regular file: %.64s", name);
    return BK_RESOURCE_ERROR;
  }
  FILE *file = fopen(path, "rb");
  free(path);
  if (!file) {
    snprintf(error, 256, "cannot open loose resource: %.64s", name);
    return BK_RESOURCE_ERROR;
  }
  uint8_t *data = NULL;
  if (fseek(file, 0, SEEK_END))
    goto bad;
  long size = ftell(file);
  if (size < 0 || (uint64_t)size > mount->size_limit ||
      fseek(file, 0, SEEK_SET))
    goto bad;
  data = malloc(size ? (size_t)size : 1);
  if (!data || fread(data, 1, (size_t)size, file) != (size_t)size ||
      fgetc(file) != EOF || ferror(file))
    goto bad;
  if (fclose(file)) {
    free(data);
    snprintf(error, 256, "loose resource close error");
    return BK_RESOURCE_ERROR;
  }
  *out = (BkBlob){data, (size_t)size};
  return BK_RESOURCE_OK;
bad:
  free(data);
  fclose(file);
  snprintf(error, 256, "loose resource read error or size limit: %.64s", name);
  return BK_RESOURCE_ERROR;
}
BkResourceResult bk_resources_read(BkResourceStore *store, const char *pack,
                                   const char *name, BkBlob *out,
                                   char error[256]) {
  if (!out) {
    snprintf(error, 256, "missing resource output");
    return BK_RESOURCE_ERROR;
  }
  *out = (BkBlob){0};
  if (!store || !valid_pack(pack) || !name || !*name || strchr(name, '/') ||
      strchr(name, '\\') || !strcmp(name, ".") || !strcmp(name, "..")) {
    snprintf(error, 256, "invalid resource key");
    return BK_RESOURCE_ERROR;
  }
  for (unsigned i = store->count; i > 0; i--) {
    Mount *mount = &store->mounts[i - 1];
    if (strcasecmp(mount->pack, pack))
      continue;
    if (mount->directory) {
      BkResourceResult result = read_loose(mount, name, out, error);
      if (result != BK_RESOURCE_MISSING)
        return result;
      continue;
    }
    const BkEntry *entry = bk_archive_find(&mount->archive, name);
    if (!entry)
      continue;
    if (!bk_archive_read(&mount->archive, entry, &out->data, error))
      return BK_RESOURCE_ERROR;
    out->size = entry->size;
    return BK_RESOURCE_OK;
  }
  snprintf(error, 256, "resource missing: %.32s/%.64s", pack, name);
  return BK_RESOURCE_MISSING;
}
void bk_blob_free(BkBlob *blob) {
  free(blob->data);
  *blob = (BkBlob){0};
}
