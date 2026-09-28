#ifndef BK_MEDIA_PCM_H
#define BK_MEDIA_PCM_H
#include <stddef.h>
#include <stdint.h>
typedef struct BkPcm BkPcm;
/* Owned immutable RIFF/WAVE PCM16, mono or stereo, interleaved host int16.
 * Preserves the exact authored sample rate. Strict RIFF/chunk/block bounds;
 * unknown chunks are skipped with word padding. No streaming/native API.
 * Input can be released immediately. Empty/compressed/ambiguous files fail. */
BkPcm *bk_pcm_decode(const void *bytes, size_t size, char error[256]);
void bk_pcm_destroy(BkPcm *pcm);
uint32_t bk_pcm_rate(const BkPcm *pcm);
uint32_t bk_pcm_channels(const BkPcm *pcm);
size_t bk_pcm_frames(const BkPcm *pcm);
const int16_t *bk_pcm_samples(const BkPcm *pcm);
#endif
