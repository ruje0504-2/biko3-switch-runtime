#include "game/draw_dispatch.h"
#include <string.h>
int bk_draw_dispatch_select(const BkDrawDispatchInput *in,
                            BkDrawDispatch *out) {
  if (!in || !out || in->flow > 255)
    return 0;
  BkDrawDispatch p = {0};
  switch (in->flow) {
  case 8:
    p.mode = 1;
    p.objects[4] = in->root_733e28;
    break;
  case 56:
    if (!in->root_bef77c || !in->root_bef780)
      return 0;
    p.mode = 1;
    p.objects[4] = in->root_bef77c;
    p.objects[0] = in->root_bef780;
    break;
  case 72:
    p.mode = in->group == 4 ? 10 : 1;
    p.objects[4] = in->root_721b28;
    break;
  case 16:
    p.mode = 1;
    if (in->event_state != 7) {
      p.objects[4] = in->root_721b28;
      if (in->event_state == 1 || in->event_state == 8)
        p.objects[5] = in->root_721b2c;
      p.objects[0] = in->root_721b34;
      p.event_stage = 1;
    }
    break;
  default:
    break;
  }
  *out = p;
  return 1;
}
int bk_draw_dispatch_lighting(const BkDrawDispatch *p,
                              BkLightingPassInput *out) {
  if (!p || !out)
    return 0;
  memcpy(out->objects, p->objects, sizeof(out->objects));
  out->mode = p->mode;
  out->shadow_mode = p->shadow_mode;
  return 1;
}
static int buffer_playing(const BkEventDrawState *event,
                          const BkDrawDispatchOps *ops, uint32_t *active) {
  *active = 0;
  if (!event->buffer_present)
    return 1;
  int32_t result;
  uint32_t flags;
  if (!ops->buffer_status(ops->context, &result, &flags))
    return 0;
  *active = result == 0 && (flags & 1);
  return 1;
}
int bk_draw_dispatch_run(const BkDrawDispatchInput *in,
                         const BkEventDrawState *event,
                         const BkDrawDispatchOps *ops) {
  BkDrawDispatch p;
  if (!ops || !bk_draw_dispatch_select(in, &p))
    return 0;
  if (!p.event_stage)
    return ops->regular && ops->regular(ops->context, &p);
  if (!event || !ops->prepare_event)
    return 0;
  int media = event->mode >= 0 && event->mode <= 2;
  int scene = event->mode == 0 || event->mode == 1;
  if ((media &&
       (!ops->duck_audio || (event->buffer_present && !ops->buffer_status))) ||
      (scene && !ops->event_scene) || (event->mode != 1 && !ops->regular))
    return 0;
  if (!ops->prepare_event(ops->context))
    return 0;
  uint32_t active = 0;
  if (event->mode == 1 && !ops->event_scene(ops->context, &p))
    return 0;
  if (media) {
    if (!buffer_playing(event, ops, &active))
      return 0;
    if (event->mode == 0 && active && !ops->event_scene(ops->context, &p))
      return 0;
    if (!ops->duck_audio(ops->context, active))
      return 0;
    if (event->mode == 1 || (event->mode == 0 && active))
      return 1;
  }
  return ops->regular(ops->context, &p);
}
