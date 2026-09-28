#include "game/draw_dispatch.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  unsigned count, fail_at;
  char trace[8];
} Services;
static int step(Services *s, char kind) {
  s->trace[s->count++] = kind;
  return s->count != s->fail_at;
}
static int prepare(void *s) { return step(s, 'P'); }
static int scene(void *s, BkDrawDispatch *p) {
  p->objects[51] = 123;
  return step(s, 'S');
}
static int status(void *s, int32_t *result, uint32_t *flags) {
  *result = 0;
  *flags = 1;
  return step(s, 'Q');
}
static int duck(void *s, uint32_t enabled) {
  assert(enabled == 1);
  return step(s, 'D');
}
static int regular(void *s, const BkDrawDispatch *p) {
  (void)p;
  return step(s, 'R');
}
int main(void) {
  BkDrawDispatchInput in = {.flow = 16,
                            .event_state = 1,
                            .root_721b28 = 11,
                            .root_721b2c = 22,
                            .root_721b34 = 33};
  BkDrawDispatch p;
  assert(bk_draw_dispatch_select(&in, &p));
  assert(p.mode == 1 && p.event_stage == 1 && p.objects[0] == 33 &&
         p.objects[4] == 11 && p.objects[5] == 22);
  in.event_state = 7;
  assert(bk_draw_dispatch_select(&in, &p));
  assert(p.mode == 1 && p.event_stage == 0);
  for (unsigned i = 0; i < 52; ++i)
    assert(p.objects[i] == 0);
  in.flow = 72;
  in.group = 4;
  assert(bk_draw_dispatch_select(&in, &p) && p.mode == 10 &&
         p.objects[4] == 11);
  BkLightingPassInput lights = {.light_count = 1, .scene_root = 1234};
  lights.lights[0] = (BkPassLight){{.25f, .5f, 1}, 2, 1};
  BkPassLight saved_light = lights.lights[0];
  assert(bk_draw_dispatch_lighting(&p, &lights));
  assert(!memcmp(&saved_light, &lights.lights[0], sizeof(saved_light)) &&
         lights.light_count == 1 && lights.scene_root == 1234 &&
         lights.mode == 10 && lights.objects[4] == 11);
  BkDrawDispatch saved = p;
  in.flow = 56;
  assert(!bk_draw_dispatch_select(&in, &p) && !memcmp(&p, &saved, sizeof(p)));
  in.root_bef77c = 44;
  in.root_bef780 = 55;
  assert(bk_draw_dispatch_select(&in, &p) && p.objects[4] == 44 &&
         p.objects[0] == 55);
  in.flow = 256;
  saved = p;
  assert(!bk_draw_dispatch_select(&in, &p) && !memcmp(&p, &saved, sizeof(p)));
  in.flow = 16;
  in.event_state = 1;
  BkEventDrawState event = {1, 1};
  Services services = {0};
  BkDrawDispatchOps ops = {&services, prepare, scene, status, duck, regular};
  for (unsigned fail = 0; fail <= 4; ++fail) {
    services = (Services){.fail_at = fail};
    assert(bk_draw_dispatch_run(&in, &event, &ops) == (fail == 0));
    assert(services.count == (fail ? fail : 4));
    assert(!memcmp(services.trace, "PSQD", services.count));
  }
  services = (Services){0};
  ops.buffer_status = NULL;
  assert(!bk_draw_dispatch_run(&in, &event, &ops) && services.count == 0);
  ops.buffer_status = status;
  event.mode = 0;
  services = (Services){0};
  assert(bk_draw_dispatch_run(&in, &event, &ops));
  assert(services.count == 4 && !memcmp(services.trace, "PQSD", 4));
  event.mode = 3;
  services = (Services){0};
  assert(bk_draw_dispatch_run(&in, &event, &ops));
  assert(services.count == 2 && !memcmp(services.trace, "PR", 2));
  in.event_state = 7;
  services = (Services){0};
  assert(bk_draw_dispatch_run(&in, NULL, &ops));
  assert(services.count == 1 && services.trace[0] == 'R');
  puts("PASS main draw root selection, event delegation, gallery requirements "
       "and light snapshot retention");
}
