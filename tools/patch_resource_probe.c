/* Compare actual v1/v2 patch resources through the production store. */
#include "resource/store.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s: %s\n", #x, error); goto done; } } while (0)
int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: patch-resource-probe game-root old-patch.pp new-patch.pp\n");
    return 2;
  }
  char error[256] = {0}, mounts[64][33] = {{0}};
  unsigned mounted = 0, checked = 0;
  uint64_t bytes = 0;
  int result = 1;
  BkArchive index = {0};
  BkBlob a = {0}, b = {0};
  BkResourceStore *first = bk_resources_create(error);
  BkResourceStore *second = bk_resources_create(error);
  CHECK(first && second && bk_archive_open(&index, argv[2], error));
  CHECK(bk_resources_load_patch(first, argv[2], error));
  CHECK(bk_resources_load_patch(second, argv[3], error));
  CHECK(bk_resources_patch_flags(first) == 3 && bk_resources_patch_flags(second) == 3);
  for (unsigned i = 0; i < index.count; ++i) {
    const char *key = index.entries[i].name;
    if (!strcmp(key, "patch.cfg")) continue;
    const char *dot = strchr(key, '.');
    CHECK(dot && dot-key > 0 && dot-key <= 32);
    char pack[33] = {0}, path[1024];
    memcpy(pack, key, (size_t)(dot-key));
    unsigned j = 0;
    while (j < mounted && strcmp(mounts[j], pack)) ++j;
    if (j == mounted) {
      CHECK(mounted < 64);
      strcpy(mounts[mounted++], pack);
      if (!strcmp(pack, "fonts")) {
        CHECK(snprintf(path, sizeof(path), "%s/Data", argv[1]) < (int)sizeof(path));
        CHECK(bk_resources_mount_directory(first, pack, path, 16u<<20, error));
        CHECK(bk_resources_mount_directory(second, pack, path, 16u<<20, error));
      } else {
        CHECK(snprintf(path, sizeof(path), "%s/Data/%s.pp", argv[1], pack) < (int)sizeof(path));
        CHECK(bk_resources_mount(first, pack, path, error));
        CHECK(bk_resources_mount(second, pack, path, error));
      }
    }
    CHECK(bk_resources_read(first, pack, dot+1, &a, error) == BK_RESOURCE_OK);
    CHECK(bk_resources_read(second, pack, dot+1, &b, error) == BK_RESOURCE_OK);
    CHECK(a.size == b.size && !memcmp(a.data, b.data, a.size));
    bytes += a.size; ++checked;
    bk_blob_free(&a); bk_blob_free(&b);
  }
  printf("PASS patch v1/v2: %u resources, %" PRIu64 " identical decoded bytes\n", checked, bytes);
  result = 0;
done:
  bk_blob_free(&a); bk_blob_free(&b);
  bk_archive_close(&index);
  bk_resources_destroy(first); bk_resources_destroy(second);
  return result;
}
