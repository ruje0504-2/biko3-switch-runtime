#ifndef BK_GAME_ENDING_GALLERY_VOICE_H
#define BK_GAME_ENDING_GALLERY_VOICE_H
#include <stdint.h>
typedef struct {
  const uint8_t *group;       /*live721b3c, signed when formatting*/
  char (*speech_names)[32];   /*existing722224/722344, preserve strcpy tail*/
  const int32_t *volume;
} BkEndingGalleryVoiceBindings;
typedef struct {
  void *context;
  int (*random)(void *, int32_t *, char[256]);
  int (*load)(void *, unsigned, const char *, char[256]);
  int (*play)(void *, unsigned, int32_t flags, int32_t volume, char[256]);
} BkEndingGalleryVoiceOps;
/*Complete48c8c2. Mode uses its low byte; slots other than0/1 are native
 *no-ops. An unmatched action on slot0/1 has an undefined native cue and
 *fails explicitly. Group is read after RNG and volume after loading.*/
int bk_ending_gallery_voice(const BkEndingGalleryVoiceBindings *, int32_t action,
    int32_t mode, int32_t slot, int32_t flags,
    const BkEndingGalleryVoiceOps *, char error[256]);
#endif
