#ifndef BK_SCENE_DIALOGUE_MEDIA_H
#define BK_SCENE_DIALOGUE_MEDIA_H
#include <stdint.h>
typedef struct {
  int32_t volume; /*728dd4*/
  uint8_t wanted; /*728dd8: only0/1 change volume; other bytes hold*/
} BkDialogueMusic;
typedef enum {
  BK_DIALOGUE_SPEECH_RELEASE,
  BK_DIALOGUE_SPEECH_LOAD,
  BK_DIALOGUE_SPEECH_PLAY,
  BK_DIALOGUE_MUSIC_GAIN,
  BK_DIALOGUE_MUSIC_PAUSE,
  BK_DIALOGUE_MUSIC_LOAD
} BkDialogueMediaKind;
typedef struct {
  BkDialogueMediaKind kind;
  const char *pack, *name; /* borrowed only for the service call */
  int32_t volume;
  int loop;
} BkDialogueMediaCommand;
typedef struct {
  void *context;
  int (*command)(void *, const BkDialogueMediaCommand *, char error[256]);
} BkDialogueMediaOps;
/*4f175d packed-resource branch: case-sensitive "se" selects bk3_02,
 * everything else bk3_06. Release/load stopped/restart0, then acknowledge.
 * Pending bytes other than1 are retained. Names must be bounded256 strings. */
int bk_dialogue_speech_step(uint8_t *pending, const char *name, int32_t master,
                            const BkDialogueMediaOps *, char error[256]);
/*4f0ce7..4f0d9c: optional looping music load at-6000, then initialize only
 * pending/wanted/volume. Empty name does not release an existing buffer. */
int bk_dialogue_music_open(BkDialogueMusic *, uint8_t *pending,
                           const char *name, const BkDialogueMediaOps *,
                           char error[256]);
/*4f18cd with50db23: advance old wanted, submit gain even for a held wanted
 * byte, then process pending. Empty new name pauses and retains the buffer.
 * Invalid inputs reject before mutation; service failure retains the executed
 * prefix and must terminate the frame. Missing services never imply success. */
int bk_dialogue_music_step(BkDialogueMusic *, int present, uint8_t *pending,
                           const char *name, float game_seconds, int32_t master,
                           const BkDialogueMediaOps *, char error[256]);
#endif
