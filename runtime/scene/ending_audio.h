#ifndef BK_SCENE_ENDING_AUDIO_H
#define BK_SCENE_ENDING_AUDIO_H
#include "game/ending_auxiliary.h"
#include "game/ending_sound.h"
#include "scene/voice_audio.h"
typedef struct BkEndingAudio BkEndingAudio;
/* Owns six decoded buffers (722334+slot*120), borrows six distinct contiguous
 * mixer slots and the store. Actual ending loader must bind initial sounds;
 * create clears slots but does not invent any clip. */
BkEndingAudio *bk_ending_audio_create(BkResourceStore *, BkAudio *,
                                      unsigned first, char error[256]);
/*4cc582 audio stage: reserves48 contiguous mixer slots, loads the selected
 * music plus41 valid effects, then starts looping music. Slots0/1 and the
 * four observed absent effects remain unbound. Slots2..5 are the SAME buffers
 * used by auxiliary commands; do not create a second six-buffer owner.
 * Caller must retire any previous owner of these mixer slots first. Missing
 * valid assets fail, and failed loading never starts music. This does not
 * perform the rest of ending entry or initialize the retained duck latch. */
BkEndingAudio *bk_ending_audio_create_entry(BkResourceStore *, BkAudio *,
                                            unsigned first, unsigned group,
                                            unsigned variant,
                                            int32_t music_volume,
                                            char error[256]);
int bk_ending_audio_present(const BkEndingAudio *, unsigned slot, int *present);
int bk_ending_audio_stop(BkEndingAudio *, char error[256]);
/*4cfede/4d0dca release only their two speech buffers. The shared effect
 * bank, music cursor/gain, queued PCM and other mixer users remain owned by
 * the surrounding ending session. Clearing0 succeeds before clearing1;
 * a later backend failure retains that completed prefix. */
int bk_ending_audio_release_speech(BkEndingAudio *, char error[256]);
void bk_ending_audio_destroy(BkEndingAudio *);
/* Replace a stopped buffer. Clears/releases the prior buffer before loading,
 * as4e0956. A missing asset is fatal, not a successful silent replacement. */
int bk_ending_audio_bind(BkEndingAudio *, unsigned slot, const char *pack,
                         const char *name, char error[256]);
/*4e0956 alone: clear the previous slot and load named speech without Play.
 * Keep its separate call boundary: parent controllers can read live volume
 * after loading, and a failed subsequent play must leave this buffer loaded.
 * Uses the same143 verified absent-name policy as the combined helper. */
int bk_ending_audio_load_speech(BkEndingAudio *, unsigned slot,
                                const char *name, char error[256]);
/*4e0956 +4ad2bf named speech, same existing slots0/1. Caller has already
 * committed the retained filename. Load failure keeps the old slot cleared.
 * The143 proven absent Japanese table names retain the native null buffer;
 * an existing corrupt file or any other missing name still fails. */
int bk_ending_audio_speech(BkEndingAudio *, unsigned slot, const char *name,
                           int32_t volume, char error[256]);
/* 481E0A's state1 path: PH(group+1)33cue, loaded into the speech slot and
 * immediately restarted with the live voice volume. */
int bk_ending_audio_auxiliary_voice(BkEndingAudio *, unsigned group,
                                    int32_t cue, unsigned slot,
                                    int32_t volume, char error[256]);
/* STATUS has missing-buffer=false, PAUSE/RESTART retain native null no-op.
 * RESTART rewinds, PAUSE keeps cursor, VOICE/CUE load then restart. Bit0 of
 * raw Play flags controls looping; Windows buffer-placement flags do not
 * change the portable PCM output. Special4946b4 group0 cue30 targets slot1. */
int bk_ending_audio_call(BkEndingAudio *, unsigned group, int32_t variant,
                         int32_t selection, const BkEndingAudioCall *,
                         int *playing, char error[256]);
/* Actual4ad5a4/4ad363: independent ending envelope and two status reads.
 * Read220 interleaved PCM samples at the consumed cursor, preserving the
 * unsigned near-end exclusion (offset >= buffer_bytes-443). Exactly442-byte
 * buffers may split the lock across their end; shorter buffers cannot lock.
 * A playing buffer without a usable sample smooths toward the old target. */
int bk_ending_audio_level(BkEndingAudio *, unsigned slot,
                          BkEndingVoiceEnvelope *, float *level,
                          char error[256]);
/* Actual4e01e4 adapter. Changes only speech0 gain, preserves pan, source phase,
 * queued samples and independent entry music. Out-of-range native SetVolume
 * requests retain gain as rejected DirectSound commands; backend failure
 * still terminates the frame. */
int bk_ending_audio_duck(BkEndingAudio *, int32_t *transition,
                         int32_t direction, int32_t voice_master,
                         float game_seconds, int32_t *result, char error[256]);
#endif
