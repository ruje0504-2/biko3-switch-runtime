#include "scene/voice_audio.h"
int bk_scene_voice_envelope(BkAudio *audio, unsigned voice,
                            BkVoiceEnvelope *shared, float seconds,
                            float *level, char error[256]) {
  BkAudioCursor cursor;
  if (!bk_audio_cursor(audio, voice, &cursor)) {
    snprintf(error, 256, "voice audio: cannot read played cursor");
    return 0;
  }
  int32_t magnitude = 0;
  int available = cursor.playing && cursor.buffered;
  if (available) {
    size_t channels = bk_pcm_channels(cursor.pcm);
    size_t count = bk_pcm_frames(cursor.pcm) * channels;
    available = count >= 221; /* Native lock requests442 bytes. */
    if (available) {
      uint8_t window[440];
      size_t offset = cursor.source_frame * channels;
      const int16_t *samples = bk_pcm_samples(cursor.pcm);
      for (size_t i = 0; i < 220; ++i) {
        uint16_t sample = (uint16_t)samples[(offset + i) % count];
        window[i * 2] = (uint8_t)sample;
        window[i * 2 + 1] = (uint8_t)(sample >> 8);
      }
      if (!bk_voice_pcm_level(window, sizeof(window), NULL, 0, &magnitude,
                              error))
        return 0;
    }
  }
  return bk_voice_envelope_step(shared, available, magnitude, seconds, level,
                                error);
}
