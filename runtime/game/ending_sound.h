#ifndef BK_GAME_ENDING_SOUND_H
#define BK_GAME_ENDING_SOUND_H
#include <stdint.h>
enum {
  BK_ENDING_EFFECTS = 45,
  BK_ENDING_SOUND_MUSIC = 47,
  BK_ENDING_SOUND_BUFFERS = 48
};
/*4cc582 packaged bank. Buffers0/1 are speech; effect i is buffer2+i.
 * Auxiliary audio's slots2..5 alias effects0..3. Music is a separate buffer.
 * The last four native table reads run into non-name data and find no asset;
 * NULL represents those observed absent buffers, never a missing valid file. */
const char *bk_ending_sound_music(unsigned group, unsigned variant);
const char *bk_ending_sound_effect(unsigned index);
/*4e0032 and4e0818 filename selection. Output is unchanged on rejection.
 * Native uninitialized target/mode paths and out-of-table indices reject.
 * Contact table -1 entries retain their literal filename; this is not a
 * promise that a playable resource exists. Native filename-cache writes
 * belong to the caller, before the actual load. */
int bk_ending_sound_normal_voice(unsigned group, int32_t target, int32_t mode,
                                 char name[32], char error[256]);
int bk_ending_sound_contact_voice(unsigned group, int32_t kind, int32_t index,
                                  int32_t alternate, char name[32],
                                  char error[256]);
/*4dfbbd/4dfca9. The phase1 loop builder requires progress >= .19;
 * other phases and the original uninitialized cue paths reject. Builders
 * return the resource name only; caller commits722224 before loading. */
int bk_ending_sound_loop_name(unsigned group, int32_t phase, float progress,
                               char name[32], char error[256]);
int bk_ending_sound_action_name(unsigned group, int32_t target, float progress,
                                 char name[32], char error[256]);
/*479739 filename selection. Any nonzero select uses31, zero uses32;
 * its bank argument has no effect. The caller stores the name in the chosen
 * speech lane BEFORE the actual load and performs Play separately. */
int bk_ending_sound_tertiary_voice(unsigned group, int32_t cue, int32_t select,
                                   char name[32], char error[256]);
/* Literal table outputs absent in the retained Japanese bk3_06 archive.
 * Only an actual MISSING result may use the native empty-buffer behavior;
 * an existing replacement still loads, and corruption remains fatal. */
int bk_ending_sound_absent_speech(const char *name);
typedef struct {
  int32_t direction, master;
  float seconds;
  uint8_t present[2];
} BkEndingDuckInput;
typedef struct {
  void *context;
  /* A native failed GetStatus maps to playing=0; infrastructure failure is
   * separate. Volume returns the4643e4 result (initialized0 on failed read).
   * gain observes the request even when DirectSound would reject its range;
   * native HRESULT is ignored, portable service failure is not. */
  int (*status)(void *, unsigned slot, int *playing, char error[256]);
  int (*volume)(void *, int32_t *volume, char error[256]);
  int (*gain)(void *, int32_t volume, char error[256]);
} BkEndingDuckOps;
/* Complete4e01e4: speech0 is attenuated while speech1 plays. Direction==1
 * decreases; every other value increases. It re-queries status independently
 * of the caller's direction. transition(719c5c) survives early returns and
 * reentry. Two ordered volume reads are intentional. Native EAX is result,
 * not the portable success return. Failed services preserve the prefix. */
int bk_ending_sound_duck(int32_t *transition, const BkEndingDuckInput *,
                         const BkEndingDuckOps *, int32_t *result,
                         char error[256]);
#endif
