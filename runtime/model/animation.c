#include "model/animation.h"
#include "core/matrix.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  float time, position[3], rotation[4], scale[3];
  uint32_t channels;
} Key;
typedef struct {
  uint32_t frame, count;
  Key *keys;
} Track;
struct BkModelAnimation {
  const BkModel *model;
  uint32_t count;
  float duration;
  Track *tracks;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "ANIM: %s", message);
  return 0;
}
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static float f32(const uint8_t *p) {
  uint32_t bits = u32(p);
  float value;
  memcpy(&value, &bits, 4);
  return value;
}
/* 0x523d9c: shortest arc, float-stored weights, no extra normalization. */
static void slerp(float out[4], const float a[4], const float b[4], float t) {
  double dot = (double)a[1] * b[1] + (double)a[2] * b[2] + (double)a[3] * b[3] +
               (double)a[0] * b[0];
  double sign = dot < 0 ? -1 : 1;
  float stored_dot = (float)dot;
  double cosine = sign * stored_dot;
  float left = 1.0f - t;
  double right = t;
  if (1.0 - cosine > (double)1e-5f) {
    double sine = sqrt(1.0 - cosine * cosine);
    float angle = (float)atan2(sine, cosine);
    left = (float)(sin((double)angle * left) / sine);
    right = sin((double)angle * t) / sine;
  }
  for (unsigned i = 0; i < 4; i++)
    out[i] = (float)((double)a[i] * left + (double)b[i] * (right * sign));
}
static float lerp(float a, float b, float t) {
  return (float)(((double)b - a) * t + a);
}
/* 0x4072d9 fills each missing channel between authored keys, holding the last
 * authored value at the end. The first record is the seed even if unflagged.
 * Search monotonically so a long sparse track remains linear-time. */
static void prepare(Track *track) {
  const unsigned order[] = {1, 2, 0};
  for (unsigned c = 0; c < 3; c++) {
    unsigned channel = order[c];
    uint32_t left = 0, right = 0;
    for (uint32_t i = 0; i < track->count; i++) {
      Key *key = &track->keys[i];
      if (key->channels & (1u << channel)) {
        left = i;
        continue;
      }
      if (right <= i) {
        right = i + 1;
        while (right < track->count &&
               !(track->keys[right].channels & (1u << channel)))
          right++;
      }
      const Key *a = &track->keys[left];
      const Key *b = right < track->count ? &track->keys[right] : a;
      float ratio = b == a ? 0
                           : (float)(((double)key->time - a->time) /
                                     ((double)b->time - a->time));
      if (channel == 1) {
        if (a == b)
          memcpy(key->rotation, a->rotation, sizeof(key->rotation));
        else
          slerp(key->rotation, a->rotation, b->rotation, ratio);
      } else {
        float *out = channel == 0 ? key->position : key->scale;
        const float *from = channel == 0 ? a->position : a->scale;
        const float *to = channel == 0 ? b->position : b->scale;
        for (unsigned j = 0; j < 3; j++)
          out[j] = a == b ? from[j] : lerp(from[j], to[j], ratio);
      }
    }
  }
}
/* 0x407d71 -> 0x523704: scale * quaternion rotation * translation.
 * Preserve the float intermediates in 0x5235e6. */
