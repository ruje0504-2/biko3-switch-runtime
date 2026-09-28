#include "resource/assets.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: asset-probe archive.pp [--verify | --image name "
                    "output.rgba]\n");
    return 2;
  }
  BkArchive a;
  char error[256];
  unsigned images = 0;
  if (!bk_archive_open(&a, argv[1], error)) {
    fprintf(stderr, "%s: %s\n", argv[1], error);
    return 1;
  }
  for (uint32_t i = 0; i < a.count; i++) {
    const BkEntry *entry = a.entries + i;
    const char *ext = strrchr(entry->name, '.');
    if (argc == 2)
      printf("%u\t%u\t%s\n", entry->offset, entry->size, entry->name);
    int image = ext && (!strcasecmp(ext, ".tga") || !strcasecmp(ext, ".bmp"));
    if (argc >= 3 && image &&
        (!strcmp(argv[2], "--verify") ||
         (argc == 5 && !strcmp(argv[2], "--image") &&
          !strcasecmp(entry->name, argv[3])))) {
      uint8_t *bytes;
      BkImage im;
      if (!bk_archive_read(&a, entry, &bytes, error))
        goto failed;
      int ok = bk_image_decode(bytes, entry->size, &im, error);
      free(bytes);
      if (!ok) {
        fprintf(stderr, "resource: %s\n", entry->name);
        goto failed;
      }
      images++;
      if (argc == 5) {
        FILE *out = fopen(argv[4], "wb");
        size_t size = (size_t)im.width * im.height * 4;
        if (!out) {
          bk_image_free(&im);
          snprintf(error, 256, "cannot open output");
          goto failed;
        }
        int written = fwrite(im.rgba, 1, size, out) == size,
            closed = fclose(out) == 0;
        if (!written || !closed) {
          bk_image_free(&im);
          snprintf(error, 256, "output write failed");
          goto failed;
        }
        printf("%u %u\n", im.width, im.height);
      }
      bk_image_free(&im);
    }
  }
  if (argc == 3)
    printf("%s: %u entries, %u images decoded\n", argv[1], a.count, images);
  if (argc == 5 && !images) {
    snprintf(error, 256, "image not found");
    goto failed;
  }
  bk_archive_close(&a);
  return 0;
failed:
  fprintf(stderr, "%s: %s\n", argv[1], error);
  bk_archive_close(&a);
  return 1;
}
