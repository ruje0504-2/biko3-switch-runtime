#ifndef BK_MEDIA_AUDIO_H
#define BK_MEDIA_AUDIO_H
#include "media/pcm.h"
#include "resource/store.h"
#define BK_AUDIO_VOICES                                                        \
  64 /* actors/props plus persistent system and UI voices */
typedef struct BkAudio BkAudio;
typedef struct BkAudioClip BkAudioClip;
typedef struct {
  void *context;
  void (*lock)(void *);
  void (*unlock)(void *);
} BkAudioSync;
/* Single-threaded push sink. submit copies/owns samples before returning and
 * is all-or-nothing. poll returns an absolute, monotonic count of consumed
 * stereo frames, never wall-clock estimates. Context outlives BkAudio.
 * Offline sinks must identify themselves as such to their caller. */
typedef struct {
  void *context;
  uint32_t rate, block_frames, capacity_frames;
  int (*submit)(void *, const int16_t *, size_t, char error[256]);
  int (*poll)(void *, uint64_t *, char error[256]);
} BkAudioSink;
typedef struct {
  uint64_t submitted, consumed, queue_drains;
  int failed;
} BkAudioStats;
typedef struct {
  /* Borrowed until the next cursor query for this voice or mixer destruction.
   * Poll/fill and commands do not invalidate it, including on a pump thread.
   * One caller owns cursor queries per voice. This is the audible epoch, which
   * can differ from the newest play/gain/clear command while data is queued. */
  const BkPcm *pcm;
  size_t source_frame;
  int playing, buffered, pending_change;
} BkAudioCursor;
BkAudioClip *bk_audio_clip_decode(const void *, size_t, char error[256]);
BkAudioClip *bk_audio_clip_load(BkResourceStore *, const char *pack,
                                const char *name, char error[256]);
void bk_audio_clip_release(BkAudioClip *);
uint32_t bk_audio_clip_rate(const BkAudioClip *);
BkAudio *bk_audio_create(const BkAudioSink *, char error[256]);
void bk_audio_destroy(BkAudio *);
/* Install/remove before starting/after joining the pump, with no concurrent
 * API calls. Hooks serialize every command, pump and status/cursor operation.
 * NULL restores single-threaded mode. Context outlives the installed hooks.
 * Destroy requires the pump already stopped. Sink calls must not reenter. */
int bk_audio_set_sync(BkAudio *, const BkAudioSync *, char error[256]);
/* Commands take effect at the next unsubmitted frame; queued data is immutable.
 * Same-boundary commands coalesce. play always restarts at source0 and retains
 * clip independently of the caller. clear releases a voice (not pause).
 * On allocation/argument failure commands leave state unchanged. */
int bk_audio_play(BkAudio *, unsigned voice, BkAudioClip *, int loop,
                  int32_t volume, int32_t pan, char error[256]);
int bk_audio_gain(BkAudio *, unsigned voice, int32_t volume, int32_t pan,
                  char error[256]);
/* Latest commanded gain, including pending or paused epochs. Does not poll,
 * alter cursor lifetime or report the gain of older queued samples. Missing
 * or cleared voices fail without modifying either output. */
int bk_audio_get_gain(const BkAudio *, unsigned voice, int32_t *volume,
                      int32_t *pan);
/* Change playback speed without restarting or changing queued samples.
 * Hz100..100000; zero restores the authored rate (DirectSound ORIGINAL).
 * Audible cursor follows each queued rate epoch, retaining fractional phase. */
int bk_audio_frequency(BkAudio *, unsigned voice, uint32_t hz, char error[256]);
/* Position-preserving secondary-buffer Stop/Play. Requires a loaded voice.
 * pause freezes the next unsubmitted source phase; resume keeps that phase,
 * gain and frequency and replaces the loop flag. Repeated resume while playing
 * does not rewind. After natural completion, resume starts at source0.
 * Audible cursor still follows already queued samples; latest status reports
 * pause immediately. Fractional resampling phase is retained while paused. */
int bk_audio_pause(BkAudio *, unsigned voice, char error[256]);
int bk_audio_resume(BkAudio *, unsigned voice, int loop, char error[256]);
int bk_audio_clear(BkAudio *, unsigned voice, char error[256]);
/* poll before game updates; fill afterwards. Sink failure is sticky and fatal
 * to this session. Only successful submit advances the submitted clock.
 * Output is interleaved signed16 stereo, mixed with deterministic voice order.
 */
int bk_audio_poll(BkAudio *, char error[256]);
int bk_audio_fill(BkAudio *, char error[256]);
int bk_audio_cursor(const BkAudio *, unsigned voice, BkAudioCursor *);
/* Status of the newest command: pending play/resume is playing, pause/clear is
 * stopped. Once audible, completion follows consumed frames. This is for
 * gameplay status gates; lip-sync must continue using the audible cursor. */
int bk_audio_playing(const BkAudio *, unsigned voice, int *playing);
BkAudioStats bk_audio_stats(const BkAudio *);
#endif
