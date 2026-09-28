#include "scene/title_menu.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  float point[2];
  unsigned calls, fail_at;
} Services;
static int event(Services *s) { return ++s->calls != s->fail_at; }
static int sound(void *p, unsigned slot, char e[256]) {
  (void)slot;
  (void)e;
  return event(p);
}
static int gain(void *p, int32_t volume, char e[256]) {
  (void)volume;
  (void)e;
  return event(p);
}
static int warp(void *p, float x, float y, char e[256]) {
  (void)e;
  Services *s = p;
  s->point[0] = x;
  s->point[1] = y;
  return event(s);
}
static int position(void *p, float out[2], char e[256]) {
  (void)e;
  memcpy(out, ((Services *)p)->point, 2 * sizeof(float));
  return event(p);
}
static int motion(void *p, float out[2], char e[256]) {
  (void)e;
  out[0] = out[1] = 0;
  return event(p);
}
static int release(void *p, uint8_t flow, char e[256]) {
  (void)e;
  assert(flow == 1);
  return event(p);
}
int main(void) {
  char e[256];
  Services svc = {0};
  BkTitleMenuOps ops = {&svc, sound, gain, warp, position, motion, release};
  BkTitleMenuState s = {.row = 4};
  assert(bk_title_menu_initialize(&s, 1001, 0, -900, &ops, e));
  assert(s.row == 4 && s.loaded_mask == 8191 && svc.point[0] == 320);
  BkTitleSprite old = s.sprites[3];
  assert(bk_title_menu_initialize(&s, 640, 1, -600, &ops, e));
  assert(!memcmp(&old, &s.sprites[3], sizeof(old)));
  assert(!bk_title_menu_image(3, 1) &&
         !strcmp(bk_title_menu_image(0, 1), "te_10.bmp"));
  assert(bk_title_menu_initialize(&s, 640, 0, -900, &ops, e));
  BkCommonHudState c = {.curtain = {0, 2, 0}};
  BkMenuCursor cursor = {0};
  assert(bk_menu_cursor_initialize(&cursor, 640, 480));
  BkFlowTransition flow = {1, 0, 0, 0};
  uint8_t latch = 0;
  BkTitleMenuBindings b = {&c, &flow, &cursor, &latch};
  BkTitleMenuInput in = {.seconds = .25f, .scale = .5f, .music_master = -900};
  BkTitleMenuFrame f;
  /* Hovering switches which instance advances; unselected counterpart freezes.
   */
  svc.point[0] = s.sprites[1].rect[0];
  svc.point[1] = s.sprites[1].rect[1];
  old = s.sprites[1];
  assert(bk_title_menu_step(&s, &b, &in, &ops, &f, e));
  assert(s.sprites[1].zoom.fade.alpha == old.zoom.fade.alpha &&
         s.sprites[2].zoom.fade.alpha == 0);
  /* Pointer is sampled before warp; this draw still uses the old point. */
  in.buttons = BK_PAUSE_DOWN;
  float x = svc.point[0], y = svc.point[1];
  assert(bk_title_menu_step(&s, &b, &in, &ops, &f, e));
  assert(cursor.sprite.rect[0] == x && cursor.sprite.rect[1] == y &&
         svc.point[1] != y);
  /* Real services are mandatory, and an error stops at the exact prefix. */
  BkTitleMenuState start = s;
  for (unsigned failure = 1; failure <= 4; ++failure) {
    s = start;
    c = (BkCommonHudState){.curtain = {1, 2, 3}, .blocked = 1, .action = 9};
    flow.current = 1;
    svc = (Services){.point = {-100, -100}, .fail_at = failure};
    in.buttons = 0;
    assert(!bk_title_menu_step(&s, &b, &in, &ops, &f, e));
    assert(svc.calls == failure && flow.current == 1 &&
           s.loaded_mask == start.loaded_mask);
  }
  ops.release = NULL;
  svc.calls = 0;
  assert(!bk_title_menu_step(&s, &b, &in, &ops, &f, e) && !svc.calls);
  BkZoomSprite z;
  bk_zoom_sprite_initialize(&z);
  assert(bk_zoom_sprite_advance(&z, 0));
  BkZoomSprite copy = z;
  assert(!bk_zoom_sprite_advance(&z, NAN) && !memcmp(&z, &copy, sizeof(z)));
  float rect[4] = {1, 2, INFINITY, 40}, out[4] = {4, 5, 6, 7}, keep[4];
  memcpy(keep, out, sizeof(keep));
  assert(!bk_zoom_sprite_rect(&z, rect, out) &&
         !memcmp(out, keep, sizeof(out)));
  puts("title menu failure/retention/pointer/geometry tests passed");
  return 0;
}
