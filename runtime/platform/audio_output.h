#ifndef BK_PLATFORM_AUDIO_OUTPUT_H
#define BK_PLATFORM_AUDIO_OUTPUT_H
#include "media/audio.h"
#include <stdio.h>
typedef struct BkAudioOutput BkAudioOutput;
/* Native device, not an offline clock. Single lifecycle owner; one instance.
 * Open fills a borrowed sink. start installs serialized mixer access and
 * continuously pumps audio independently of rendering/resource loading.
 * Stop/join the pump before destroying the mixer, then close this output.
 * Closing stops/closes the native session before freeing queued buffers. */
BkAudioOutput *bk_audio_output_open(BkAudioSink *sink, FILE *log,
                                    char error[256]);
int bk_audio_output_start(BkAudioOutput *, BkAudio *, char error[256]);
int bk_audio_output_check(BkAudioOutput *, char error[256]);
void bk_audio_output_stop(BkAudioOutput *);
void bk_audio_output_close(BkAudioOutput *output);
#endif
