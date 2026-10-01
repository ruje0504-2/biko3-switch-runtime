#include "resource/store.h"
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <zlib.h>
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
  BkArchive patch;
  unsigned patch_flags;
  unsigned patch_version;
};
static uint32_t patch_word(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
int bk_resources_load_patch(BkResourceStore *s, const char *path, char e[256]) {
  if (!s || !path || s->patch.file) {
    snprintf(e, 256, "invalid or duplicate resource patch"); return 0;
  }
  struct stat info;
  if (stat(path, &info)) {
    if (errno == ENOENT) return 1;
    snprintf(e, 256, "cannot inspect resource patch"); return 0;
  }
  BkArchive patch = {0};
  if (!bk_archive_open(&patch, path, e)) return 0;
  const BkEntry *entry = bk_archive_find(&patch, "patch.cfg");
  uint8_t *raw = NULL;
  if (!entry || entry->size != 12 || !bk_archive_read(&patch, entry, &raw, e) ||
      memcmp(raw, "BKPT", 4) || (patch_word(raw + 4) != 1 && patch_word(raw + 4) != 2) ||
      !patch_word(raw + 8) || (patch_word(raw + 8) & ~3u)) {
    free(raw); bk_archive_close(&patch);
    snprintf(e, 256, "invalid resource patch metadata"); return 0;
  }
  s->patch_flags = patch_word(raw + 8);
  s->patch_version = patch_word(raw + 4);
  s->patch = patch;
  free(raw);
  return 1;
}
unsigned bk_resources_patch_flags(const BkResourceStore *s) {
  return s ? s->patch_flags : 0;
}
typedef struct {
  uint8_t *bytes;
  uint32_t size, count;
  int complete;
} ResourcePatch;
static int prepare_patch(BkResourceStore *s, const char *pack,
                          const char *name, size_t original_size,
                          ResourcePatch *out, char e[256]) {
  if (!s->patch.file) return 1;
  char key[100];
  int n = snprintf(key, sizeof(key), "%s.%s", pack, name);
  if (n < 0 || n >= (int)sizeof(key)) return 1;
  const BkEntry *entry = bk_archive_find(&s->patch, key);
  if (!entry) return 1;
  uint8_t *raw = NULL;
  if (!bk_archive_read(&s->patch, entry, &raw, e)) goto bad;
  size_t raw_size = entry->size;
  if (s->patch_version == 2) {
    if (raw_size < 4) goto invalid;
    uint32_t expanded = patch_word(raw);
    if (expanded < 12 || expanded > 256u * 1024 * 1024) goto invalid;
    uint8_t *decoded = malloc(expanded);
    if (!decoded) { snprintf(e, 256, "resource patch allocation failed"); goto bad; }
    uLongf length = expanded;
    uLong compressed = raw_size - 4;
    int status = uncompress2(decoded, &length, raw + 4, &compressed);
    if (status != Z_OK || length != expanded || compressed != raw_size - 4) {
      free(decoded); goto invalid;
    }
    free(raw);
    raw = decoded;
    raw_size = expanded;
  }
  if (raw_size < 12 || patch_word(raw) != original_size) goto invalid;
  uint32_t size = patch_word(raw + 4), count = patch_word(raw + 8);
  if (!size || !count || count > (raw_size - 12) / 8) goto invalid;
  size_t cursor = 12;
  for (uint32_t i = 0; i < count; ++i) {
    if (raw_size - cursor < 8) goto invalid;
    uint32_t offset = patch_word(raw + cursor), length = patch_word(raw + cursor + 4);
    cursor += 8;
    if (offset > size || length > size - offset || length > raw_size - cursor)
      goto invalid;
    /* A size-changing replacement must supply the complete new resource. */
    if (size != original_size && (count != 1 || offset || length != size)) goto invalid;
    cursor += length;
  }
  if (cursor != raw_size) goto invalid;
  *out = (ResourcePatch){raw, size, count,
                        count == 1 && !patch_word(raw + 12) && patch_word(raw + 16) == size};
  return 1;
invalid:
  snprintf(e, 256, "resource patch incompatible/corrupt: %.32s/%.64s", pack, name);
bad:
  free(raw);
  return 0;
}
static int patch_complete(ResourcePatch *patch, BkBlob *out) {
  if (!patch->complete) return 0;
  /* Transfer the replacement buffer directly. No read/allocation/decoding of
   * the base payload, and no second allocation for the replacement. */
  memmove(patch->bytes, patch->bytes + 20, patch->size);
  *out = (BkBlob){patch->bytes, patch->size};
  return 1;
}
static void patch_ranges(ResourcePatch *patch, BkBlob *out) {
  if (!patch->bytes) return;
  /* All ranges were checked before reading the base. Same-size model edits
   * remain in place; never retain another large model buffer. */
  size_t cursor = 12;
  for (uint32_t i = 0; i < patch->count; ++i) {
    uint32_t offset = patch_word(patch->bytes + cursor);
    uint32_t length = patch_word(patch->bytes + cursor + 4);
    cursor += 8;
    memcpy(out->data + offset, patch->bytes + cursor, length);
    cursor += length;
  }
  free(patch->bytes);
}
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
  bk_archive_close(&store->patch);
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
static BkResourceResult read_loose(BkResourceStore *store, const Mount *mount, const char *name,
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
  ResourcePatch patch = {0};
  if (info.st_size < 0 || (uintmax_t)info.st_size > mount->size_limit) {
    free(path);
    snprintf(error, 256, "loose resource exceeds size limit: %.64s", name);
    return BK_RESOURCE_ERROR;
  }
  if (!prepare_patch(store, mount->pack, name, (size_t)info.st_size, &patch, error)) {
    free(path); return BK_RESOURCE_ERROR;
  }
  if (patch_complete(&patch, out)) {
    free(path); return BK_RESOURCE_OK;
  }
  FILE *file = fopen(path, "rb");
  free(path);
  if (!file) {
    free(patch.bytes);
    snprintf(error, 256, "cannot open loose resource: %.64s", name);
    return BK_RESOURCE_ERROR;
  }
  uint8_t *data = NULL;
  if (fseek(file, 0, SEEK_END))
    goto bad;
  long size = ftell(file);
  if (size < 0 || size != info.st_size || (uint64_t)size > mount->size_limit ||
      fseek(file, 0, SEEK_SET))
    goto bad;
  data = malloc(size ? (size_t)size : 1);
  if (!data || fread(data, 1, (size_t)size, file) != (size_t)size ||
      fgetc(file) != EOF || ferror(file))
    goto bad;
  if (fclose(file)) {
    free(data);
    free(patch.bytes);
    snprintf(error, 256, "loose resource close error");
    return BK_RESOURCE_ERROR;
  }
  *out = (BkBlob){data, (size_t)size};
  patch_ranges(&patch, out);
  return BK_RESOURCE_OK;
bad:
  free(data);
  free(patch.bytes);
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
      BkResourceResult result = read_loose(store, mount, name, out, error);
      if (result != BK_RESOURCE_MISSING)
        return result;
      continue;
    }
    const BkEntry *entry = bk_archive_find(&mount->archive, name);
    if (!entry)
      continue;
    if (entry->size > 256u * 1024 * 1024) {
      snprintf(error, 256, "resource exceeds 256 MiB");
      return BK_RESOURCE_ERROR;
    }
    ResourcePatch patch = {0};
    if (!prepare_patch(store, pack, name, entry->size, &patch, error))
      return BK_RESOURCE_ERROR;
    if (patch_complete(&patch, out)) return BK_RESOURCE_OK;
    if (!bk_archive_read(&mount->archive, entry, &out->data, error)) {
      free(patch.bytes); return BK_RESOURCE_ERROR;
    }
    out->size = entry->size;
    patch_ranges(&patch, out);
    return BK_RESOURCE_OK;
  }
  snprintf(error, 256, "resource missing: %.32s/%.64s", pack, name);
  return BK_RESOURCE_MISSING;
}
void bk_blob_free(BkBlob *blob) {
  free(blob->data);
  *blob = (BkBlob){0};
}
