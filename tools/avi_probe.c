#include "media/avi.h"
#include "resource/assets.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
static uint64_t hash_pixels(const uint16_t *p, size_t count) {
  uint64_t hash = UINT64_C(14695981039346656037);
  for (size_t i = 0; i < count; i++) {
    hash ^= p[i] & 255;
    hash *= UINT64_C(1099511628211);
    hash ^= p[i] >> 8;
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}
int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: avi-probe PACK\n");
    return 2;
  }
  BkArchive pack = {0};
  BkAvi *avi = NULL;
  BkAviDecoder *decoder = NULL;
  uint8_t *bytes = NULL;
  uint64_t *hashes = NULL, aggregate = 0, pixels = 0;
  unsigned files = 0, decoded = 0;
  char error[256];
  if (!bk_archive_open(&pack, argv[1], error))
    goto fail;
  for (uint32_t file = 0; file < pack.count; file++) {
    const BkEntry *entry = &pack.entries[file];
    size_t n = strlen(entry->name);
    if (n < 4 || strcmp(entry->name + n - 4, ".avi"))
      continue;
    if (!bk_archive_read(&pack, entry, &bytes, error))
      goto fail;
    avi = bk_avi_open(bytes, entry->size, error);
    free(bytes);
    bytes = NULL;
    if (!avi || !(decoder = bk_avi_decoder_create(avi, error)))
      goto fail;
    const BkAviInfo *info = bk_avi_info(avi);
    hashes = calloc(info->frames, sizeof(*hashes));
    if (!hashes) {
      snprintf(error, 256, "hash allocation failed");
      goto fail;
    }
    for (unsigned pass = 0; pass < 3; pass++) {
      for (uint32_t i = 0; i < info->frames; i++) {
        uint32_t at = pass == 1 ? info->frames - 1 - i : i;
        if (!bk_avi_decoder_frame(decoder, at, error))
          goto fail;
        uint64_t hash = hash_pixels(bk_avi_decoder_pixels(decoder),
                                    (size_t)info->width * info->height);
        if (!pass)
          hashes[at] = hash;
        else if (hash != hashes[at]) {
          snprintf(error, 256, "seek pixel mismatch %s/%u", entry->name, at);
          goto fail;
        }
        aggregate = (aggregate ^ hash) * UINT64_C(1099511628211);
        pixels += (size_t)info->width * info->height;
        ++decoded;
      }
    }
    printf("AVI %s %ux%u %u/%u fps %u frames\n", entry->name, info->width,
           info->height, info->rate, info->scale, info->frames);
    free(hashes);
    hashes = NULL;
    bk_avi_decoder_destroy(decoder);
    decoder = NULL;
    bk_avi_destroy(avi);
    avi = NULL;
    ++files;
  }
  bk_archive_close(&pack);
  if (!files) {
    fprintf(stderr, "no AVI assets\n");
    return 1;
  }
  printf("PASS AVI %u files %u decoded/seeking frames %" PRIu64
         " pixels hash %016" PRIx64 "\n",
         files, decoded, pixels, aggregate);
  return 0;
fail:
  fprintf(stderr, "AVI probe FAILED: %s\n", error);
  free(bytes);
  free(hashes);
  bk_avi_decoder_destroy(decoder);
  bk_avi_destroy(avi);
  bk_archive_close(&pack);
  return 1;
}
