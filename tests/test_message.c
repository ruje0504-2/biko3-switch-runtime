#include "resource/dialogue.h"
#include "resource/message.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void dialogue(void) {
  char error[256];
  const char raw[] = "#SG01_00#bg003#00001 #C01#F02#E03#M04#PT01000\r\n"
                     "\x81\x75"
                     "hello\x81\x76\r\n"
                     "#bg_off\r\n#00002 #C02\r\nworld\r\n#end";
  BkDialogue s = {.first_label = 1, .last_label = 2, .current_label = -1};
  assert(bk_dialogue_open(&s, raw, sizeof(raw) - 1, "test.txt", error));
  assert(!strcmp(s.image, "SG01_00.bmp") && !strcmp(s.music, "bg003.wav"));
  int done = -1;
  assert(bk_dialogue_next(&s, raw, sizeof(raw) - 1, &done, error) && !done);
  assert(s.current_label == 1 && s.code_c == 1 && s.code_f == 2 &&
         s.code_e == 3 && s.code_m == 4 && s.text.length == 9 &&
         raw[s.cursor] == (char)0x81 && !strcmp(s.sound, "PT01000.wav"));
  s.sound_pending = 0;
  assert(bk_dialogue_next(&s, raw, sizeof(raw) - 1, &done, error) && !done);
  assert(s.current_label == 2 && s.code_c == 2 && s.code_f == -1 &&
         s.code_e == 3 && s.code_m == 4 && !s.sound_pending &&
         !strcmp(s.sound, "PT01000.wav") && !s.music[0] &&
         s.music_pending == 1);
  strcpy(s.previous_sound, "old.wav");
  assert(bk_dialogue_next(&s, NULL, 0, &done, error) && done);
  assert(!s.text.length && s.text.carriage_returns == 1 &&
         !s.previous_sound[0] && s.code_m == 0 && s.code_e == 3);
  /* Truncated and mutated sources cannot read past the owned byte span. */
  uint32_t rng = 0x517c8e;
  for (unsigned case_index = 0; case_index < 4096; ++case_index) {
    unsigned char bytes[sizeof(raw)];
    memcpy(bytes, raw, sizeof(bytes));
    size_t size = case_index < sizeof(bytes) ? case_index : sizeof(bytes) - 1;
    for (unsigned i = 0; i < case_index % 7; ++i) {
      rng = rng * 1664525u + 1013904223u;
      bytes[rng % sizeof(bytes)] ^= (uint8_t)(rng >> 24);
    }
    s.first_label = 1;
    s.last_label = 2;
    s.current_label = -1;
    BkDialogue before = s;
    if (!bk_dialogue_open(&s, bytes, size, "test.txt", error)) {
      assert(!memcmp(&s, &before, sizeof(s)));
      continue;
    }
    for (unsigned step = 0; step < 4; ++step) {
      before = s;
      done = 99;
      if (!bk_dialogue_next(&s, bytes, size, &done, error)) {
        assert(!memcmp(&s, &before, sizeof(s)) && done == 99);
        break;
      }
      if (done)
        break;
    }
  }
  unsigned char over[1100];
  memset(over, 'x', sizeof(over));
  memcpy(over, "#00001\n", 7);
  memcpy(over + sizeof(over) - 4, "#end", 4);
  s.first_label = 1;
  s.current_label = -1;
  assert(bk_dialogue_open(&s, over, sizeof(over), "x", error));
  BkDialogue before = s;
  done = 77;
  assert(!bk_dialogue_next(&s, over, sizeof(over), &done, error));
  assert(!memcmp(&s, &before, sizeof(s)) && done == 77);
  char name[257];
  memset(name, 'x', 256);
  name[256] = 0;
  assert(!bk_dialogue_open(&s, raw, sizeof(raw), name, error));
  assert(!memcmp(&s, &before, sizeof(s)));
}
int main(void) {
  dialogue();
  char error[256];
  BkMessage out = {.length = 99, .carriage_returns = 71}, before;
  memset(out.bytes, 0xa5, sizeof(out.bytes));
  const char raw[] = "#00001\r\nab\0c\r\n\r\n#end";
  assert(bk_message_lookup(raw, sizeof(raw) - 1, 1, &out, error));
  assert(out.length == 2 && out.carriage_returns == 2 &&
         !memcmp(out.bytes, "ab", 2));
  for (size_t i = 2; i < sizeof(out.bytes); ++i)
    assert(!out.bytes[i]);
  assert(bk_message_lookup(raw, sizeof(raw) - 1, 2, &out, error));
  assert(out.length == 0 && out.carriage_returns == 2);
  const char header[] = "#00001#end";
  assert(bk_message_lookup(header, sizeof(header) - 1, 1, &out, error));
  assert(out.length == 0 && out.carriage_returns == 2);
  uint8_t full[7 + BK_MESSAGE_CAPACITY + 4];
  memcpy(full, "#00000\n", 7);
  memset(full + 7, 'X', BK_MESSAGE_CAPACITY);
  memcpy(full + 7 + BK_MESSAGE_CAPACITY, "#end", 4);
  assert(bk_message_lookup(full, sizeof(full), 0, &out, error));
  assert(out.length == BK_MESSAGE_CAPACITY && out.carriage_returns == 0);
  before = out;
  full[7 + BK_MESSAGE_CAPACITY] = 'X';
  assert(!bk_message_lookup(full, sizeof(full), 0, &out, error));
  assert(!memcmp(&out, &before, sizeof(out)));
  const char quote[] = "#00000\n\x81\x75"
                       "a\x81\x69"
                       "b\x81\x6a"
                       "c\x81\x76"
                       "rest";
  assert(bk_message_lookup(quote, sizeof(quote) - 1, 0, &out, error));
  assert(out.length == 11 && out.bytes[10] == 0x76);
  /* Every truncated prefix must either terminate safely or reject atomically.
   */
  for (size_t n = 0; n < sizeof(quote); ++n) {
    before = out;
    if (!bk_message_lookup(quote, n, 0, &out, error))
      assert(!memcmp(&out, &before, sizeof(out)));
  }
  const char *bad[] = {"#END", "#00000", "#00000\nhello", "#00000\n\x81"};
  for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
    before = out;
    assert(!bk_message_lookup(bad[i], strlen(bad[i]), 0, &out, error));
    assert(!memcmp(&out, &before, sizeof(out)));
  }
  before = out;
  assert(!bk_message_lookup(NULL, 4, 0, &out, error));
  assert(!memcmp(&out, &before, sizeof(out)));
  assert(!bk_message_lookup(raw, sizeof(raw), 0, NULL, error));
  puts("PASS bounded message lookup, NUL padding, retained CR count, quotes, "
       "full capacity and atomic malformed rejection");
}