static void compose(float out[16], const Key *key) {
  bk_matrix_quaternion(out, key->rotation);
  for (unsigned i = 0; i < 3; i++)
    for (unsigned j = 0; j < 3; j++)
      out[i * 4 + j] = (float)((double)key->scale[i] * out[i * 4 + j]);
  memcpy(out + 12, key->position, 12);
  out[15] = 1;
}
void bk_model_animation_destroy(BkModelAnimation *a) {
  if (!a)
    return;
  for (uint32_t i = 0; i < a->count; i++)
    free(a->tracks[i].keys);
  free(a->tracks);
  free(a);
}
BkModelAnimation *bk_model_animation_create(const BkModel *m, char error[256]) {
  if (!m || !m->frames || !m->frame_count) {
    fail(error, "model/frames missing");
    return NULL;
  }
  const BkModelChunk *c = bk_model_chunk(m, "ANIM");
  if (!c) {
    /* XAN may wrap a static model (kage_01). Native 4026fe advances its
     * timeline but skips SRT when model+148 is NULL. No key data is invented.
     */
    BkModelAnimation *a = calloc(1, sizeof(*a));
    if (!a) {
      fail(error, "allocation failed");
      return NULL;
    }
    a->model = m;
    return a;
  }
  if (c->size < 72) {
    fail(error, "truncated header");
    return NULL;
  }
  const uint8_t *data = m->source + c->offset;
  uint32_t count = u32(data + 68);
  if (count > (c->size - 72) / 24) {
    fail(error, "invalid track count");
    return NULL;
  }
  BkModelAnimation *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->model = m;
  a->count = count;
  a->tracks = count ? calloc(count, sizeof(*a->tracks)) : NULL;
  if (count && !a->tracks) {
    a->count = 0;
    fail(error, "allocation failed");
    goto bad;
  }
  size_t pos = 72;
  for (uint32_t i = 0; i < count; i++) {
    if (c->size - pos < 24) {
      fail(error, "truncated track");
      goto bad;
    }
    const uint8_t *header = data + pos;
    pos += 24;
    Track *track = &a->tracks[i];
    track->frame = BK_MODEL_NONE;
    for (uint32_t j = 0; j < m->frame_count; j++)
      if (m->frames[j].id == u32(header))
        track->frame = j;
    if (track->frame == BK_MODEL_NONE) {
      fail(error, "missing target frame");
      goto bad;
    }
    /* 4097d6/409a94 submit every track in file order. h02_55 contains
     * repeated targets: later tracks replace earlier complete SRT matrices.
     * Keep all tracks (and their independent duration/preprocessing). */
    for (unsigned j = 4; j < 20; j += 4)
      if (u32(header + j)) {
        fail(error, "unsupported track header");
        goto bad;
      }
    track->count = u32(header + 20);
    if (track->count < 2 || track->count > 1000000 ||
        track->count > (c->size - pos) / 220) {
      fail(error, "unsupported key layout/count");
      goto bad;
    }
    track->keys = calloc(track->count, sizeof(*track->keys));
    if (!track->keys) {
      fail(error, "allocation failed");
      goto bad;
    }
    for (uint32_t j = 0; j < track->count; j++, pos += 220) {
      const uint8_t *p = data + pos;
      Key *k = &track->keys[j];
      if (u32(p + 4) > 1 || u32(p + 20) > 1 || u32(p + 36) > 1 ||
          u32(p + 160) != 0) {
        fail(error, "unsupported channel flags/curved key");
        goto bad;
      }
      k->channels = u32(p + 4) | u32(p + 20) << 1 | u32(p + 36) << 2;
      k->time = f32(p);
      if (!isfinite(k->time) || k->time < 0 || k->time >= 2147483648.0f ||
          (j == 0 ? k->time != 0 : k->time <= track->keys[j - 1].time)) {
        fail(error, "invalid key times");
        goto bad;
      }
      double norm = 0;
      for (unsigned n = 0; n < 3; n++) {
        k->position[n] = f32(p + 8 + n * 4);
        k->scale[n] = f32(p + 40 + n * 4);
      }
      for (unsigned n = 0; n < 4; n++) {
        k->rotation[n] = f32(p + 116 + n * 4);
        norm += (double)k->rotation[n] * k->rotation[n];
      }
      for (unsigned n = 0; n < 3; n++)
        if (!isfinite(k->position[n]) || !isfinite(k->scale[n])) {
          fail(error, "nonfinite key");
          goto bad;
        }
      if (!isfinite(norm)) {
        fail(error, "nonfinite quaternion");
        goto bad;
      }
    }
    prepare(track);
    for (uint32_t j = 0; j < track->count; j++) {
      float matrix[16];
      compose(matrix, &track->keys[j]);
      for (unsigned n = 0; n < 16; n++)
        if (!isfinite(matrix[n])) {
          fail(error, "key matrix overflow");
          goto bad;
        }
    }
    float end = track->keys[track->count - 1].time;
    if (end < 1) {
      fail(error, "unsupported sub-tick duration");
      goto bad;
    }
    if (end > a->duration)
      a->duration = end;
  }
  if (pos != c->size) {
    fail(error, "trailing bytes or unsupported layout");
    goto bad;
  }
  return a;
bad:
  bk_model_animation_destroy(a);
  return NULL;
}
uint32_t bk_model_animation_track_count(const BkModelAnimation *a) {
  return a ? a->count : 0;
}
float bk_model_animation_duration(const BkModelAnimation *a) {
  return a ? a->duration : 0;
}
static Key key_at(const Track *track, float t, int loop) {
  float end = track->keys[track->count - 1].time;
  if (t > end)
    t = loop ? (float)((int32_t)t % (int32_t)end) : end;
  uint32_t lo = 0, hi = track->count - 1;
  while (lo + 1 < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (track->keys[mid].time <= t)
      lo = mid;
    else
      hi = mid;
  }
  const Key *left = &track->keys[lo], *right = &track->keys[hi];
  if (t == right->time)
    return *right;
  if (t == left->time)
    return *left;
  Key key = {0};
  float ratio =
      (float)(((double)t - left->time) / ((double)right->time - left->time));
  for (unsigned j = 0; j < 3; j++) {
    key.position[j] = lerp(left->position[j], right->position[j], ratio);
    key.scale[j] = lerp(left->scale[j], right->scale[j], ratio);
  }
  slerp(key.rotation, left->rotation, right->rotation, ratio);
  return key;
}
static int pose(const BkModelAnimation *a, float from, float to, float weight,
                int blend, int loop, int base, const BkModelRootTransform *root,
                float *world, float *local_output, const float *previous_local,
                size_t count, char error[256]) {
  if (!a || !world || count != (size_t)a->model->frame_count * 16 ||
      !isfinite(from) || from < 0 || from >= 2147483648.0f || !isfinite(to) ||
      to < 0 || to >= 2147483648.0f || !isfinite(weight) ||
      (loop != 0 && loop != 1) || (blend != 0 && blend != 1))
    return fail(error, "invalid sample arguments");
  if (root) {
    if (root->frame >= a->model->frame_count ||
        a->model->frames[root->frame].parent_index != BK_MODEL_NONE)
      return fail(error, "placement target is not a root");
    for (uint32_t i = 0; i < a->count; i++)
      if (a->tracks[i].frame == root->frame)
        return fail(error, "animated root placement unsupported");
    for (unsigned i = 0; i < 16; i++)
      if (!isfinite(root->world[i]))
        return fail(error, "nonfinite root placement");
    if (root->world[3] || root->world[7] || root->world[11] ||
        root->world[15] == 0)
      return fail(error, "projective or zero-W root placement");
  }
  if (weight < 0)
    weight = 0;
  if (weight > 1)
    weight = 1;
  float *local = malloc(count * sizeof(float)),
        *result = malloc(count * sizeof(float));
  if (!local || !result) {
    free(local);
    free(result);
    return fail(error, "pose allocation failed");
  }
  for (uint32_t i = 0; i < a->model->frame_count; i++) {
    const float *seed =
        previous_local ? previous_local + i * 16 : a->model->frames[i].local;
    /* Placement is explicit on every call; NULL restores unanimated asset
     * roots. Tracked roots retain their most recently submitted animation. */
    if (previous_local && a->model->frames[i].parent_index == BK_MODEL_NONE) {
      int tracked = 0;
      for (uint32_t j = 0; j < a->count; j++)
        if (a->tracks[j].frame == i) {
          tracked = 1;
          break;
        }
      if (!tracked)
        seed = a->model->frames[i].local;
    }
    memcpy(local + i * 16, seed, 64);
  }
  for (uint32_t i = 0; !base && i < a->count; i++) {
    const Track *track = &a->tracks[i];
    Key key = key_at(track, from, loop);
    if (blend) {
      Key target = key_at(track, to, loop);
      float rotation[4];
      slerp(rotation, key.rotation, target.rotation, weight);
      memcpy(key.rotation, rotation, sizeof(rotation));
      for (unsigned j = 0; j < 3; j++) {
        /* 0x42d92f, not a linear translation crossfade. */
        float middle = lerp(key.position[j], target.position[j], weight);
        float left = lerp(key.position[j], middle, weight);
        float right = lerp(middle, target.position[j], weight);
        key.position[j] = lerp(left, right, weight);
        key.scale[j] = lerp(key.scale[j], target.scale[j], weight);
      }
    }
    compose(local + track->frame * 16, &key);
  }
  if (root)
    memcpy(local + root->frame * 16, root->world, 64);
  int ok = bk_model_pose_world_matrices(a->model, local, result, count, error);
  if (ok) {
    memcpy(world, result, count * sizeof(float));
    if (local_output)
      memcpy(local_output, local, count * sizeof(float));
  }
  free(local);
  free(result);
  return ok;
}
int bk_model_animation_sample(const BkModelAnimation *a, float time, int loop,
                              float *world, size_t count, char error[256]) {
  return pose(a, time, time, 0, 0, loop, 0, NULL, world, NULL, NULL, count,
              error);
}
int bk_model_animation_blend(const BkModelAnimation *a, float from, float to,
                             float weight, int loop, float *world, size_t count,
                             char error[256]) {
  return pose(a, from, to, weight, 1, loop, 0, NULL, world, NULL, NULL, count,
              error);
}
int bk_model_animation_pose(const BkModelAnimation *a,
                            const BkModelPoseSample *sample,
                            const BkModelRootTransform *root, float *world,
                            size_t count, char error[256]) {
  if (!sample)
    return fail(error, "missing sample request");
  return pose(a, sample->from, sample->to, sample->weight, sample->blend,
              sample->loop, 0, root, world, NULL, NULL, count, error);
}
int bk_model_animation_base_pose(const BkModelAnimation *a,
                                 const BkModelRootTransform *root, float *world,
                                 size_t count, char error[256]) {
  return pose(a, 0, 0, 0, 0, 0, 1, root, world, NULL, NULL, count, error);
}
int bk_model_animation_matrices(const BkModelAnimation *a,
                                const BkModelPoseSample *sample,
                                const BkModelRootTransform *root, float *world,
                                float *local, size_t count, char error[256]) {
  if (local && local == world)
    return fail(error, "local and world outputs overlap");
  if (!sample)
    return pose(a, 0, 0, 0, 0, 0, 1, root, world, local, NULL, count, error);
  return pose(a, sample->from, sample->to, sample->weight, sample->blend,
              sample->loop, 0, root, world, local, NULL, count, error);
}

int bk_model_animation_update(const BkModelAnimation *a,
                              const BkModelPoseSample *sample,
                              const BkModelRootTransform *root,
                              const float *previous_local, float *world,
                              float *local, size_t count, char error[256]) {
  if (!previous_local || !local || local == world)
    return fail(error, "missing/overlapping incremental pose arrays");
  if (!sample)
    return pose(a, 0, 0, 0, 0, 0, 1, root, world, local, previous_local, count,
                error);
  return pose(a, sample->from, sample->to, sample->weight, sample->blend,
              sample->loop, 0, root, world, local, previous_local, count,
              error);
}
