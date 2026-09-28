#include "model/material_animation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkMaterialTrack info;
  BkMaterialKey *keys;
} Track;
struct BkMaterialAnimation {
  Track *tracks;
  BkMaterialValuesEdit *pending;
  uint32_t count;
  float time;
};
static int fail(char *error, const char *why) {
  snprintf(error, 256, "MATA: %s", why);
  return 0;
}
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static float f32(const uint8_t *p) {
  uint32_t bits = u32(p);
  float value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}
static int valid_values(const BkMaterialValues *v) {
  for (unsigned i = 0; i < 4; i++)
    if (!isfinite(v->diffuse[i]) || !isfinite(v->ambient[i]) ||
        !isfinite(v->specular[i]) || !isfinite(v->emissive[i]))
      return 0;
  return isfinite(v->power);
}
void bk_material_animation_destroy(BkMaterialAnimation *a) {
  if (!a)
    return;
  if (a->tracks)
    for (uint32_t i = 0; i < a->count; i++)
      free(a->tracks[i].keys);
  free(a->tracks);
  free(a->pending);
  free(a);
}
BkMaterialAnimation *bk_material_animation_create(const BkModel *m,
                                                  char error[256]) {
  if (!m || (m->material_count && !m->materials)) {
    fail(error, "missing model/materials");
    return NULL;
  }
  const BkModelChunk *c = bk_model_chunk(m, "MATA");
  if (!c || !m->source || c->size < 72 || c->offset > m->source_size ||
      c->size > m->source_size - c->offset) {
    fail(error, "missing/truncated chunk");
    return NULL;
  }
  const uint8_t *data = m->source + c->offset;
  uint32_t count = u32(data + 68);
  if (count > 65536 || count > (c->size - 72) / 16) {
    fail(error, "invalid track count");
    return NULL;
  }
  BkMaterialAnimation *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->count = count;
  a->tracks = calloc(count ? count : 1, sizeof(*a->tracks));
  a->pending = calloc(count ? count : 1, sizeof(*a->pending));
  if (!a->tracks || !a->pending) {
    fail(error, "track allocation failed");
    goto bad;
  }
  size_t pos = 72;
  for (uint32_t i = 0; i < count; i++) {
    if (c->size - pos < 16) {
      fail(error, "truncated track");
      goto bad;
    }
    Track *t = a->tracks + i;
    uint32_t id = u32(data + pos), keys = u32(data + pos + 8);
    t->info = (BkMaterialTrack){BK_MODEL_NONE, id, u32(data + pos + 12), 1};
    pos += 16;
    if (keys > 1000000 || keys > (c->size - pos) / 72) {
      fail(error, "invalid key count");
      goto bad;
    }
    for (uint32_t j = 0; j < m->material_count; j++)
      if (m->materials[j].id == id) {
        if (t->info.material_index != BK_MODEL_NONE) {
          fail(error, "ambiguous material ID");
          goto bad;
        }
        t->info.material_index = j;
      }
    if (t->info.material_index == BK_MODEL_NONE) {
      fail(error, "missing material target");
      goto bad;
    }
    t->keys = calloc((size_t)keys + 1, sizeof(*t->keys));
    if (!t->keys) {
      fail(error, "key allocation failed");
      goto bad;
    }
    const BkModelMaterial *base = m->materials + t->info.material_index;
    BkMaterialState initial;
    if (!bk_material_state(base, 0, &initial, error))
      goto bad;
    BkMaterialValues *v = &t->keys[0].values;
    memcpy(v->diffuse, base->diffuse, 16);
    memcpy(v->ambient, base->ambient, 16);
    memcpy(v->specular, base->specular, 16);
    memcpy(v->emissive, base->emissive, 16);
    v->power = base->power;
    v->diffuse[3] = initial.encoded_alpha;
    for (uint32_t k = 0; k < keys; k++) {
      uint32_t bits = u32(data + pos);
      int32_t tick;
      memcpy(&tick, &bits, 4);
      BkMaterialKey key = {.time = (float)tick};
      if (tick < 0 || key.time >= 2147483648.f) {
        fail(error, "unsupported signed key time");
        goto bad;
      }
      for (unsigned j = 0; j < 4; j++) {
        key.values.diffuse[j] = f32(data + pos + 4 + j * 4);
        key.values.ambient[j] = f32(data + pos + 20 + j * 4);
        key.values.specular[j] = f32(data + pos + 36 + j * 4);
        key.values.emissive[j] = f32(data + pos + 52 + j * 4);
      }
      key.values.power = f32(data + pos + 68);
      pos += 72;
      if (!valid_values(&key.values)) {
        fail(error, "nonfinite key");
        goto bad;
      }
      uint32_t lo = 0, hi = t->info.key_count;
      while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (t->keys[mid].time < key.time)
          lo = mid + 1;
        else
          hi = mid;
      }
      if (lo == t->info.key_count || t->keys[lo].time != key.time) {
        memmove(t->keys + lo + 1, t->keys + lo,
                (t->info.key_count - lo) * sizeof(*t->keys));
        t->info.key_count++;
      }
      t->keys[lo] = key;
    }
  }
  if (pos != c->size) {
    fail(error, "trailing chunk bytes");
    goto bad;
  }
  return a;
