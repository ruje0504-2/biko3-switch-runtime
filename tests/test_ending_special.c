#include "game/ending_special.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned calls, stop, draws, finds, hides, material_calls;
  BkDrawDispatch initial, special;
  float aim[3];
} Trace;
static int step(Trace *t, char e[256]) {
  if (++t->calls == t->stop) {
    snprintf(e, 256, "injected failure");
    return 0;
  }
  return 1;
}
static int draw(void *c, const BkDrawDispatch *d, char e[256]) {
  Trace *t = c;
  if (t->draws++ == 0)
    t->initial = *d;
  else
    t->special = *d;
  return step(t, e);
}
static int read_camera(void *c, float p[3], float f[3], float u[3],
                       char e[256]) {
  const float values[9] = {10, 20, 30, 0, 0, 1, 0, 1, 0};
  memcpy(p, values, 12);
  memcpy(f, values + 3, 12);
  memcpy(u, values + 6, 12);
  return step(c, e);
}
static int position(void *c, const float p[3], char e[256]) {
  (void)p;
  return step(c, e);
}
static int orientation(void *c, const float f[3], const float u[3],
                       char e[256]) {
  (void)f;
  (void)u;
  return step(c, e);
}
static int aim(void *c, const float p[3], char e[256]) {
  memcpy(((Trace *)c)->aim, p, 12);
  return step(c, e);
}
static int publish(void *c, char e[256]) { return step(c, e); }
static int render(void *c, BkEndingSpecialRenderEvent kind, unsigned arg,
                  char e[256]) {
  assert(kind <= BK_ENDING_SPECIAL_BEGIN);
  assert(arg == (kind == BK_ENDING_SPECIAL_CLEAR ? 2 : arg));
  return step(c, e);
}
static int find(void *c, uint32_t root, const char *name, uint32_t *node,
                char e[256]) {
  assert(root == 7 && name);
  Trace *t = c;
  t->finds++;
  *node = 99; /* duplicate aliases intentionally share the same real token */
  return step(c, e);
}
static int hide(void *c, uint32_t node, uint32_t hidden, char e[256]) {
  assert(node == 99 && (hidden == 0 || hidden == 1 || hidden == 255));
  ((Trace *)c)->hides++;
  return step(c, e);
}
static int material(void *c, const char *name, int mode, float alpha,
                    char e[256]) {
  assert(!strcmp(name, "Om_syokusyu_maki") && (mode == 0 || mode == 1) &&
         alpha == 1);
  ((Trace *)c)->material_calls++;
  return step(c, e);
}
int main(void) {
  char e[256];
  float cameras[BK_ENDING_SPECIAL_CAMERAS][4],
      before[sizeof(cameras) / sizeof(float)];
  for (unsigned g = 0; g < 5; g++) {
    assert(bk_ending_special_cameras(cameras, g));
    for (unsigned i = 0; i < BK_ENDING_SPECIAL_CAMERAS; i++)
      for (unsigned j = 0; j < 4; j++)
        assert(isfinite(cameras[i][j]));
  }
  memcpy(before, cameras, sizeof(cameras));
  assert(!bk_ending_special_cameras(cameras, 5));
  assert(!memcmp(before, cameras, sizeof(cameras)));
  assert(!bk_ending_special_cameras(NULL, 0));
  assert(!bk_ending_special_hidden_name(5, 0, 0));
  assert(!bk_ending_special_hidden_name(0, 10, 0));
  assert(!bk_ending_special_hidden_name(0, 0, 3));
  assert(!strcmp(bk_ending_special_hidden_name(0, 0, 0), ""));
  BkEndingFrameState frame = {.group = 4, .phase = 6};
  int32_t variant = 1, index = 47, mode = 1;
  uint8_t camera_variant = 2, restore = 255;
  uint32_t primary = 7, auxiliary = 8;
  float target[3] = {100, 200, 300};
  BkEndingSpecialBindings b = {&frame,   &variant,  &camera_variant, &restore,
                               &index,   &mode,     cameras,         target,
                               &primary, &auxiliary};
  Trace t = {0};
  BkEndingSpecialOps o = {&t,          draw, read_camera, position,
                          orientation, aim,  publish,     render,
                          find,        hide, material};
  BkDrawDispatch d = {.mode = 1, .shadow_mode = 2, .event_stage = 17};
  for (unsigned i = 0; i < 52; i++)
    d.objects[i] = i + 1;
  BkDrawDispatch original = d;
  assert(bk_ending_special_draw(&b, &d, &o, e));
  assert(!memcmp(&t.initial, &original, sizeof(d)));
  assert(d.mode == 2 && d.shadow_mode == 2 && d.event_stage == 17);
  assert(d.objects[20] == primary && !d.objects[21]);
  for (unsigned i = 0; i < 20; i++)
    assert(d.objects[i] == original.objects[i]);
  for (unsigned i = 22; i < 52; i++)
    assert(!d.objects[i]);
  assert(t.draws == 2 && t.finds == 5 && t.hides == 10 &&
         t.material_calls == 2);
  unsigned calls = t.calls;
  for (unsigned stop = 1; stop <= calls; stop++) {
    t = (Trace){.stop = stop};
    d = original;
    assert(!bk_ending_special_draw(&b, &d, &o, e));
    assert(t.calls == stop && !strcmp(e, "injected failure"));
  }
  t = (Trace){0};
  d = original;
  index = 108;
  assert(!bk_ending_special_draw(&b, &d, &o, e));
  assert(t.draws == 1 && !memcmp(&d, &original, sizeof(d)));
  index = 47;
  o.material = NULL;
  t = (Trace){0};
  assert(!bk_ending_special_draw(&b, &d, &o, e));
  assert(strstr(e, "missing material service") && t.draws == 1);
  assert(!bk_ending_special_draw(NULL, &d, &o, e));
  printf("PASS ending special ordered failure prefixes=%u, invalid input, "
         "descriptor retention\n",
         calls);
  return 0;
}
