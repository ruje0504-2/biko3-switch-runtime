#include "media/pcm.h"
#include "resource/assets.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: pcm-probe PACK...\n");
    return 2;
  }
  char error[256] = {0};
  uint64_t frames = 0, hash = UINT64_C(14695981039346656037);
  unsigned files = 0;
  for (int i = 1; i < argc; i++) {
    BkArchive a;
    if (!bk_archive_open(&a, argv[i], error))
      goto fail;
    for (uint32_t j = 0; j < a.count; j++) {
      const BkEntry *e = &a.entries[j];
      size_t length = strlen(e->name);
      if (length < 4 || strcmp(e->name + length - 4, ".wav"))
        continue;
      uint8_t *bytes = NULL;
      if (!bk_archive_read(&a, e, &bytes, error)) {
        bk_archive_close(&a);
        goto fail;
      }
      BkPcm *pcm = bk_pcm_decode(bytes, e->size, error);
      free(bytes);
      if (!pcm) {
        fprintf(stderr, "%s/%s\n", argv[i], e->name);
        bk_archive_close(&a);
        goto fail;
      }
      size_t count = bk_pcm_frames(pcm) * bk_pcm_channels(pcm);
      const int16_t *data = bk_pcm_samples(pcm);
      for (size_t k = 0; k < count; k++) {
        uint16_t v = (uint16_t)data[k];
        hash ^= v & 255;
        hash *= UINT64_C(1099511628211);
        hash ^= v >> 8;
        hash *= UINT64_C(1099511628211);
      }
      files++;
      frames += bk_pcm_frames(pcm);
      bk_pcm_destroy(pcm);
    }
    bk_archive_close(&a);
  }
  printf("PASS PCM %u files, %" PRIu64
         " sample frames, owned samples hash %016" PRIx64 "\n",
         files, frames, hash);
  return 0;
fail:
  fprintf(stderr, "PCM probe FAILED: %s\n", error);
  return 1;
}
