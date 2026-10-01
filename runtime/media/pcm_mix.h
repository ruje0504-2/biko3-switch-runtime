#ifndef BK_MEDIA_PCM_MIX_H
#define BK_MEDIA_PCM_MIX_H
#include "media/pcm.h"
/* Stateless resampling: elapsed counts OUTPUT frames since this voice's
 * current start/seek origin. Backends must use submitted frames for mixing
 * and actual consumed frames for audible position/envelope. These are not
 * interchangeable. Source PCM is borrowed only during each call. */
int bk_pcm_duration(const BkPcm *pcm, uint32_t output_rate, uint64_t *frames);
/* Non-loop ended cursor equals source frame count. Loop cursor wraps. */
int bk_pcm_position(const BkPcm *pcm, uint32_t output_rate, uint64_t elapsed,
                    int loop, size_t *source_frame, int *ended);
/* Adds to initialized, finite stereo FLOAT samples in signed16 amplitude
 * units; caller clips/quantizes once after mixing all voices. Volume[-10000,0]
 * and pan[-10000,10000] follow DirectSound attenuation units. Mono duplicates
 * to both sides; stereo retains channels. Linear interpolation and integer
 * rational phase are portable policy, not bit-identical Windows driver DSP.
 * Loop interpolation wraps, non-loop holds its last frame until duration.
 * Zero frames is valid; invalid input leaves destination unchanged. */
int bk_pcm_mix(const BkPcm *pcm, uint32_t output_rate, uint64_t elapsed,
               int loop, int32_t volume, int32_t pan, float *stereo,
               size_t frames, char error[256]);
/* Piecewise-frequency playback keeps an exact source phase whose fraction
 * denominator is output_rate. Changing Hz must retain this phase. */
typedef struct {
  size_t frame;
  uint32_t fraction;
} BkPcmPhase;
int bk_pcm_phase_advance(const BkPcm *, uint32_t output_rate, uint32_t hz,
                         uint64_t elapsed, int loop, BkPcmPhase start,
                         BkPcmPhase *out);
int bk_pcm_mix_phase(const BkPcm *, uint32_t output_rate, uint32_t hz,
                     BkPcmPhase, int loop, int32_t volume, int32_t pan,
                     float *stereo, size_t frames, char error[256]);
/* Linear clip baseline applied with attenuation, before voices are summed. */
int bk_pcm_mix_phase_gain(const BkPcm *, uint32_t output_rate, uint32_t hz,
                          BkPcmPhase, int loop, int32_t volume, int32_t pan,
                          float gain, float *stereo, size_t frames,
                          char error[256]);
/* Saturating nearest-integer (ties away from0) stereo output conversion. */
int bk_pcm_quantize(const float *stereo, int16_t *out, size_t frames);
/* Apply the output gain before the same final saturation/quantization. */
int bk_pcm_quantize_gain(const float *stereo, int16_t *out, size_t frames,
                          float gain);
#endif
