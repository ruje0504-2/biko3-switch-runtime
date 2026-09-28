#include "scene/dialogue_media.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fail(char e[256]) {
  snprintf(e, 256, "dialogue media: invalid state/services/time/name");
  return 0;
}
static int valid(uint8_t *pending, const char *name,
                 const BkDialogueMediaOps *ops) {
  return pending && name && memchr(name, 0, 256) && ops && ops->command;
}
static int command(const BkDialogueMediaOps *ops, BkDialogueMediaKind kind,
                   const char *pack, const char *name, int32_t volume, int loop,
                   char e[256]) {
  BkDialogueMediaCommand c = {kind, pack, name, volume, loop};
  return ops->command(ops->context, &c, e);
}
int bk_dialogue_speech_step(uint8_t *pending, const char *name, int32_t master,
                            const BkDialogueMediaOps *ops, char e[256]) {
  if (!valid(pending, name, ops) || master < -10000 || master > 0)
    return fail(e);
  if (*pending != 1)
    return 1;
  const char *pack = !strncmp(name, "se", 2) ? "bk3_02" : "bk3_06";
  if (!command(ops, BK_DIALOGUE_SPEECH_RELEASE, NULL, NULL, 0, 0, e) ||
      !command(ops, BK_DIALOGUE_SPEECH_LOAD, pack, name, master, 0, e) ||
      !command(ops, BK_DIALOGUE_SPEECH_PLAY, NULL, NULL, 0, 0, e))
    return 0;
  *pending = 0;
  return 1;
}
int bk_dialogue_music_open(BkDialogueMusic *s, uint8_t *pending,
                           const char *name, const BkDialogueMediaOps *ops,
                           char e[256]) {
  if (!s || !valid(pending, name, ops))
    return fail(e);
  if (*name &&
      !command(ops, BK_DIALOGUE_MUSIC_LOAD, "bk3_02", name, -6000, 1, e))
    return 0;
  *pending = 0;
  s->wanted = 1;
  s->volume = -6000;
  return 1;
}
int bk_dialogue_music_step(BkDialogueMusic *s, int present, uint8_t *pending,
                           const char *name, float seconds, int32_t master,
                           const BkDialogueMediaOps *ops, char e[256]) {
  if (!s || !valid(pending, name, ops) || (present != 0 && present != 1) ||
      !isfinite(seconds) || seconds < 0 ||
      (double)(float)((double)seconds * 800) > INT32_MAX - 10000 ||
      master < -10000 || master > 0 || s->volume < -10000 || s->volume > 0)
    return fail(e);
  if (present) {
    int32_t delta = (int32_t)(float)((double)seconds * 800);
    if (delta <= 1)
      delta = 1;
    if (s->wanted == 1) {
      s->volume += delta;
      if (s->volume >= master)
        s->volume = master;
    } else if (s->wanted == 0) {
      s->volume -= delta;
      if (s->volume <= -6000)
        s->volume = -6000;
    }
    if (!command(ops, BK_DIALOGUE_MUSIC_GAIN, NULL, NULL, s->volume, 0, e))
      return 0;
  }
  if (*pending == 1) {
    s->wanted = 0;
    if (s->volume <= -6000) {
      if (present &&
          !command(ops, BK_DIALOGUE_MUSIC_PAUSE, NULL, NULL, 0, 0, e))
        return 0;
      if (*name) {
        if (!command(ops, BK_DIALOGUE_MUSIC_LOAD, "bk3_02", name, -6000, 1, e))
          return 0;
        s->volume = -6000;
        s->wanted = 1;
      }
      *pending = 0;
    }
  }
  return 1;
}
