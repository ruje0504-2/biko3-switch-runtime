#include "resource/store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);               \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static void word(FILE *f, unsigned n) {
  for (unsigned i = 0; i < 4; i++)
    CHECK(fputc((n >> (i * 8)) & 255, f) != EOF);
}
static void fixture(const char *path, const char **names, const char **values,
                    unsigned count) {
  FILE *f = fopen(path, "wb");
  CHECK(f);
  unsigned total = 0;
  for (unsigned i = 0; i < count; i++)
    total += (unsigned)strlen(values[i]);
  word(f, count);
  word(f, total);
  for (unsigned i = 0; i < count; i++)
    for (unsigned j = 0; j < 32; j++)
      CHECK(fputc(j < strlen(names[i]) ? (unsigned char)-names[i][j] : 0, f) !=
            EOF);
  for (unsigned i = 0; i < count; i++)
    word(f, (unsigned)strlen(values[i]));
  for (unsigned i = 0; i < count; i++)
    for (unsigned j = 0; j < strlen(values[i]); j++)
      CHECK(fputc((unsigned char)-values[i][j], f) != EOF);
  CHECK(!fclose(f));
}
static void expect(BkResourceStore *store, const char *pack, const char *key,
                   const char *expected) {
  BkBlob b;
  char error[256];
  CHECK(bk_resources_read(store, pack, key, &b, error) == BK_RESOURCE_OK);
  CHECK(b.size == strlen(expected) && !memcmp(b.data, expected, b.size));
  bk_blob_free(&b);
  CHECK(!b.data && !b.size);
}
int main(void) {
  char directory[] = "/tmp/biko3-store-XXXXXX";
  CHECK(mkdtemp(directory));
  char base[256], patch[256], corrupt[256], error[256];
  snprintf(base, sizeof(base), "%s/base.pp", directory);
  snprintf(patch, sizeof(patch), "%s/patch.pp", directory);
  snprintf(corrupt, sizeof(corrupt), "%s/bad.pp", directory);
  const char *names[] = {"title.txt", "fallback.txt"};
  const char *original[] = {"original", "base only"};
  const char *replacement[] = {"translated"};
  fixture(base, names, original, 2);
  fixture(patch, names, replacement, 1);
  FILE *bad = fopen(corrupt, "wb");
  CHECK(bad);
  CHECK(fputc(1, bad) != EOF);
  CHECK(!fclose(bad));
  BkResourceStore *store = bk_resources_create(error);
  CHECK(store);
  CHECK(bk_resources_mount(store, "base", base, error));
  CHECK(bk_resources_mount(store, "unrelated", patch, error));
  expect(store, "BASE", "TITLE.TXT", "original");
  CHECK(bk_resources_mount(store, "base", patch, error));
  expect(store, "base", "title.txt", "translated");
  expect(store, "base", "fallback.txt", "base only");
  CHECK(!bk_resources_mount(store, "base", corrupt, error));
  expect(store, "base", "title.txt", "translated");
  CHECK(!bk_resources_mount(store, "../escape", base, error));
  BkBlob blob;
  CHECK(bk_resources_read(store, "base", "absent.txt", &blob, error) ==
        BK_RESOURCE_MISSING);
  CHECK(!blob.data && !blob.size);
  CHECK(bk_resources_read(store, "base", "../title.txt", &blob, error) ==
        BK_RESOURCE_ERROR);
  CHECK(bk_resources_read(store, "base", "title.txt", NULL, error) ==
        BK_RESOURCE_ERROR);
  char loose[256];
  snprintf(loose, sizeof(loose), "%s/title.txt", directory);
  FILE *plain = fopen(loose, "wb");
  CHECK(plain && fwrite("loose", 1, 5, plain) == 5 && !fclose(plain));
  CHECK(!bk_resources_mount_directory(store, "base", corrupt, 64, error));
  CHECK(!bk_resources_mount_directory(store, "base", directory, 0, error));
  CHECK(bk_resources_mount_directory(store, "base", directory, 64, error));
  expect(store, "BASE", "TITLE.TXT", "loose");
  expect(store, "base", "fallback.txt", "base only");
  CHECK(bk_resources_mount_directory(store, "small", directory, 4, error));
  CHECK(bk_resources_read(store, "small", "title.txt", &blob, error) ==
        BK_RESOURCE_ERROR);
  CHECK(!blob.data && !blob.size);
  CHECK(!unlink(loose));
  expect(store, "base", "title.txt", "translated");
  /* A directory with the requested name is a corrupt selected resource,
   * not a missing entry that should silently expose the archive fallback. */
  CHECK(!mkdir(loose, 0700));
  CHECK(bk_resources_read(store, "base", "title.txt", &blob, error) ==
        BK_RESOURCE_ERROR);
  CHECK(!blob.data && !blob.size);
  CHECK(!rmdir(loose));
  /* A selected overlay's failed payload read must not expose base content. */
  CHECK(!truncate(patch, 0));
  /* Closing/reopening is intentionally not done: archive descriptors stay owned
   * by the store. Stdio may buffer small files, so use a fresh large archive.
   */
  bk_resources_destroy(store);
  store = bk_resources_create(error);
  CHECK(store);
  CHECK(bk_resources_mount(store, "base", base, error));
  char *large = malloc(32769);
  CHECK(large);
  memset(large, 'p', 32768);
  large[32768] = 0;
  const char *large_values[] = {large};
  fixture(patch, names, large_values, 1);
  free(large);
  CHECK(bk_resources_mount(store, "base", patch, error));
  CHECK(!truncate(patch, 0));
  CHECK(bk_resources_read(store, "base", "title.txt", &blob, error) ==
        BK_RESOURCE_ERROR);
  CHECK(!blob.data && !blob.size);
  bk_resources_destroy(store);
  CHECK(!unlink(base));
  CHECK(!unlink(patch));
  CHECK(!unlink(corrupt));
  CHECK(!rmdir(directory));
  puts("PASS: overlay priority, pack isolation, base fallback, failed mount "
       "rollback, read-error propagation");
  return 0;
}
