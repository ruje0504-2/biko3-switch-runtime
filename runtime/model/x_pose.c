#include "model/x_pose.h"
#include "model/animation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SOURCE_LIMIT (16u * 1024 * 1024)
#define FRAME_LIMIT 4096u
#define KEY_LIMIT 65536u
#define DEPTH_LIMIT 128u
typedef struct {
  uint32_t time, channel;
  float v[4];
} Key;
typedef struct {
  char name[65];
  Key *keys;
  uint32_t count, mask;
} Track;
typedef struct {
  const unsigned char *data;
  size_t size, pos;
  char token[128], *error;
  int kind, failed;
  BkModel *model;
  Track *tracks;
  uint32_t count, keys, sets;
} Parser;
/* Token kinds: 0=end, 1=identifier, 2=decimal, 3=string, punctuation=ASCII. */
static int fail(Parser *p, const char *s) {
  if (!p->failed)
    snprintf(p->error, 256, "text X pose at byte %zu: %s", p->pos, s);
  p->failed = 1;
  return 0;
}
static int space(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
static int alpha(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int digit(unsigned char c) { return c >= '0' && c <= '9'; }
static int next(Parser *p) {
  if (p->failed)
    return 0;
  for (;;) {
    while (p->pos < p->size &&
           (space(p->data[p->pos]) || p->data[p->pos] == ',' ||
            p->data[p->pos] == ';'))
      ++p->pos;
    if (p->pos + 1 < p->size && p->data[p->pos] == '/' &&
        p->data[p->pos + 1] == '/') {
      while (p->pos < p->size && p->data[p->pos] != '\n')
        ++p->pos;
      continue;
    }
    if (p->pos + 1 < p->size && p->data[p->pos] == '/' &&
        p->data[p->pos + 1] == '*') {
      p->pos += 2;
      while (p->pos + 1 < p->size &&
             !(p->data[p->pos] == '*' && p->data[p->pos + 1] == '/'))
        ++p->pos;
      if (p->pos + 1 >= p->size)
        return fail(p, "unterminated comment");
      p->pos += 2;
      continue;
    }
    break;
  }
  p->token[0] = 0;
  p->kind = 0;
  if (p->pos == p->size)
    return 1;
  size_t start = p->pos++;
  unsigned char c = p->data[start];
  if (c == '{' || c == '}') {
    p->kind = c;
    return 1;
  }
  if (alpha(c)) {
    p->kind = 1;
    while (p->pos < p->size &&
           (alpha(p->data[p->pos]) || digit(p->data[p->pos])))
      ++p->pos;
  } else if (digit(c) || c == '-' || c == '.') {
    p->kind = 2;
    while (p->pos < p->size &&
           (digit(p->data[p->pos]) || p->data[p->pos] == '.'))
      ++p->pos;
    /* The game's44b671 supports decimal notation, not exponents. */
    if (p->pos < p->size && alpha(p->data[p->pos]))
      return fail(p, "unsupported numeric suffix");
  } else if (c == '"') {
    p->kind = 3;
    while (p->pos < p->size && p->data[p->pos] != '"') {
      if (!p->data[p->pos] || p->data[p->pos] == '\n' ||
          p->data[p->pos] == '\r')
        return fail(p, "invalid string");
      ++p->pos;
    }
    if (p->pos == p->size)
      return fail(p, "unterminated string");
    ++p->pos;
  } else
    return fail(p, "unsupported token");
  size_t n = p->pos - start;
  if (n >= sizeof(p->token))
    return fail(p, "token too long");
  memcpy(p->token, p->data + start, n);
  p->token[n] = 0;
  return 1;
}
static int take(Parser *p, int kind) {
  return p->kind == kind ? next(p) : fail(p, "unexpected token");
}
static int opening(Parser *p, char name[65], int required) {
  name[0] = 0;
  if (p->kind == 1) {
    if (strlen(p->token) > 64)
      return fail(p, "name too long");
    strcpy(name, p->token);
    if (!next(p))
      return 0;
  } else if (required)
    return fail(p, "name required");
  return take(p, '{');
}
static int number(Parser *p, float *v) {
  if (p->kind != 2)
    return fail(p, "number required");
  char *end;
  double d = strtod(p->token, &end);
  *v = (float)d;
  if (end == p->token || *end || !isfinite(*v))
    return fail(p, "invalid finite decimal");
  return next(p);
}
static int integer(Parser *p, uint32_t *v) {
  if (p->kind != 2 || !digit((unsigned char)p->token[0]) ||
      strchr(p->token, '.'))
    return fail(p, "nonnegative integer required");
  char *end;
  unsigned long n = strtoul(p->token, &end, 10);
  if (*end || n > 16777215u)
    return fail(p, "integer exceeds exact float range");
  *v = (uint32_t)n;
  return next(p);
}
static int opaque(Parser *p) {
  char name[65];
  if (!opening(p, name, 0))
    return 0;
  unsigned depth = 1;
  while (depth) {
    if (!p->kind)
      return fail(p, "truncated opaque body");
    if (p->kind == '{' && ++depth > DEPTH_LIMIT)
      return fail(p, "nesting exceeds limit");
    if (p->kind == '}')
      --depth;
    if (!next(p))
      return 0;
  }
  return 1;
}
static int frame(Parser *p, uint32_t parent, unsigned depth) {
  char name[65];
  if (depth > DEPTH_LIMIT || p->model->frame_count == FRAME_LIMIT)
    return fail(p, "frame limit exceeded");
  if (!opening(p, name, 1))
    return 0;
  for (uint32_t i = 0; i < p->model->frame_count; ++i)
    if (!strcmp(name, p->model->frames[i].name))
      return fail(p, "duplicate frame name");
  uint32_t index = p->model->frame_count;
  BkModelFrame *frames =
      realloc(p->model->frames, (index + 1) * sizeof(*frames));
  if (!frames)
    return fail(p, "frame allocation failed");
  p->model->frames = frames;
  ++p->model->frame_count;
  BkModelFrame *f = frames + index;
  memset(f, 0, sizeof(*f));
  strcpy(f->name, name);
  f->id = index + 1;
  f->parent_index = parent;
  f->parent_id = parent == BK_MODEL_NONE ? 0 : parent + 1;
  f->mesh_index = BK_MODEL_NONE;
  f->local[0] = f->local[5] = f->local[10] = f->local[15] = 1;
  int matrix = 0;
  while (p->kind != '}') {
    if (p->kind != 1)
      return fail(p, "frame child required");
    char type[128];
    strcpy(type, p->token);
    if (!next(p))
      return 0;
    if (!strcmp(type, "Frame")) {
      if (!frame(p, index, depth + 1))
        return 0;
    } else if (!strcmp(type, "FrameTransformMatrix")) {
      if (matrix++ || !take(p, '{'))
        return fail(p, "duplicate/invalid frame matrix");
      for (unsigned i = 0; i < 16; ++i)
        if (!number(p, &p->model->frames[index].local[i]))
          return 0;
      if (!take(p, '}'))
        return 0;
    } else if (!strcmp(type, "Mesh")) {
      if (!opaque(p))
        return 0;
    } else
      return fail(p, "unsupported frame child");
  }
  return next(p);
}
static int keys(Parser *p, Track *t) {
  uint32_t channel, count;
  if (!take(p, '{') || !integer(p, &channel) || !integer(p, &count))
    return 0;
  if (channel > 2 || (t->mask & (1u << channel)) || !count ||
      count > KEY_LIMIT - p->keys)
    return fail(p, "unsupported/duplicate channel or key count");
  Key *k = realloc(t->keys, (t->count + count) * sizeof(*k));
  if (!k)
    return fail(p, "key allocation failed");
  t->keys = k;
  uint32_t start = t->count;
  t->count += count;
  p->keys += count;
  t->mask |= 1u << channel;
  for (uint32_t i = 0; i < count; ++i) {
    Key *v = k + start + i;
    memset(v, 0, sizeof(*v));
    uint32_t components;
    if (!integer(p, &v->time) || !integer(p, &components))
      return 0;
    if (components != (channel == 0 ? 4u : 3u) ||
        (i && v->time <= k[start + i - 1].time))
      return fail(p, "invalid key components/time ordering");
    v->channel = channel;
    for (unsigned n = 0; n < components; ++n)
      if (!number(p, &v->v[n]))
        return 0;
  }
  return take(p, '}');
}
static int animation(Parser *p) {
  char name[65];
  if (!opening(p, name, 0) || p->count >= FRAME_LIMIT)
    return fail(p, "animation limit/header");
  Track *ts = realloc(p->tracks, (p->count + 1) * sizeof(*ts));
  if (!ts)
    return fail(p, "track allocation failed");
  p->tracks = ts;
  Track *t = &ts[p->count++];
  memset(t, 0, sizeof(*t));
  while (p->kind != '}') {
    if (p->kind == '{') {
      if (*t->name || !next(p) || p->kind != 1 || strlen(p->token) > 64)
        return fail(p, "invalid/duplicate animation target");
      strcpy(t->name, p->token);
      if (!next(p) || !take(p, '}'))
        return 0;
    } else if (p->kind == 1 && !strcmp(p->token, "AnimationKey")) {
      if (!next(p) || !keys(p, t))
        return 0;
    } else
      return fail(p, "unsupported animation child");
  }
  if (!*t->name || !t->count)
    return fail(p, "animation target/keys missing");
  for (uint32_t i = 0; i + 1 < p->count; ++i)
    if (!strcmp(p->tracks[i].name, t->name))
      return fail(p, "duplicate animation target");
  return next(p);
}
static int key_order(const void *va, const void *vb) {
  const Key *a = va, *b = vb;
  if (a->time != b->time)
    return a->time < b->time ? -1 : 1;
  return a->channel < b->channel ? -1 : a->channel > b->channel;
}
static void put32(unsigned char *p, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    p[i] = (unsigned char)(v >> (i * 8));
}
static void putf(unsigned char *p, float f) {
  uint32_t v;
  memcpy(&v, &f, 4);
  put32(p, v);
}
static int finish(Parser *p) {
  BkModel *m = p->model;
  if (m->frame_count <= 1)
    return fail(p, "no authored frames");
  size_t anim_size = 72;
  for (uint32_t i = 0; i < p->count; ++i) {
    Track *t = p->tracks + i;
    qsort(t->keys, t->count, sizeof(Key), key_order);
    uint32_t count = 1; /* Native404f10 seeds time0, even if not authored. */
    for (uint32_t j = 0; j < t->count; ++j)
      if (t->keys[j].time && (!j || t->keys[j].time != t->keys[j - 1].time))
        ++count;
    if (count < 2)
      return fail(p, "single-time animation unsupported");
    anim_size += 24 + (size_t)count * 220;
  }
  m->source_size = p->size + (p->count ? anim_size : 0);
  m->source = malloc(m->source_size);
  if (!m->source)
    return fail(p, "source allocation failed");
  memcpy(m->source, p->data, p->size);
  if (!p->count)
    return 1;
  m->chunks = calloc(1, sizeof(*m->chunks));
  if (!m->chunks)
    return fail(p, "chunk allocation failed");
  m->chunk_count = 1;
  strcpy(m->chunks[0].tag, "ANIM");
  m->chunks[0].offset = (uint32_t)p->size;
  m->chunks[0].size = (uint32_t)anim_size;
  unsigned char *a = m->source + p->size;
  memset(a, 0, anim_size);
  put32(a + 68, p->count);
  size_t at = 72;
  for (uint32_t i = 0; i < p->count; ++i) {
    Track *t = p->tracks + i;
    uint32_t target;
    if (!bk_model_find_frame(m, t->name, &target, p->error)) {
      p->failed = 1;
      return 0;
    }
    put32(a + at, m->frames[target].id);
    size_t header = at;
    at += 24;
    unsigned char *key = a + at;
    /* Constructor seed: position0, identity quaternion and scale1, flagged. */
    put32(key + 4, 1);
    put32(key + 20, 1);
    put32(key + 36, 1);
    for (unsigned j = 0; j < 3; ++j)
      putf(key + 40 + j * 4, 1);
    putf(key + 128, 1);
    uint32_t count = 1, time = 0;
    for (uint32_t j = 0; j < t->count; ++j) {
      const Key *v = t->keys + j;
      if (v->time != time) {
        ++count;
        at += 220;
        key = a + at;
        time = v->time;
        putf(key, (float)time);
      }
      if (v->channel == 0) {
        put32(key + 20, 1);
        for (unsigned n = 0; n < 3; ++n)
          putf(key + 116 + n * 4, v->v[n + 1]);
        /*44795f: source quaternion is WXYZ; negate W, keep XYZ. */
        putf(key + 128, -v->v[0]);
      } else {
        unsigned flag = v->channel == 1 ? 36 : 4;
        unsigned offset = v->channel == 1 ? 40 : 8;
        put32(key + flag, 1);
        for (unsigned n = 0; n < 3; ++n)
          putf(key + offset + n * 4, v->v[n]);
      }
    }
    at += 220;
    put32(a + header + 20, count);
  }
  BkModelAnimation *check = bk_model_animation_create(m, p->error);
  if (!check) {
    p->failed = 1;
    return 0;
  }
  bk_model_animation_destroy(check);
  return 1;
}
BkModelResult bk_model_x_pose_decode(const void *data, size_t size,
                                     BkModel **out, char error[256]) {
  if (!out) {
    snprintf(error, 256, "text X pose: missing output");
    return BK_MODEL_INVALID;
  }
  *out = NULL;
  if (!data || size < 17 || size > SOURCE_LIMIT ||
      memcmp(data, "xof 0302txt 0032", 16) ||
      !space(((const unsigned char *)data)[16])) {
    snprintf(error, 256, "text X pose: unsupported header/size");
    return BK_MODEL_INVALID;
  }
  Parser p = {.data = data, .size = size, .pos = 16, .error = error};
  p.model = calloc(1, sizeof(*p.model));
  if (!p.model) {
    fail(&p, "model allocation failed");
    return BK_MODEL_INVALID;
  }
  p.model->frames = calloc(1, sizeof(*p.model->frames));
  if (!p.model->frames) {
    fail(&p, "root allocation failed");
    goto done;
  }
  p.model->frame_count = 1;
  BkModelFrame *root = p.model->frames;
  strcpy(root->name, "_XTextRoot_");
  root->id = 1;
  root->parent_index = root->mesh_index = BK_MODEL_NONE;
  root->local[0] = root->local[5] = root->local[10] = root->local[15] = 1;
  if (!next(&p))
    goto done;
  while (p.kind) {
    if (p.kind != 1) {
      fail(&p, "top-level template required");
      break;
    }
    char type[128];
    strcpy(type, p.token);
    if (!next(&p))
      break;
    if (!strcmp(type, "Frame")) {
      if (!frame(&p, 0, 1))
        break;
    } else if (!strcmp(type, "Material") || !strcmp(type, "Header")) {
      if (!opaque(&p))
        break;
    } else if (!strcmp(type, "AnimationSet")) {
      char name[65];
      if (p.sets++ || !opening(&p, name, 0)) {
        fail(&p, "multiple/invalid animation set");
        break;
      }
      while (p.kind == 1 && !strcmp(p.token, "Animation"))
        if (!next(&p) || !animation(&p))
          break;
      if (!take(&p, '}'))
        break;
    } else {
      fail(&p, "unsupported top-level template");
      break;
    }
  }
  if (!p.failed && finish(&p)) {
    *out = p.model;
    p.model = NULL;
  }
done:
  for (uint32_t i = 0; i < p.count; ++i)
    free(p.tracks[i].keys);
  free(p.tracks);
  bk_model_destroy(p.model);
  return *out ? BK_MODEL_OK : BK_MODEL_INVALID;
}
