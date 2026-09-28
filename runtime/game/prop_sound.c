#include "game/prop_sound.h"
#include <math.h>
#include <string.h>
static int car(int32_t kind) { return kind == 0 || (kind >= 13 && kind <= 17); }
static void load(BkPropSoundCommand *out, const char *file, int32_t volume,
                 int loop, int play) {
  out->file = file;
  out->initial_volume = volume;
  out->release = 1;
  out->loop = loop;
  out->play = play;
}
int bk_prop_sound_initial(int32_t kind, int32_t volume,
                          BkPropSoundCommand *out) {
  if (!out || volume < -10000 || volume > 0)
    return 0;
  BkPropSoundCommand c = {.release = 1};
  if (car(kind))
    load(&c, "se127.wav", -6000, 1, 0);
  else
    switch (kind) {
    case 1:
      load(&c, "se131.wav", -6000, 1, 0);
      break;
    case 3:
      load(&c, "se126.wav", -6000, 0, 0);
      break;
    case 4:
      load(&c, "se125.wav", -6000, 0, 0);
      break;
    case 5:
      load(&c, "se123.wav", volume, 0, 0);
      break;
    case 8:
      load(&c, "se145.wav", -6000, 0, 0);
      break;
    case 9:
    case 12:
      load(&c, "se124.wav", -6000, 0, 0);
      break;
    default:
      break;
    }
  *out = c;
  return 1;
}
static int train_source(float out[3], const BkPropState *p,
                        const float *listener) {
  double angle = (double)p->path.yaw * 0.01745329238474369f;
  double sine = sin(angle), cosine = cos(angle);
  float x[3], z[3], distance[3];
  const double offset[] = {400., 0., -400.};
  for (unsigned i = 0; i < 3; ++i) {
    x[i] = (float)(sine * offset[i] + p->path.position[0]);
    z[i] = (float)(cosine * offset[i] + p->path.position[2]);
    double dx = (double)x[i] - listener[0], dz = (double)z[i] - listener[2];
    distance[i] = (float)sqrt(dx * dx + dz * dz);
    if (!isfinite(distance[i]))
      return 0;
  }
  unsigned nearest = 0;
  for (unsigned i = 1; i < 3; ++i)
    if (distance[i] < distance[nearest])
      nearest = i;
  out[0] = x[nearest];
  out[1] = p->path.position[1];
  out[2] = z[nearest];
  return 1;
}
int bk_prop_sound_step(BkPropSoundState *state, const BkPropState *prop,
                       const BkPropSoundInput *in, BkPropSoundCommand *out) {
  if (!state || !prop || !in || !out || in->effect_volume < -10000 ||
      in->effect_volume > 0 || !isfinite(in->listener_yaw) ||
      !isfinite(prop->path.yaw))
    return 0;
  for (unsigned i = 0; i < 3; ++i)
    if (!isfinite(in->listener[i]) || !isfinite(prop->path.position[i]))
      return 0;
  BkPropSoundState next = *state;
  BkPropSoundCommand c = {0};
  memcpy(c.source, prop->path.position, sizeof(c.source));
  int kind = prop->kind;
  if (car(kind)) {
    switch (next.stage) {
    case 0:
      if (prop->action == prop->actions[0]) {
        load(&c, "se150.wav", -6000, 0, 1);
        next.stage = 1;
      }
      break;
    case 1:
      if (prop->action == prop->actions[3]) {
        load(&c, "se149.wav", -6000, 0, 1);
        next.stage = 2;
      }
      break;
    case 2:
      if (!in->voice_present || !in->voice_playing)
        next.stage = 3;
      if (prop->action == prop->actions[0]) {
        load(&c, "se150.wav", -6000, 0, 1);
        next.stage = 1;
      }
      break;
    case 3:
      load(&c, "se127.wav", -6000, 1, 0);
      next.stage = 0;
      break;
    case 4:
      load(&c, "se128.wav", 0, 0, 1);
      next.stage = 5;
      break;
    default:
      break;
    }
    c.spatial = c.frequency = 1;
  } else if (kind == 1) {
    if (!train_source(c.source, prop, in->listener))
      return 0;
    c.spatial = c.frequency = 1;
  } else if (kind == 3 || kind == 4 || kind == 8 || kind == 9 || kind == 12) {
    c.spatial = 1;
  }
  if (c.spatial &&
      !bk_spatial_audio(&c.gain, c.source, in->listener, in->listener_yaw,
                        in->effect_volume, kind == 1 ? 2.f : 18.f))
    return 0;
  *state = next;
  *out = c;
  return 1;
}
