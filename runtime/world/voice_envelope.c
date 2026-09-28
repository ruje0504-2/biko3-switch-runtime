#include "world/voice_envelope.h"
#include <math.h>
#include <stdio.h>
static int fail(char *error, const char *message) {
  snprintf(error, 256, "voice envelope: %s", message);
  return 0;
}
static int32_t absolute_sample(const uint8_t *p) {
  uint32_t word = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
  int32_t sample = word >= 32768 ? (int32_t)word - 65536 : (int32_t)word;
  return sample < 0 ? -sample : sample;
}
int bk_voice_pcm_level(const void *first, size_t first_bytes,
                        const void *second, size_t second_bytes,
                        int32_t *level, char error[256]) {
  size_t first_count = first_bytes / 2;
  if (first_count > 220)
    first_count = 220;
  size_t second_count = 220 - first_count;
  if (!level || (first_count && !first) || (second_count && !second) ||
      second_bytes / 2 < second_count)
    return fail(error, "incomplete PCM window");
  const uint8_t *a = first, *b = second;
  int32_t sum = 0;
  for (size_t i = 0; i < first_count; i++)
    sum += absolute_sample(a + i * 2);
  for (size_t i = 0; i < second_count; i++)
    sum += absolute_sample(b + i * 2);
  *level = sum / 110;
  return 1;
}
int bk_voice_envelope_step(BkVoiceEnvelope *state, int available,
                            int32_t level, float seconds, float *out,
                            char error[256]) {
  if (!state || !out || (available != 0 && available != 1) ||
      !isfinite(state->target) || !isfinite(state->smoothed) ||
      !isfinite(seconds) || seconds < 0 ||
      (available && (level < 0 || level > 65536)))
    return fail(error, "invalid input");
  if (!available) {
    *out = 0;
    return 1;
  }
  BkVoiceEnvelope next = *state;
  next.target = (float)((double)level / 512);
  if (next.target < next.smoothed) {
    next.smoothed = (float)((double)next.smoothed - 10.0 * seconds);
    if (next.target >= next.smoothed)
      next.smoothed = next.target;
  } else if (next.target > next.smoothed) {
    next.smoothed = (float)((double)next.smoothed + 10.0 * seconds);
    if (next.target <= next.smoothed)
      next.smoothed = next.target;
  } else {
    next.smoothed = next.target;
  }
  if (next.smoothed >= 9)
    next.smoothed = 9;
  *state = next;
  *out = next.smoothed;
  return 1;
}
