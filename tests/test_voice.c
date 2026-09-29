#include "world/voice_envelope.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  char error[256];
  uint8_t a[443], b[442];
  for (unsigned i = 0; i < 442; i += 2) {
    a[i] = b[i] = 0;
    a[i + 1] = b[i + 1] = 128;
  }
  int32_t level = -1;
  for (unsigned split = 0; split <= 442; split++) {
    assert(bk_voice_pcm_level(a, split, b, 442 - split, &level, error));
    assert(level == 65536);
  }
  memset(a, 0, sizeof(a));
  a[440] = 255;
  a[441] = 127;
  assert(bk_voice_pcm_level(a, sizeof(a), NULL, 0, &level, error));
  assert(level == 0);
  assert(!bk_voice_pcm_level(a, 2, b, 437, &level, error));
  assert(level == 0);
  assert(!bk_voice_pcm_level(NULL, 440, b, 440, &level, error));
  assert(!bk_voice_pcm_level(a, 0, NULL, 440, &level, error));
  assert(bk_voice_pcm_level(NULL, 0, b, 440, &level, error));
  int16_t ring[221];
  int32_t total = 0;
  for (unsigned i = 0; i < 221; i++) {
    ring[i] = (int16_t)((int)i * 257 - 28000);
    total += ring[i] < 0 ? -(int32_t)ring[i] : ring[i];
  }
  ring[0] = INT16_MIN;
  total += 32768 - 28000;
  for (size_t start = 0; start < 221; start++) {
    int sampled = 0;
    assert(bk_ending_voice_pcm_level(ring, 221, start, &sampled, &level,
                                     error));
    /* A full442-byte ring omits exactly the sample before the cursor. */
    int32_t omitted = ring[start ? start - 1 : 220];
    if (omitted < 0)
      omitted = -omitted;
    assert(sampled && level == (total - omitted) / 110);
  }
  int sampled = 9;
  for (size_t count = 0; count <= 220; count++) {
    assert(bk_ending_voice_pcm_level(ring, count, 0, &sampled, &level,
                                     error));
    assert(!sampled && !level);
  }
  int16_t tail[224] = {0};
  tail[0] = INT16_MIN;
  tail[220] = INT16_MAX;
  for (size_t count = 222; count <= 224; count++) {
    for (size_t start = 0; start <= count; start++) {
      assert(bk_ending_voice_pcm_level(tail, count, start, &sampled,
                                       &level, error));
      /*444/446/448-byte buffers admit only1/2/3 cursor positions. */
      assert(sampled == (start < count - 221));
      assert(level == (sampled ? (start ? 32767 : 32768) / 110 : 0));
    }
  }
  sampled = 7;
  level = 13;
  assert(!bk_ending_voice_pcm_level(NULL, 221, 0, &sampled, &level, error));
  assert(sampled == 7 && level == 13);
  assert(!bk_ending_voice_pcm_level(ring, 221, 0, NULL, &level, error));
  assert(level == 13);
  assert(bk_ending_voice_pcm_level(NULL, 0, 0, &sampled, &level, error));
  assert(!sampled && !level);
  BkVoiceEnvelope state = {3, 2};
  float output = 99;
  assert(bk_voice_envelope_step(&state, 0, -1, 1, &output, error));
  assert(output == 0 && state.target == 3 && state.smoothed == 2);
  assert(bk_voice_envelope_step(&state, 1, 65536, 1, &output, error));
  assert(output == 9 && state.target == 128);
  assert(bk_voice_envelope_step(&state, 1, 512, 1, &output, error));
  assert(output == 1 && state.target == 1);
  BkVoiceEnvelope held = state;
  assert(!bk_voice_envelope_step(&state, 1, 65537, 1, &output, error));
  assert(!bk_voice_envelope_step(&state, 1, -1, 1, &output, error));
  assert(!bk_voice_envelope_step(&state, 1, 0, NAN, &output, error));
  assert(!bk_voice_envelope_step(&state, 0, 0, -1, &output, error));
  assert(!memcmp(&held, &state, sizeof(held)) && output == 1);
  BkEndingVoiceEnvelope ending = {3, 2}, previous = ending;
  assert(bk_ending_voice_envelope_step(&ending, 0, 0, 0, &output, error));
  assert(output == 0 && !memcmp(&ending, &previous, sizeof(ending)));
  assert(bk_ending_voice_envelope_step(&ending, 1, 0, 0, &output, error));
  assert(ending.target == 3 && ending.smoothed > 2 && ending.smoothed < 3);
  assert(bk_ending_voice_envelope_step(&ending, 1, 1, 511, &output, error));
  assert(ending.target == 0);
  assert(bk_ending_voice_envelope_step(&ending, 1, 1, 512, &output, error));
  assert(ending.target == .5f);
  assert(bk_ending_voice_envelope_step(&ending, 1, 1, 65536, &output, error));
  assert(ending.target == 64 && output == 9);
  previous = ending;
  assert(!bk_ending_voice_envelope_step(&ending, 1, 1, -1, &output, error));
  assert(!memcmp(&ending, &previous, sizeof(ending)) && output == 9);
  puts("PASS: PCM span bounds, signed16 extremes, ignored tail, envelope hold/clamp and rollback");
  return 0;
}
