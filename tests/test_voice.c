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
  puts("PASS: PCM span bounds, signed16 extremes, ignored tail, envelope hold/clamp and rollback");
  return 0;
}
