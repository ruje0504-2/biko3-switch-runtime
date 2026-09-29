#include "game/ending_sound.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int bk_ending_sound_absent_speech(const char *name) {
  static const char *const absent[] = {
#include "game/ending_absent_speech.inc"
  };
  if (name)
    for (unsigned i = 0; i < sizeof(absent) / sizeof(*absent); i++)
      if (!strcmp(name, absent[i]))
        return 1;
  return 0;
}
int bk_ending_sound_loop_name(unsigned group, int32_t phase, float progress,
                               char name[32], char e[256]) {
  int code = progress >= .19f && progress < .4f ? 1
             : progress >= .39f && progress < .6f ? 2
             : progress >= .59f ? 3 : -1;
  if (!name || group >= 5 || phase != 1 || !isfinite(progress) || code < 0) {
    if (e)
      snprintf(e, 256, "ending sound: undefined loop voice selection");
    return 0;
  }
  snprintf(name, 32, "PH%u01%02d.wav", group + 1, code);
  return 1;
}
int bk_ending_sound_action_name(unsigned group, int32_t target, float progress,
                                 char name[32], char e[256]) {
  int code;
  switch (target) {
  case 1: code = 1; break;
  case 11: code = 12; break;
  case 12: code = 20; break;
  case 9: code = 28; break;
  case 10: code = 36; break;
  case 26:
  case 27: code = 44; break;
  case 5: code = 52; break;
  default: code = -1; break;
  }
  if (!name || group >= 5 || code < 0 || !isfinite(progress)) {
    if (e)
      snprintf(e, 256, "ending sound: undefined action voice selection");
    return 0;
  }
  if (progress >= .2f)
    code += progress >= .19f && progress < .4f ? 1 : 2;
  snprintf(name, 32, "PH%u02%02d.wav", group + 1, code);
  return 1;
}
int bk_ending_sound_tertiary_voice(unsigned group, int32_t cue, int32_t select,
                                   char name[32], char e[256]) {
  if (!name || group >= 5) {
    if (e) snprintf(e, 256, "ending sound: invalid third-ending voice group/output");
    return 0;
  }
  snprintf(name, 32, "PH%u%02d%02d.wav", group + 1, select ? 31 : 32, cue);
  return 1;
}
int bk_ending_sound_normal_voice(unsigned group, int32_t target, int32_t mode,
                                 char name[32], char e[256]) {
  int code;
  switch (target) {
  case 1:
    code = 4;
    break;
  case 11:
    code = 15;
    break;
  case 12:
    code = 23;
    break;
  case 9:
    code = 31;
    break;
  case 10:
    code = 39;
    break;
  case 26:
  case 27:
    code = 47;
    break;
  case 5:
    code = 55;
    break;
  default:
    code = -1;
    break;
  }
  if (!name || group >= 5 || code < 0 ||
      (target == 1 && mode != 0 && mode != 1)) {
    if (e)
      snprintf(e, 256, "ending sound: undefined normal voice selection");
    return 0;
  }
  code += mode == 0 ? 0 : mode == 1 ? 1 : 3;
  snprintf(name, 32, "PH%u02%02d.wav", group + 1, code);
  return 1;
}
int bk_ending_sound_contact_voice(unsigned group, int32_t kind, int32_t index,
                                  int32_t alternate, char name[32],
                                  char e[256]) {
  static const int normal[] = {2, 4, 6};
  static const int contact[] = {6,  -1, 17, -1, 25, 27, 33,
                                35, 41, 43, 49, -1, 57, 59};
  int64_t offset = (int64_t)index + (kind != 0 && alternate != 0);
  if (!name || group >= 5 || offset < 0 || offset >= (kind ? 14 : 3)) {
    if (e)
      snprintf(e, 256, "ending sound: invalid contact voice selection");
    return 0;
  }
  snprintf(name, 32, "PH%u%02d%02d.wav", group + 1, kind ? 2 : 0,
           kind ? contact[offset] : normal[offset]);
  return 1;
}
const char *bk_ending_sound_music(unsigned group, unsigned variant) {
  static const char *const names[5][2] = {{"bg005.wav", "bg006.wav"},
                                          {"bg023.wav", "bg024.wav"},
                                          {"bg030.wav", "bg031.wav"},
                                          {"bg017.wav", "bg018.wav"},
                                          {"bg011.wav", "bg012.wav"}};
  return group < 5 && variant < 2 ? names[group][variant] : NULL;
}
const char *bk_ending_sound_effect(unsigned index) {
  static const char *const names[BK_ENDING_EFFECTS] = {
      "se200.wav",    "se201.wav",    "se202.wav",    "se203.wav",
      "se204.wav",    "se205.wav",    "se206.wav",    "se207.wav",
      "se208.wav",    "se209.wav",    "se210.wav",    "se211.wav",
      "se212.wav",    "se213_00.wav", "se214_00.wav", "se214_01.wav",
      "se215_00.wav", "se215_01.wav", "se216_00.wav", "se216_01.wav",
      "se216_02.wav", "se217_00.wav", "se217_01.wav", "se217_02.wav",
      "se218_00.wav", "se218_01.wav", "se218_02.wav", "se218_03.wav",
      "se218_04.wav", "se219.wav",    "se220.wav",    "se221.wav",
      "se222_00.wav", "se222_01.wav", "se223_00.wav", "se223_01.wav",
      "se224_00.wav", "se224_01.wav", "se225.wav",    "se226.wav",
      "se301.wav"};
  return index < BK_ENDING_EFFECTS ? names[index] : NULL;
}
int bk_ending_sound_duck(int32_t *transition, const BkEndingDuckInput *in,
                         const BkEndingDuckOps *ops, int32_t *result,
                         char e[256]) {
  if (!transition || !in || !ops || !result || !ops->status || !ops->volume ||
      !ops->gain || !isfinite(in->seconds) || in->seconds < 0 ||
      (double)in->seconds * 2000 > INT32_MAX || in->master < -10000 ||
      in->master > 0) {
    snprintf(e, 256, "ending sound: invalid duck input/services");
    return 0;
  }
  int playing = 0;
  if (in->present[0] && !ops->status(ops->context, 0, &playing, e))
    return 0;
  if (!playing) {
    *result = 1;
    return 1;
  }
  int32_t volume;
  if (!ops->volume(ops->context, &volume, e))
    return 0;
  playing = 0;
  if (in->present[1] && !ops->status(ops->context, 1, &playing, e))
    return 0;
  if ((!playing && volume >= in->master) ||
      (playing && volume <= in->master - 2000)) {
    *result = 1;
    return 1;
  }
  if (!*transition)
    *transition = 1;
  if (!ops->volume(ops->context, &volume, e))
    return 0;
  /* x87 multiplication is not rounded to float before truncation. Wrap the
   * original32-bit add/sub without C signed overflow or narrowing overflow. */
  uint32_t step = (uint32_t)((double)in->seconds * 2000);
  uint32_t bits =
      in->direction == 1 ? (uint32_t)volume - step : (uint32_t)volume + step;
  volume = bits <= INT32_MAX ? (int32_t)bits
                             : (int32_t)((int64_t)bits - INT64_C(4294967296));
  int32_t complete = 0;
  if (volume >= in->master) {
    volume = in->master;
    complete = 1;
  } else if (volume <= in->master - 2000) {
    volume = in->master - 2000;
    complete = 1;
  }
  if (!ops->gain(ops->context, volume, e))
    return 0;
  if (complete)
    *transition = 0;
  *result = complete;
  return 1;
}
