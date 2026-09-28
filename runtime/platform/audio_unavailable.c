#include "platform/audio_output.h"
BkAudioOutput *bk_audio_output_open(BkAudioSink *sink, FILE *log,
                                    char error[256]) {
  (void)sink;
  (void)log;
  snprintf(error, 256,
           "native audio output is not implemented on this host; "
           "use audio-probe for explicit offline validation");
  return NULL;
}
void bk_audio_output_close(BkAudioOutput *output) { (void)output; }
int bk_audio_output_start(BkAudioOutput *output, BkAudio *audio, char e[256]) {
  (void)output;
  (void)audio;
  snprintf(e, 256, "native audio output unavailable on this host");
  return 0;
}
int bk_audio_output_check(BkAudioOutput *output, char e[256]) {
  (void)output;
  snprintf(e, 256, "native audio output unavailable on this host");
  return 0;
}
void bk_audio_output_stop(BkAudioOutput *output) { (void)output; }
