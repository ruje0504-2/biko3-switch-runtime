#include "game/ending_gallery_voice.h"
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  if (e) snprintf(e, 256, "gallery voice: %s", why);
  return 0;
}
int bk_ending_gallery_voice(const BkEndingGalleryVoiceBindings *b, int32_t action,
    int32_t mode, int32_t slot, int32_t flags,
    const BkEndingGalleryVoiceOps *o, char e[256]) {
  if (!b || !b->group || !b->speech_names || !b->volume || !o)
    return fail(e, "invalid bindings");
  if (slot != 0 && slot != 1) return 1;
  if ((uint32_t)action > 7) return fail(e, "native cue is uninitialized");
  static const int32_t initial[] = {1, 12, 20, 28, 36, 44, 44, 52};
  static const int32_t loop[] = {4, 15, 23, 31, 39, 47, 47, 55};
  static const int32_t secondary[] = {6, 17, 25, 33, 41, 49, 49, 57};
  int32_t cue;
  if (!slot) {
    int32_t random;
    if (!o->random) return fail(e, "missing random service");
    if (!o->random(o->context, &random, e)) return 0;
    cue = ((uint32_t)mode & 255u) ? loop[action] + random % 2
                                : initial[action] + random % 3;
  } else cue = secondary[action];
  int group = *b->group;
  if (group >= 128) group -= 256;
  char name[32];
  snprintf(name, sizeof(name), "PH%d02%02d.wav", group + 1, cue);
  memcpy(b->speech_names[slot], name, strlen(name) + 1);
  if (!o->load) return fail(e, "missing load service");
  if (!o->load(o->context, (unsigned)slot, name, e)) return 0;
  if (!o->play) return fail(e, "missing play service");
  return o->play(o->context, (unsigned)slot, flags, *b->volume, e);
}
