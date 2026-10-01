#include "media/pcm_mix.h"
#include <math.h>
#include <stdio.h>
static int valid(const BkPcm *pcm, uint32_t rate) {
  return pcm && bk_pcm_frames(pcm) && rate && rate <= 192000;
}
int bk_pcm_duration(const BkPcm *pcm, uint32_t rate, uint64_t *out) {
  if (!valid(pcm, rate) || !out)
    return 0;
  uint64_t source = bk_pcm_frames(pcm), hz = bk_pcm_rate(pcm);
  if (source > (UINT64_MAX - (hz - 1)) / rate)
    return 0;
  *out = (source * rate + hz - 1) / hz;
  return 1;
}
static void position(const BkPcm *pcm, uint32_t rate, uint64_t elapsed,
                     size_t *frame, uint32_t *fraction) {
  uint64_t count = bk_pcm_frames(pcm), hz = bk_pcm_rate(pcm);
  uint64_t partial = (elapsed % rate) * hz;
  /* PCM is RIFF-sized, so count<=2^31 and rate<=2^31: product fits64 bits.
   * Reduction before multiplication supports arbitrarily old looping clocks. */
  *frame = (((elapsed / rate) % count) * hz + partial / rate) % count;
  *fraction = (uint32_t)(partial % rate);
}
int bk_pcm_position(const BkPcm *pcm, uint32_t rate, uint64_t elapsed, int loop,
                    size_t *frame, int *ended) {
  uint64_t duration;
  if (!frame || !ended || (loop != 0 && loop != 1) ||
      !bk_pcm_duration(pcm, rate, &duration))
    return 0;
  int complete = !loop && elapsed >= duration;
  size_t f = bk_pcm_frames(pcm);
  uint32_t fraction;
  if (!complete)
    position(pcm, rate, elapsed, &f, &fraction);
  *frame = f;
  *ended = complete;
  return 1;
}
static double attenuation(int32_t value) {
  return value <= -10000 ? 0 : pow(10., (double)value / 2000.);
}
int bk_pcm_phase_advance(const BkPcm *pcm, uint32_t rate, uint32_t hz,
                         uint64_t elapsed, int loop, BkPcmPhase start,
                         BkPcmPhase *out) {
  if (!out || !valid(pcm, rate) || !hz || hz > 192000 ||
      (loop != 0 && loop != 1) || start.fraction >= rate)
    return 0;
  uint64_t count = bk_pcm_frames(pcm);
  if (start.frame > count || (start.frame == count && (loop || start.fraction)))
    return 0;
  if (!loop) {
    uint64_t remaining = (count - start.frame) * rate - start.fraction;
    uint64_t duration = (remaining + hz - 1) / hz;
    if (elapsed >= duration) {
      *out = (BkPcmPhase){(size_t)count, 0};
      return 1;
    }
  }
  uint64_t partial = (elapsed % rate) * hz + start.fraction;
  uint64_t whole = ((elapsed / rate) % count) * hz + partial / rate;
  *out = (BkPcmPhase){(size_t)((start.frame + whole) % count),
                      (uint32_t)(partial % rate)};
  return 1;
}
int bk_pcm_mix_phase(const BkPcm *pcm, uint32_t rate, uint32_t hz,
                     BkPcmPhase start, int loop, int32_t volume, int32_t pan,
                     float *out, size_t frames, char error[256]) {
  return bk_pcm_mix_phase_gain(pcm, rate, hz, start, loop, volume, pan, 1.f,
                                out, frames, error);
}
int bk_pcm_mix_phase_gain(const BkPcm *pcm, uint32_t rate, uint32_t hz,
                          BkPcmPhase start, int loop, int32_t volume, int32_t pan,
                          float baseline, float *out, size_t frames,
                          char error[256]) {
  BkPcmPhase checked;
  if (!isfinite(baseline) || baseline < 0 ||
      volume > 0 || volume < -10000 || pan < -10000 || pan > 10000 ||
      (frames && !out) || frames > SIZE_MAX / (2 * sizeof(*out)) ||
      !bk_pcm_phase_advance(pcm, rate, hz, 0, loop, start, &checked)) {
    snprintf(error, 256, "PCM mix: invalid input/format/phase");
    return 0;
  }
  for (size_t i = 0; i < frames * 2; i++)
    if (!isfinite(out[i])) {
      snprintf(error, 256, "PCM mix: nonfinite destination");
      return 0;
    }
  double gain = attenuation(volume) * baseline;
  double gains[2] = {gain * attenuation(pan > 0 ? -pan : 0),
                     gain * attenuation(pan < 0 ? pan : 0)};
  size_t at = checked.frame, count = bk_pcm_frames(pcm);
  uint32_t fraction = checked.fraction, channels = bk_pcm_channels(pcm);
  const int16_t *samples = bk_pcm_samples(pcm);
  for (size_t i = 0; i < frames && at < count; i++) {
    size_t next = at + 1 < count ? at + 1 : loop ? 0 : at;
    for (unsigned c = 0; c < 2; c++) {
      unsigned channel = channels == 1 ? 0 : c;
      double a = samples[at * channels + channel],
             b = samples[next * channels + channel];
      out[i * 2 + c] +=
          (float)((a + (b - a) * ((double)fraction / rate)) * gains[c]);
    }
    uint64_t phase = (uint64_t)fraction + hz;
    at += phase / rate;
    if (loop)
      at %= count;
    fraction = (uint32_t)(phase % rate);
  }
  return 1;
}
int bk_pcm_mix(const BkPcm *pcm, uint32_t rate, uint64_t elapsed, int loop,
               int32_t volume, int32_t pan, float *out, size_t frames,
               char error[256]) {
  BkPcmPhase phase;
  if (!bk_pcm_phase_advance(pcm, rate, bk_pcm_rate(pcm), elapsed, loop,
                            (BkPcmPhase){0}, &phase)) {
    snprintf(error, 256, "PCM mix: invalid clock/format");
    return 0;
  }
  return bk_pcm_mix_phase(pcm, rate, bk_pcm_rate(pcm), phase, loop, volume, pan,
                          out, frames, error);
}
int bk_pcm_quantize(const float *in, int16_t *out, size_t frames) {
  return bk_pcm_quantize_gain(in, out, frames, 1.f);
}
int bk_pcm_quantize_gain(const float *in, int16_t *out, size_t frames,
                          float gain) {
  if (!isfinite(gain) || gain < 0 || (frames && (!in || !out)) ||
      frames > SIZE_MAX / (2 * sizeof(*in)))
    return 0;
  for (size_t i = 0; i < frames * 2; i++)
    if (!isfinite(in[i]))
      return 0;
  for (size_t i = 0; i < frames * 2; i++) {
    float v = in[i] * gain;
    out[i] = v <= -32768 ? -32768 : v >= 32767 ? 32767 : (int16_t)lroundf(v);
  }
  return 1;
}
