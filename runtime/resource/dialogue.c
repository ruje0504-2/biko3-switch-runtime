#include "resource/dialogue.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
static int fail(char *error) {
  snprintf(error, 256, "dialogue: invalid/truncated/oversized input");
  return 0;
}
static int match(const uint8_t *raw, size_t size, uint32_t pos,
                 const char *tag) {
  size_t n = strlen(tag);
  return pos <= size && n <= size - pos && !memcmp(raw + pos, tag, n);
}
static int copy_name(char out[256], const uint8_t *raw, size_t size,
                     uint32_t *pos, unsigned n, const char *suffix) {
  if (*pos > size || n > size - *pos)
    return 0;
  memset(out, 0, 256);
  unsigned used = 0;
  while (used < n && raw[*pos + used]) {
    out[used] = (char)raw[*pos + used];
    ++used;
  }
  memcpy(out + used, suffix, strlen(suffix) + 1);
  *pos += n;
  return 1;
}
static int32_t number(const uint8_t *raw, unsigned n) {
  unsigned i = 0;
  while (i < n && (raw[i] == ' ' || (raw[i] >= 9 && raw[i] <= 13)))
    ++i;
  int sign = 1;
  if (i < n && (raw[i] == '-' || raw[i] == '+'))
    sign = raw[i++] == '-' ? -1 : 1;
  int32_t value = 0;
  for (; i < n && raw[i] >= '0' && raw[i] <= '9'; ++i)
    value = value * 10 + raw[i] - '0';
  return sign * value;
}
static int picture(BkDialogue *s, const uint8_t *raw, size_t size) {
  if (match(raw, size, s->cursor, "#SG")) {
    ++s->cursor;
    s->image_kind = 1;
    if (!copy_name(s->image, raw, size, &s->cursor, 7, ".bmp"))
      return 0;
  }
  if (match(raw, size, s->cursor, "#g")) {
    ++s->cursor;
    s->image_kind = 2;
    if (!copy_name(s->image, raw, size, &s->cursor, 6, ".bmp"))
      return 0;
  }
  return 1;
}
int bk_dialogue_open(BkDialogue *state, const void *data, size_t size,
                     const char *filename, char error[256]) {
  if (!state || !data || !size || size > UINT32_MAX || !filename)
    return fail(error);
  size_t n = 0;
  while (n < 256 && filename[n])
    ++n;
  if (n == 256)
    return fail(error);
  const uint8_t *raw = data;
  BkDialogue s = *state;
  memset(s.filename, 0, sizeof(s.filename));
  memcpy(s.filename, filename, n);
  s.cursor = 0;
  memset(s.text.bytes, 0, sizeof(s.text.bytes));
  s.text.length = 0;
  memset(s.image, 0, sizeof(s.image));
  s.code_c = 0;
  s.code_f = s.code_m = -1;
  s.code_e = 9;
  char key[16];
  snprintf(key, sizeof(key), "#%05" PRId32, s.first_label);
  /* Original compares only six bytes even for a wider formatted label. */
  key[6] = 0;
  for (;;) {
    if (!picture(&s, raw, size))
      return fail(error);
    if (match(raw, size, s.cursor, "#bg_off")) {
      memset(s.music, 0, sizeof(s.music));
      s.music_pending = 1;
      s.cursor += 7;
    }
    if (match(raw, size, s.cursor, "#bg")) {
      ++s.cursor;
      s.music_pending = 1;
      if (!copy_name(s.music, raw, size, &s.cursor, 5, ".wav"))
        return fail(error);
    }
    if (match(raw, size, s.cursor, key))
      break;
    if (match(raw, size, s.cursor, "#end")) {
      s.cursor = 0;
      break;
    }
    if (s.cursor >= size)
      return fail(error);
    ++s.cursor;
  }
  *state = s;
  return 1;
}
static int numeric_tag(const uint8_t *raw, size_t size, uint32_t p,
                       const char *prefix) {
  size_t n = strlen(prefix);
  return match(raw, size, p, prefix) && n < size - p && raw[p + n] >= '0' &&
         raw[p + n] <= '9';
}
int bk_dialogue_next(BkDialogue *state, const void *data, size_t size,
                     int *done, char error[256]) {
  if (!state || !done)
    return fail(error);
  BkDialogue s = *state;
  s.code_f = -1;
  if (s.current_label == s.last_label) {
    memset(s.text.bytes, 0, sizeof(s.text.bytes));
    s.text.length = 0;
    memset(s.previous_sound, 0, sizeof(s.previous_sound));
    s.code_c = s.code_m = 0;
    *state = s;
    *done = 1;
    return 1;
  }
  if (!data || !size || size > UINT32_MAX || s.cursor >= size)
    return fail(error);
  const uint8_t *raw = data;
  for (;;) {
    if (match(raw, size, s.cursor, "#bg_off")) {
      memset(s.music, 0, sizeof(s.music));
      s.music_pending = 1;
      /* Unlike open, next does not advance seven bytes here. */
    }
    if (numeric_tag(raw, size, s.cursor, "#bg")) {
      ++s.cursor;
      s.music_pending = 1;
      if (!copy_name(s.music, raw, size, &s.cursor, 5, ".wav"))
        return fail(error);
    }
    if (!picture(&s, raw, size))
      return fail(error);
    if (numeric_tag(raw, size, s.cursor, "#") ||
        match(raw, size, s.cursor, "#end"))
      break;
    if (s.cursor >= size)
      return fail(error);
    ++s.cursor;
  }
  /* Native tests '#' before '#end' in this second scan. */
  if (s.cursor >= size || raw[s.cursor++] != '#' || size - s.cursor < 5)
    return fail(error);
  s.current_label = number(raw + s.cursor, 5);
  s.cursor += 5;
  for (;;) {
    const char *tags[4] = {"#C", "#F", "#E", "#M"};
    int32_t *codes[4] = {&s.code_c, &s.code_f, &s.code_e, &s.code_m};
    for (unsigned i = 0; i < 4; ++i)
      if (match(raw, size, s.cursor, tags[i])) {
        s.cursor += 2;
        if (size - s.cursor < 2)
          return fail(error);
        *codes[i] = number(raw + s.cursor, 2);
        s.cursor += 2;
      }
    const char *sounds[2] = {"#P", "#se"};
    for (unsigned i = 0; i < 2; ++i)
      if (match(raw, size, s.cursor, sounds[i])) {
        ++s.cursor;
        if (!copy_name(s.sound, raw, size, &s.cursor, i ? 5 : 7, ".wav"))
          return fail(error);
        s.sound_pending = 1;
      }
    if (s.cursor >= size)
      return fail(error);
    if (raw[s.cursor] == '\n') {
      ++s.cursor;
      break;
    }
    if (match(raw, size, s.cursor, "#end")) {
      *state = s;
      *done = 1;
      return 1;
    }
    ++s.cursor;
  }
  uint32_t body = s.cursor;
  uint8_t closing = 0;
  s.text.carriage_returns = 0;
  for (size_t pos = body; pos < size; ++pos) {
    if (raw[pos] == '\r')
      ++s.text.carriage_returns;
    if (size - pos >= 2 && raw[pos] == 0x81) {
      if (!closing && (raw[pos + 1] == 0x75 || raw[pos + 1] == 0x69 ||
                       raw[pos + 1] == 0x67))
        closing = raw[pos + 1] + 1;
      if (closing && raw[pos + 1] == closing) {
        pos += 2;
        size_t span = pos - body;
        if (span > BK_MESSAGE_CAPACITY)
          return fail(error);
        memset(s.text.bytes, 0, sizeof(s.text.bytes));
        const uint8_t *nul = memchr(raw + body, 0, span);
        s.text.length = nul ? (size_t)(nul - raw - body) : span;
        memcpy(s.text.bytes, raw + body, s.text.length);
        *state = s;
        *done = 0;
        return 1;
      }
    }
    if (raw[pos] == '#') {
      size_t span = pos - body;
      if (span > BK_MESSAGE_CAPACITY)
        return fail(error);
      memset(s.text.bytes, 0, sizeof(s.text.bytes));
      const uint8_t *nul = memchr(raw + body, 0, span);
      s.text.length = nul ? (size_t)(nul - raw - body) : span;
      memcpy(s.text.bytes, raw + body, s.text.length);
      s.cursor = (uint32_t)pos;
      *state = s;
      *done = 0;
      return 1;
    }
    if (pos - body >= BK_MESSAGE_CAPACITY)
      return fail(error);
  }
  return fail(error);
}
void bk_dialogue_close(BkDialogue *s) {
  if (!s)
    return;
  s->cursor = 0;
  s->first_label = s->last_label = s->current_label = 0;
  memset(s->filename, 0, sizeof(s->filename));
  memset(s->previous_sound, 0, sizeof(s->previous_sound));
  memset(s->text.bytes, 0, sizeof(s->text.bytes));
  s->text.length = 0;
}
