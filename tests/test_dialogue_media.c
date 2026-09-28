#include "scene/dialogue_media.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned calls, fail_at;
  BkDialogueMediaKind last;
} Service;
static int command(void *ctx, const BkDialogueMediaCommand *c, char e[256]) {
  Service *s = ctx;
  s->last = c->kind;
  if (++s->calls == s->fail_at) {
    snprintf(e, 256, "injected media failure");
    return 0;
  }
  return 1;
}
int main(void) {
  Service service = {0};
  BkDialogueMediaOps ops = {&service, command};
  char error[256], name[256] = "bg001.wav";
  for (unsigned failure = 1; failure <= 3; ++failure) {
    uint8_t pending = 1;
    service = (Service){.fail_at = failure};
    assert(!bk_dialogue_speech_step(&pending, "se001.wav", -100, &ops, error));
    assert(pending == 1 && service.calls == failure &&
           strstr(error, "injected"));
    BkDialogueMusic music = {-5999, 0};
    service = (Service){.fail_at = failure};
    assert(
        !bk_dialogue_music_step(&music, 1, &pending, name, 0, 0, &ops, error));
    assert(pending == 1 && music.volume == -6000 && music.wanted == 0 &&
           service.calls == failure);
  }
  service = (Service){0};
  uint8_t pending = 1;
  BkDialogueMusic music = {-6000, 1};
  assert(bk_dialogue_music_step(&music, 1, &pending, name, 0, 0, &ops, error));
  assert(music.volume == -5999 && music.wanted == 0 && pending == 1 &&
         service.calls == 1);
  assert(bk_dialogue_music_step(&music, 1, &pending, name, 0, 0, &ops, error));
  assert(music.volume == -6000 && music.wanted == 1 && pending == 0 &&
         service.calls == 4);
  BkDialogueMusic saved = music;
  unsigned calls = service.calls;
  for (unsigned i = 0; i < 4; ++i) {
    float dt[] = {NAN, -1, FLT_MAX, 2684342.f};
    assert(!bk_dialogue_music_step(&music, 1, &pending, name, dt[i], 0, &ops,
                                   error));
    assert(!memcmp(&music, &saved, sizeof(music)) && service.calls == calls);
  }
  memset(name, 'x', sizeof(name));
  assert(!bk_dialogue_speech_step(&pending, name, 0, &ops, error));
  assert(!bk_dialogue_music_open(&music, &pending, name, &ops, error));
  assert(!memcmp(&music, &saved, sizeof(music)) && service.calls == calls);
  assert(bk_dialogue_music_open(&music, &pending, "", &ops, error));
  assert(music.volume == -6000 && music.wanted == 1 && pending == 0 &&
         service.calls == calls);
  ops.command = NULL;
  assert(!bk_dialogue_music_open(&music, &pending, "", &ops, error));
  puts("PASS dialogue media: failure-prefix retention, pending ack, old-state "
       "fade, bounded inputs");
}
