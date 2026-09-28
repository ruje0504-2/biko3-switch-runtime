#include "media/pcm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkPcm {
  uint32_t rate, channels;
  size_t frames;
  int16_t samples[];
};
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static uint16_t u16(const uint8_t *p) {
  return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}
static BkPcm *fail(char error[256], const char *why) {
  snprintf(error, 256, "PCM WAV: %s", why);
  return NULL;
}
BkPcm *bk_pcm_decode(const void *bytes, size_t size, char error[256]) {
  const uint8_t *b = bytes;
  if (!b || size < 12 || memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4) ||
      (uint64_t)u32(b + 4) + 8 != size)
    return fail(error, "invalid RIFF extent/type");
  const uint8_t *fmt = NULL, *samples = NULL;
  size_t format_size = 0, sample_bytes = 0;
  for (size_t at = 12; at < size;) {
    if (size - at < 8)
      return fail(error, "truncated chunk header");
    uint32_t n = u32(b + at + 4);
    const uint8_t *tag = b + at;
    at += 8;
    if (n > size - at || (n & 1 && n == size - at))
      return fail(error, "truncated chunk/padding");
    if (!memcmp(tag, "fmt ", 4)) {
      if (fmt)
        return fail(error, "duplicate format chunk");
      fmt = b + at;
      format_size = n;
    } else if (!memcmp(tag, "data", 4)) {
      if (samples)
        return fail(error, "duplicate data chunk");
      samples = b + at;
      sample_bytes = n;
    }
    at += (size_t)n + (n & 1);
  }
  if (!fmt || format_size < 16 || !samples || !sample_bytes)
    return fail(error, "missing/truncated format or empty samples");
  uint32_t channels = u16(fmt + 2), rate = u32(fmt + 4), align = u16(fmt + 12);
  if (u16(fmt) != 1 || (channels != 1 && channels != 2) || u16(fmt + 14) != 16)
    return fail(error, "only mono/stereo PCM16 is supported");
  if (!rate || align != channels * 2 ||
      (uint64_t)rate * align != u32(fmt + 8) || sample_bytes % align ||
      sample_bytes > SIZE_MAX - sizeof(BkPcm))
    return fail(error, "invalid sample rate/block size");
  BkPcm *p = malloc(sizeof(*p) + sample_bytes);
  if (!p)
    return fail(error, "allocation failed");
  p->rate = rate;
  p->channels = channels;
  p->frames = sample_bytes / align;
  for (size_t i = 0; i < sample_bytes / 2; i++) {
    uint32_t word = u16(samples + i * 2);
    p->samples[i] =
        (int16_t)(word < 32768 ? (int32_t)word : (int32_t)word - 65536);
  }
  return p;
}
void bk_pcm_destroy(BkPcm *p) { free(p); }
uint32_t bk_pcm_rate(const BkPcm *p) { return p ? p->rate : 0; }
uint32_t bk_pcm_channels(const BkPcm *p) { return p ? p->channels : 0; }
size_t bk_pcm_frames(const BkPcm *p) { return p ? p->frames : 0; }
const int16_t *bk_pcm_samples(const BkPcm *p) { return p ? p->samples : NULL; }
