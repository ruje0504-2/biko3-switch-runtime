#include "media/pcm.h"
#include "media/pcm_mix.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void word(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (i * 8));
}
int main(void) {
  uint8_t wav[72] = {0};
  char error[256];
  memcpy(wav, "RIFF", 4);
  word(wav + 4, 64);
  memcpy(wav + 8, "WAVEJUNK", 8);
  word(wav + 16, 1);
  wav[20] = 11;
  memcpy(wav + 22, "data", 4);
  word(wav + 26, 8);
  wav[30] = 0xff;
  wav[31] = 0x7f;
  wav[32] = 0;
  wav[33] = 0x80;
  wav[34] = 0xff;
  wav[35] = 0xff;
  wav[36] = 0;
  wav[37] = 0;
  memcpy(wav + 38, "fmt ", 4);
  word(wav + 42, 18);
  wav[46] = 1;
  wav[48] = 2;
  word(wav + 50, 22090);
  word(wav + 54, 88360);
  wav[58] = 4;
  wav[60] = 16;
  memcpy(wav + 64, "JUNK", 4);
  word(wav + 68, 0);
  BkPcm *p = bk_pcm_decode(wav, sizeof(wav), error);
  assert(p);
  assert(bk_pcm_rate(p) == 22090 && bk_pcm_channels(p) == 2 &&
         bk_pcm_frames(p) == 2);
  const int16_t *v = bk_pcm_samples(p);
  assert(v[0] == 32767 && v[1] == -32768 && v[2] == -1 && v[3] == 0);
  uint64_t duration;
  assert(bk_pcm_duration(p, 44180, &duration) && duration == 4);
  float whole[12] = {0}, split[12] = {0};
  assert(bk_pcm_mix(p, 44180, 0, 0, 0, 0, whole, 6, error));
  assert(whole[0] == 32767 && whole[1] == -32768 && whole[2] == 16383 &&
         whole[3] == -16384 && whole[4] == -1 && whole[6] == -1 &&
         whole[8] == 0);
  assert(bk_pcm_mix(p, 44180, 0, 0, 0, 0, split, 2, error));
  assert(bk_pcm_mix(p, 44180, 2, 0, 0, 0, split + 4, 4, error));
  assert(!memcmp(whole, split, sizeof(whole)));
  memset(split, 0, sizeof(split));
  assert(bk_pcm_mix(p, 44180, 3, 1, 0, 10000, split, 6, error));
  assert(split[0] == 0 && split[1] == -16384 && split[2] == 0 &&
         split[3] == -32768);
  size_t position = 7;
  int ended = 7;
  assert(bk_pcm_position(p, 44180, 4, 0, &position, &ended) && position == 2 &&
         ended);
  assert(bk_pcm_position(p, 44180, UINT64_MAX, 1, &position, &ended) &&
         position == 1 && !ended);
  float preserved[12];
  memcpy(preserved, split, sizeof(split));
  assert(!bk_pcm_mix(p, 44180, 0, 0, 1, 0, split, 6, error));
  assert(!memcmp(preserved, split, sizeof(split)));
  int16_t quantized[12];
  assert(bk_pcm_quantize(whole, quantized, 6));
  assert(quantized[0] == 32767 && quantized[1] == -32768 && quantized[8] == 0);
  wav[30] = 0;
  assert(v[0] == 32767);
  bk_pcm_destroy(p);
  for (size_t n = 0; n < sizeof(wav); n++)
    assert(!bk_pcm_decode(wav, n, error));
  uint8_t original[sizeof(wav)];
  memcpy(original, wav, sizeof(wav));
  const unsigned offsets[] = {4, 16, 26, 42, 46, 48, 50, 54, 58, 60};
  for (unsigned i = 0; i < sizeof(offsets) / sizeof(*offsets); i++) {
    memcpy(wav, original, sizeof(wav));
    word(wav + offsets[i], 0xffffffff);
    assert(!bk_pcm_decode(wav, sizeof(wav), error));
  }
  memcpy(wav, original, sizeof(wav));
  memcpy(wav + 64, "data", 4);
  assert(!bk_pcm_decode(wav, sizeof(wav), error));
  uint32_t random = 0x9234567;
  for (unsigned i = 0; i < 6000; i++) {
    memcpy(wav, original, sizeof(wav));
    random = random * 1664525 + 1013904223;
    wav[random % sizeof(wav)] ^= (uint8_t)(random >> 24);
    p = bk_pcm_decode(wav, sizeof(wav), error);
    bk_pcm_destroy(p);
  }
  bk_pcm_destroy(NULL);
  puts("PASS PCM16 chunk order, padding, stereo extrema, ownership, malformed "
       "bounds and6000 mutations");
}
