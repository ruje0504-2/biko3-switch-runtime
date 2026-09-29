#ifndef BK_GAME_ENDING_AUXILIARY_H
#define BK_GAME_ENDING_AUXILIARY_H
#include "game/ending_frame.h"
/* Additional live globals used by495d92, kept apart from dispatcher state.
 * The entry/stage owner supplies them; zeroing these is not initialization. */
typedef struct {
  int32_t gate, variant, selection, base; /*721ef0/721e04/7220f4/6ea024*/
  int32_t index, pending, direction;      /*721edc/722100/6ea35c*/
  float progress;                         /*721e20*/
  int32_t expression_a, expression_b;     /*721df4/721df0*/
} BkEndingAuxiliaryState;
typedef enum {
  BK_ENDING_CLIP_CHAIN,
  BK_ENDING_CLIP_NEXT,
  BK_ENDING_CLIP_REWIND /*source=start, no selection/advance/publication*/
} BkEndingClipWrite;
typedef enum {
  BK_ENDING_AUDIO_STATUS,
  BK_ENDING_AUDIO_PAUSE,
  BK_ENDING_AUDIO_RESTART,
  BK_ENDING_AUDIO_VOICE, /*4946b4(cue,slot,flags), selection from state*/
  BK_ENDING_AUDIO_CUE    /*49490a(cue,bank,slot,flags)*/
} BkEndingAudioOperation;
typedef struct {
  BkEndingAudioOperation operation;
  unsigned slot;
  int32_t cue, bank, flags, volume;
} BkEndingAudioCall;
typedef struct {
  void *context;
  int (*active)(void *, int32_t *slot, char error[256]);
  int (*write)(void *, unsigned slot, BkEndingClipWrite, int32_t value,
               char error[256]);
  int (*request)(void *, unsigned slot, char error[256]); /*4018c8*/
  int (*audio)(void *, const BkEndingAudioCall *, int *playing,
               char error[256]);
  int (*eyes)(void *, unsigned slot, char error[256]); /*4a07a9*/
} BkEndingAuxiliaryOps;
/* Independent process values55469c/55696d. The scale is also shared with
 * the adjacent auxiliary playback stages; retain both across scene reloads.
 * Initial image values are .02f and10, not a zero-filled scene default. */
typedef struct {
  float scale;
  int8_t countdown;
} BkEndingAuxiliaryCycle;
BkEndingAuxiliaryCycle bk_ending_auxiliary_cycle_initial(void);
typedef struct {
  int32_t duration;
  float end, source, rate;
} BkEndingAuxiliaryPrediction;
typedef struct {
  BkEndingAuxiliaryOps auxiliary;
  int (*prediction)(void *, unsigned slot, BkEndingAuxiliaryPrediction *,
                     char error[256]);
  int (*random)(void *, int32_t *, char error[256]);
} BkEndingAuxiliaryTickOps;
/* Complete4965b9 automatic alternation, distinct from495d92 mode requests.
 * Clears cached camera/event before checking active7/11. Preserve the two
 * native look-ahead multiplication orders, signed-byte countdown wrap,
 * random%11+10, configured requests13/14, ordered source rewinds and face
 * changes. Live aliases are reread after services. No animation advance,
 * world publication, mode assignment or gate test. Failure keeps its prefix. */
int bk_ending_auxiliary_tick(BkEndingAuxiliaryState *, BkEndingFrameState *,
                             BkEndingAuxiliaryCycle *, float seconds,
                             const int32_t *voice_volume,
                             const BkEndingAuxiliaryTickOps *, char error[256]);
/* Complete495d92, including ordered clip writes, gates, state and services.
 * Result is native EAX (rejected0/accepted1), separate from portable failure.
 * Invalid proposals are accepted no-ops AFTER the native gate. The function
 * never writes frame.auxiliary_mode; the common controller does that later.
 * A failed service preserves its ordered prefix and terminates this frame. */
int bk_ending_auxiliary_change(BkEndingAuxiliaryState *, BkEndingFrameState *,
                               int32_t proposed, int32_t voice_volume,
                               int32_t effect_volume,
                               const BkEndingAuxiliaryOps *, int32_t *result,
                               char error[256]);
/* Native4946b4/49490a resource identity, including group0's special cue30.
 * Supports packaged source layout; loose Windows directories are not used.
 * flags retain raw DirectSound bits; the audio backend uses LOOPING bit0. */
int bk_ending_audio_resource(unsigned group, int32_t variant, int32_t selection,
                             const BkEndingAudioCall *, char pack[16],
                             char name[32], char error[256]);
#endif
