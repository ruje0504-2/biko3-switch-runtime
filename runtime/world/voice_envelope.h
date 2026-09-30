#ifndef BK_WORLD_VOICE_ENVELOPE_H
#define BK_WORLD_VOICE_ENVELOPE_H
#include <stddef.h>
#include <stdint.h>
/* Original shared 0x708878/0x70887c state, owned explicitly by the caller.
 * This is animation input processing, not an audio decoder/player. */
typedef struct {
  float target, smoothed;
} BkVoiceEnvelope;
/* 0x4aef4e's locked PCM spans, signed16 little-endian. Sum the absolute
 * values of exactly220 samples and divide by110 with integer truncation.
 * Native locks442 bytes but ignores the final sample. A split span discards
 * any odd trailing byte in the first span, as the original loop does.
 * Caller supplies spans at the actual playback cursor; no guessed clock. */
int bk_voice_pcm_level(const void *first, size_t first_bytes,
                        const void *second, size_t second_bytes,
                        int32_t *level, char error[256]);
/* 0x4af18f: level/512, slew at10 units/sec and cap at9. Failed/inactive
 * audio returns0 without resetting the shared envelope. available is0/1;
 * level is ignored when unavailable. Invalid input leaves state/out intact. */
int bk_voice_envelope_step(BkVoiceEnvelope *state, int available,
                            int32_t level, float seconds, float *out,
                            char error[256]);
/*4af2d1 uses the SAME708878/7c owner but takes a direct target without
 * PCM scaling; it retains the cap at9. Invalid input leaves state/out intact.*/
int bk_voice_envelope_target(BkVoiceEnvelope *, float target, float seconds,
                              float *out, char error[256]);
/*4ad363 over decoded host-endian PCM16, indexed in interleaved samples.
 * The native DWORD guard is offset < buffer_bytes-443. A221-sample buffer
 * underflows that subtraction and admits a442-byte lock, possibly split
 * across its end; shorter buffers cannot supply that lock. Larger buffers
 * retain the strict native tail exclusion. Exactly220 samples contribute.
 * Unavailable windows return success with sampled=0 and magnitude=0.
 * Invalid pointers leave both outputs unchanged. No playback or allocation. */
int bk_ending_voice_pcm_level(const int16_t *samples, size_t sample_count,
                               size_t source_sample, int *sampled,
                               int32_t *magnitude, char error[256]);
/* Independent4ad5a4 globals708840/708844. Ending speech truncates the PCM
 * magnitude by512 before halving it, then interpolates by the native .35
 * coefficient once per call. It does not use the gameplay delta or its
 * 708878/70887c state. Inactive keeps both values and returns0; a failed PCM
 * sample while playing still smooths toward the retained target. */
typedef struct {
  float target, smoothed;
} BkEndingVoiceEnvelope;
int bk_ending_voice_envelope_step(BkEndingVoiceEnvelope *, int playing,
                                   int sampled, int32_t magnitude, float *out,
                                   char error[256]);
#endif
