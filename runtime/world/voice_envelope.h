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
#endif
