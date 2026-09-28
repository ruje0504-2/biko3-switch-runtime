#include "scene/dialogue_backdrop.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned calls;
  int fail;
} Service;
static int replace(void *p, const char *name, char e[256]) {
  Service *s = p;
  assert(!strcmp(name, "SG10000.bmp"));
  s->calls++;
  if (s->fail)
    snprintf(e, 256, "injected image failure");
  return !s->fail;
}
int main(void) {
  BkDialogueBackdrop s = {{.5f, 2, 3}, 3, 0, 1, 255};
  BkFadeSprite c = {0, 2, 0};
  uint8_t phase = 3, wanted = 0;
  int32_t expr = -1;
  Service service = {0, 1};
  BkDialogueBackdropOps ops = {&service, replace};
  BkDialogueBackdropBindings b = {&phase, &expr, &c, &wanted, "SG10000.bmp"};
  BkDialogueBackdropFrame frame = {.3f, .4f, 7};
  char e[256];
  BkDialogueBackdrop old = s;
  BkFadeSprite old_c = c;
  assert(!bk_dialogue_backdrop_step(&s, &b, &ops, .1f, &frame, e));
  assert(!memcmp(&s, &old, sizeof(s)) && !memcmp(&c, &old_c, sizeof(c)) &&
         phase == 3 && expr == -1 && service.calls == 1 &&
         strstr(e, "injected"));
  service.fail = 0;
  assert(bk_dialogue_backdrop_step(&s, &b, &ops, 0, &frame, e));
  assert(phase == 4 && s.image_kind == 2 && s.image_wanted == 255 &&
         frame.replaced == 1 && s.image.stage == 0 && s.image.alpha == 0);
  assert(bk_dialogue_backdrop_step(&s, &b, &ops, .25f, &frame, e));
  assert(s.image_wanted == 1 && s.image.stage == 1 && s.image.alpha == .25f &&
         !frame.replaced && service.calls == 2);
  old = s;
  old_c = c;
  BkDialogueBackdropFrame saved = frame;
  assert(!bk_dialogue_backdrop_step(&s, &b, &ops, NAN, &frame, e));
  assert(!memcmp(&s, &old, sizeof(s)) && !memcmp(&c, &old_c, sizeof(c)) &&
         !memcmp(&frame, &saved, sizeof(frame)));
  char unterminated[256];
  memset(unterminated, 'x', sizeof(unterminated));
  b.image = unterminated;
  assert(!bk_dialogue_backdrop_step(&s, &b, &ops, .1f, &frame, e));
  assert(!memcmp(&s, &old, sizeof(s)) &&
         !memcmp(&frame, &saved, sizeof(frame)));
  b.image = "SG10000.bmp";
  ops.replace = NULL;
  assert(!bk_dialogue_backdrop_step(&s, &b, &ops, .1f, &frame, e) &&
         service.calls == 2);
  float extent[2] = {17, 19};
  assert(!bk_dialogue_backdrop_extent(extent, 0, 0) && extent[0] == 17 &&
         extent[1] == 19);
  assert(!bk_dialogue_backdrop_extent(extent, 640, 2) && extent[0] == 17 &&
         extent[1] == 19);
  assert(bk_dialogue_backdrop_extent(extent, 640, 1) && extent[0] == 640 &&
         extent[1] == 480);
  puts("PASS dialogue backdrop: replacement failure, retained request, "
       "invalid-input atomicity");
}
