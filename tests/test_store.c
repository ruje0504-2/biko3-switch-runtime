#include "resource/store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
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
static void binary_fixture(const char *path, const char **names,
                           const char **values, const unsigned *sizes,
                           unsigned count) {
  FILE *f = fopen(path, "wb");
  CHECK(f);
  unsigned total = 0;
  for (unsigned i = 0; i < count; i++)
    total += sizes[i];
  word(f, count);
  word(f, total);
  for (unsigned i = 0; i < count; i++)
    for (unsigned j = 0; j < 32; j++)
      CHECK(fputc(j < strlen(names[i]) ? (unsigned char)-names[i][j] : 0, f) !=
            EOF);
  for (unsigned i = 0; i < count; i++)
    word(f, sizes[i]);
  for (unsigned i = 0; i < count; i++)
    for (unsigned j = 0; j < sizes[i]; j++)
      CHECK(fputc((unsigned char)-values[i][j], f) != EOF);
  CHECK(!fclose(f));
}
static void fixture(const char *path, const char **names, const char **values,
                    unsigned count) {
  unsigned *sizes = malloc(count * sizeof(*sizes));
  CHECK(sizes);
  for (unsigned i = 0; i < count; ++i) sizes[i] = (unsigned)strlen(values[i]);
  binary_fixture(path, names, values, sizes, count);
  free(sizes);
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
static void compressed_fixture(const char *path, const char **names,
                                const char **values, const unsigned *sizes,
                                unsigned damage) {
  unsigned char config[12], bytes[3][256] = {{0}};
  memcpy(config, values[0], 12); config[4] = 2;
  const char *encoded[] = {(char *)config, (char *)bytes[0], (char *)bytes[1], (char *)bytes[2]};
  unsigned lengths[] = {12, 0, 0, 0};
  for (unsigned i = 1; i < 4; ++i) {
    for (unsigned j = 0; j < 4; ++j) bytes[i-1][j] = sizes[i] >> (8*j);
    uLongf length = sizeof(bytes[0]) - 4;
    CHECK(compress2(bytes[i-1]+4, &length, (const Bytef *)values[i], sizes[i], 1) == Z_OK);
    lengths[i] = (unsigned)length + 4;
  }
  if (damage == 1) --lengths[1];
  if (damage == 2) ++bytes[0][0];
  if (damage == 3) bytes[0][lengths[1]-1] ^= 0x80;
  if (damage == 4) ++lengths[1];
  binary_fixture(path, names, encoded, lengths, 4);
}
static void resource_patch(const char *base, const char *patch,
                           const char *directory, const char *loose) {
  char error[256], missing[256];
  snprintf(missing, sizeof(missing), "%s/absent.pp", directory);
  const char *names[] = {"patch.cfg", "base.title.txt", "base.fallback.txt",
                         "fonts.title.txt"};
  const char config[] = {'B','K','P','T',1,0,0,0,3,0,0,0};
  const char ranges[] = {8,0,0,0,8,0,0,0,2,0,0,0,
    0,0,0,0,3,0,0,0,'N','E','W',4,0,0,0,4,0,0,0,'!','!','!','!'};
  const char whole[] = {9,0,0,0,12,0,0,0,1,0,0,0,
    0,0,0,0,12,0,0,0,'l','o','n','g','e','r',' ','v','a','l','u','e'};
  const char font[] = {5,0,0,0,2,0,0,0,1,0,0,0,0,0,0,0,2,0,0,0,'z','h'};
  const char *values[] = {config, ranges, whole, font};
  const unsigned sizes[] = {sizeof(config), sizeof(ranges), sizeof(whole), sizeof(font)};
  binary_fixture(patch, names, values, sizes, 4);
  FILE *plain = fopen(loose, "wb");
  CHECK(plain && fwrite("loose", 1, 5, plain) == 5 && !fclose(plain));
  BkResourceStore *store = bk_resources_create(error);
  CHECK(store && bk_resources_load_patch(store, missing, error));
  CHECK(!bk_resources_patch_flags(store));
  CHECK(bk_resources_mount(store, "base", base, error));
  CHECK(bk_resources_mount(store, "unrelated", base, error));
  CHECK(bk_resources_mount_directory(store, "fonts", directory, 64, error));
  expect(store, "base", "title.txt", "original");
  CHECK(bk_resources_load_patch(store, patch, error));
  CHECK(bk_resources_patch_flags(store) == (BK_PATCH_CHINESE | BK_PATCH_UNCENSORED));
  CHECK(!bk_resources_load_patch(store, patch, error));
  expect(store, "BASE", "TITLE.TXT", "NEWg!!!!");
  expect(store, "base", "fallback.txt", "longer value");
  expect(store, "fonts", "title.txt", "zh");
  expect(store, "unrelated", "title.txt", "original");
  bk_resources_destroy(store);
  for (unsigned damage = 0; damage < 5; ++damage) {
    compressed_fixture(patch, names, values, sizes, damage);
    store = bk_resources_create(error);
    CHECK(store && bk_resources_mount(store, "base", base, error));
    CHECK(bk_resources_mount_directory(store, "fonts", directory, 64, error));
    CHECK(bk_resources_load_patch(store, patch, error));
    if (!damage) {
      expect(store, "base", "title.txt", "NEWg!!!!");
      expect(store, "base", "fallback.txt", "longer value");
      expect(store, "fonts", "title.txt", "zh");
    } else {
      BkBlob blob = {0};
      CHECK(bk_resources_read(store, "base", "title.txt", &blob, error) == BK_RESOURCE_ERROR);
      CHECK(!blob.data && !blob.size);
    }
    bk_resources_destroy(store);
  }
  /* Bad source size, out-of-range segment and truncated record must fail,
   * rather than expose base bytes or a partially applied model. */
  for (unsigned kind = 0; kind < 3; ++kind) {
    char invalid[sizeof(ranges)]; memcpy(invalid, ranges, sizeof(invalid));
    if (kind == 0) invalid[0] = 7;
    if (kind == 1) invalid[23] = 7;
    values[1] = invalid;
    unsigned broken_sizes[] = {sizeof(config), sizeof(ranges) - (kind == 2)};
    binary_fixture(patch, names, values, broken_sizes, 2);
    store = bk_resources_create(error);
    CHECK(store && bk_resources_mount(store, "base", base, error));
    CHECK(bk_resources_load_patch(store, patch, error));
    BkBlob blob = {0};
    CHECK(bk_resources_read(store, "base", "title.txt", &blob, error) == BK_RESOURCE_ERROR);
    CHECK(!blob.data && !blob.size);
    expect(store, "base", "fallback.txt", "base only");
    bk_resources_destroy(store);
  }
  CHECK(!unlink(loose));
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
  resource_patch(base, patch, directory, loose);
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
       "rollback, read-error propagation, optional combined resource patch");
  return 0;
}
