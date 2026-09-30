#ifndef BK_SCENE_SYSTEM_AUDIO_H
#define BK_SCENE_SYSTEM_AUDIO_H
#include "media/audio.h"
typedef struct BkSystemAudio BkSystemAudio;
/* Native BEEE10+slot*0x120 handles, zero-based. NULL outside0..7. */
const char *bk_system_audio_name(unsigned slot);
/* Retained global bufferBEF050, initialized by4e6dee as bk3_02/se002.wav.
 * Shared across NPC waits and UI; lifetime is above individual entries.
 * The default constructor selects that buffer.
 * Caller reserves its distinct voice. Volume is captured at initialization. */
BkSystemAudio *bk_system_audio_create(BkResourceStore *, BkAudio *,
                                      unsigned voice, int32_t effect_volume,
                                      char error[256]);
/*4e6dee slots0..7 = se000..se006,se099. Each instance owns one distinct
 * reserved voice, with load-time volume. create() remains the wait slot2. */
BkSystemAudio *bk_system_audio_create_slot(BkResourceStore *, BkAudio *,
                                           unsigned voice, unsigned slot,
                                           int32_t volume, char error[256]);
int bk_system_audio_restart(BkSystemAudio *, char error[256]);
void bk_system_audio_destroy(BkSystemAudio *);
int bk_system_audio_wait(BkSystemAudio *, char error[256]);
int bk_system_audio_stop(BkSystemAudio *, char error[256]);
#endif