bad:
  bk_material_animation_destroy(a);
  return NULL;
}
uint32_t bk_material_animation_tracks(const BkMaterialAnimation *a) {
  return a ? a->count : 0;
}
const BkMaterialTrack *bk_material_animation_track(const BkMaterialAnimation *a,
                                                   uint32_t i) {
  return a && i < a->count ? &a->tracks[i].info : NULL;
}
const BkMaterialKey *bk_material_animation_key(const BkMaterialAnimation *a,
                                               uint32_t i, uint32_t k) {
  return a && i < a->count && k < a->tracks[i].info.key_count
             ? &a->tracks[i].keys[k]
             : NULL;
}
float bk_material_animation_time(const BkMaterialAnimation *a) {
  return a ? a->time : 0;
}
static float lerp(float x, float y, float weight) {
  return (float)(((double)y - x) * weight + x);
}
static void sample(const Track *t, float time, BkMaterialValues *out) {
  const BkMaterialKey *first = t->keys, *last = first + t->info.key_count - 1;
  if (t->info.loop && last->time >= 1 && time >= last->time)
    time = (float)((int32_t)time % ((int32_t)last->time + 1));
  uint32_t lo = 0, hi = t->info.key_count;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (t->keys[mid].time < time)
      lo = mid + 1;
    else
      hi = mid;
  }
  if (lo < t->info.key_count && t->keys[lo].time == time) {
    *out = t->keys[lo].values;
    return;
  }
  const BkMaterialKey *left = lo ? t->keys + lo - 1 : first;
  const BkMaterialKey *right = lo < t->info.key_count ? t->keys + lo : first;
  float weight =
      (float)(((double)time - left->time) / ((double)right->time - left->time));
  *out = (BkMaterialValues){0};
  for (unsigned i = 0; i < 3; i++) {
    out->diffuse[i] =
        lerp(left->values.diffuse[i], right->values.diffuse[i], weight);
    out->specular[i] =
        lerp(left->values.specular[i], right->values.specular[i], weight);
    out->emissive[i] =
        lerp(left->values.emissive[i], right->values.emissive[i], weight);
    out->ambient[i] = 1;
  }
  out->diffuse[3] =
      lerp(left->values.diffuse[3], right->values.diffuse[3], weight);
  out->power = lerp(left->values.power, right->values.power, weight);
}
int bk_material_animation_sample(BkMaterialAnimation *a, float time,
                                 BkMaterialPose *pose, char error[256]) {
  if (!a || !pose || !isfinite(time) || time < 0 || time >= 2147483648.f)
    return fail(error, "invalid animation/pose/time");
  if (time == a->time)
    return 1;
  size_t count = 0;
  for (uint32_t i = 0; i < a->count; i++) {
    const Track *t = a->tracks + i;
    if (t->info.key_count < 2)
      continue;
    BkMaterialValuesEdit *edit = a->pending + count++;
    edit->index = t->info.material_index;
    edit->id = t->info.material_id;
    sample(t, time, &edit->values);
  }
  if (!bk_material_pose_values(pose, a->pending, count, error))
    return 0;
  a->time = time;
  return 1;
}
int bk_material_animation_restore(BkMaterialAnimation *a, BkMaterialPose *pose,
                                  char error[256]) {
  if (!a || !pose)
    return fail(error, "invalid restore target");
  for (uint32_t i = 0; i < a->count; i++) {
    const Track *t = a->tracks + i;
    a->pending[i] = (BkMaterialValuesEdit){
        t->info.material_index, t->info.material_id, t->keys[0].values};
  }
  return bk_material_pose_values(pose, a->pending, a->count, error);
}
