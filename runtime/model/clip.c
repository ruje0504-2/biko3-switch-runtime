#include "model/clip.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  float elapsed, source, rate, blend;
  int32_t loops, frame_counter;
} Timeline;
struct BkClipSet {
  char model[256];
  BkClipDefinition clips[BK_CLIP_SLOTS];
  BkClipState authored;
  Timeline saved[BK_CLIP_SLOTS];
  uint8_t empty[BK_CLIP_SLOTS];
  int32_t frame_interval[BK_CLIP_SLOTS];
};
struct BkClipPlayer {
  const BkClipSet *set;
  int allow_empty;
  int32_t slot, requested, ended, looped, blend_done;
  float blend_elapsed, blend_from, blend_to;
  Timeline timelines[BK_CLIP_SLOTS];
  /*0 inherits immutable resource; chain1/2=off/on, next1..128=slot+1.
   * Only256 bytes per instance, copied with its existing timeline state. */
  uint8_t chain_override[BK_CLIP_SLOTS], next_override[BK_CLIP_SLOTS];
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "XAN: %s", message);
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
static int valid_time(float t) {
  return isfinite(t) && t >= 0 && t < 2147483648.0f;
}
BkClipSet *bk_clip_set_decode(const void *bytes, size_t size, char error[256]) {
  const uint8_t *data = bytes;
  if (!data || size != 0x5190 || !memchr(data, 0, 256) ||
      !memchr(data + 256, 0, 256)) {
    fail(error, "unsupported size or unterminated model names");
    return NULL;
  }
  size_t length = strlen((const char *)data);
  if (length < 3 || strcmp((const char *)data, (const char *)data + 256) ||
      strcmp((const char *)data + length - 2, ".x")) {
    fail(error, "unsupported separate models or filename");
    return NULL;
  }
  /* A resource basename, never a path interpreted by the host filesystem. */
  for (size_t i = 0; i < length; i++)
    if (!((data[i] >= 'a' && data[i] <= 'z') ||
          (data[i] >= 'A' && data[i] <= 'Z') ||
          (data[i] >= '0' && data[i] <= '9') || data[i] == '_' ||
          data[i] == '-' || (i == length - 2 && data[i] == '.'))) {
      fail(error, "unsupported model basename");
      return NULL;
    }
  BkClipSet *set = calloc(1, sizeof(*set));
  if (!set) {
    fail(error, "allocation failed");
    return NULL;
  }
  memcpy(set->model, data, length + 1);
  const uint8_t *header = data + 512;
  set->authored = (BkClipState){.slot = (int32_t)u32(header + 0x140),
                                .requested = (int32_t)u32(header + 0x148),
                                .ended = (int32_t)u32(header + 0x184),
                                .looped = (int32_t)u32(header + 0x188),
                                .blend_done = (int32_t)u32(header + 0x164),
                                .blend_elapsed = f32(header + 0x168),
                                .blend_from = f32(header + 0x16c),
                                .blend_to = f32(header + 0x170)};
  for (unsigned i = 0; i < BK_CLIP_SLOTS; i++) {
    const uint8_t *p = data + 512 + 0x190 + i * 0x9c;
    set->saved[i] = (Timeline){f32(p + 0x44),          f32(p + 0x60),
                               f32(p + 0x5c),          f32(p + 0x7c),
                               (int32_t)u32(p + 0x64), (int32_t)u32(p + 0x6c)};
    set->frame_interval[i] = (int32_t)u32(p + 0x68);
    BkClipDefinition *d = &set->clips[i];
    d->start = f32(p + 0x54);
    d->end = f32(p + 0x58);
    if (!valid_time(d->start) || !valid_time(d->end))
      goto unsupported;
    int empty = d->start == 0 && d->end == 0;
    d->active = !empty;
    d->loop = (int32_t)u32(p);
    d->loop_start = (int32_t)u32(p + 0x48);
    d->duration = (int32_t)u32(p + 0x50);
    d->chain = (int32_t)u32(p + 0x70);
    d->next = (int32_t)u32(p + 0x74);
    d->chain_after = (int32_t)u32(p + 0x78);
    d->blend_ticks = f32(p + 0x7c);
    if ((d->loop != 0 && d->loop != 1) || d->duration < 0 ||
        (!d->duration && d->start != d->end) || d->loop_start < 0 ||
        (d->duration ? d->loop_start >= d->duration : d->loop_start != 0) ||
        (d->chain != 0 && d->chain != 1) || !valid_time(d->blend_ticks) ||
        u32(p + 0x4c))
      goto invalid_definition;
    if (d->chain && (d->next < 0 || d->next >= BK_CLIP_SLOTS ||
                     (d->loop && d->chain_after < 0)))
      goto invalid_definition;
    for (unsigned off = 0x80; off < 0x9c; off += 4)
      if (u32(p + off))
        goto invalid_definition;
    set->empty[i] = empty;
    continue;
  invalid_definition:
    /* Unused zero-range records can contain ignored editor residue. Only
     * well-formed static descriptors may be selected by loaded instances. */
    if (!empty)
      goto unsupported;
    memset(d, 0, sizeof(*d));
  }
  for (unsigned i = 0; i < BK_CLIP_SLOTS; i++) {
    const BkClipDefinition *d = &set->clips[i];
    if ((d->active || set->empty[i]) && d->chain &&
        !set->clips[d->next].active && !set->empty[d->next])
      goto unsupported;
  }
  return set;
unsupported:
  fail(error, "unsupported clip fields or invalid chain target");
  free(set);
  return NULL;
}
void bk_clip_set_destroy(BkClipSet *s) { free(s); }
const char *bk_clip_model_name(const BkClipSet *s) {
  return s ? s->model : NULL;
}
const BkClipDefinition *bk_clip_definition(const BkClipSet *s, unsigned slot) {
  return s && slot < BK_CLIP_SLOTS ? &s->clips[slot] : NULL;
}
BkClipPlayer *bk_clip_player_create(const BkClipSet *s, char error[256]) {
  if (!s) {
    fail(error, "missing clip set");
    return NULL;
  }
  BkClipPlayer *p = calloc(1, sizeof(*p));
  if (!p) {
    fail(error, "player allocation failed");
    return NULL;
  }
  p->set = s;
  p->slot = p->requested = -1;
  for (unsigned i = 0; i < BK_CLIP_SLOTS; i++)
    p->timelines[i].blend = s->clips[i].blend_ticks;
  return p;
}
void bk_clip_player_destroy(BkClipPlayer *p) { free(p); }
int bk_clip_player_copy(BkClipPlayer *destination, const BkClipPlayer *source) {
  if (!destination || !source || destination->set != source->set)
    return 0;
  *destination = *source;
  return 1;
}
int bk_clip_link(const BkClipPlayer *p, unsigned slot, int32_t *chain,
                 int32_t *next) {
  if (!p || slot >= BK_CLIP_SLOTS || !chain || !next)
    return 0;
  *chain = p->chain_override[slot] ? p->chain_override[slot] - 1
                                   : p->set->clips[slot].chain;
  *next = p->next_override[slot] ? p->next_override[slot] - 1
                                 : p->set->clips[slot].next;
  return 1;
}
int bk_clip_edit(BkClipPlayer *p, const BkClipEdit *edits, size_t count,
                 char error[256]) {
  if (!p || (count && !edits))
    return fail(error, "missing clip edits/player");
  if (!count)
    return 1;
  BkClipPlayer next = *p;
  uint8_t touched[BK_CLIP_SLOTS] = {0};
  for (size_t i = 0; i < count; ++i) {
    const BkClipEdit *e = &edits[i];
    if (e->slot >= BK_CLIP_SLOTS || !e->fields || (e->fields & ~7u) ||
        ((e->fields & BK_CLIP_EDIT_CHAIN) && e->chain != 0 && e->chain != 1) ||
        ((e->fields & BK_CLIP_EDIT_NEXT) &&
         (e->next < 0 || e->next >= BK_CLIP_SLOTS)) ||
        ((e->fields & BK_CLIP_EDIT_SOURCE) && !valid_time(e->source)))
      return fail(error, "invalid runtime clip edit");
    if (e->fields & BK_CLIP_EDIT_CHAIN)
      next.chain_override[e->slot] = (uint8_t)(e->chain + 1);
    if (e->fields & BK_CLIP_EDIT_NEXT)
      next.next_override[e->slot] = (uint8_t)(e->next + 1);
    if (e->fields & BK_CLIP_EDIT_SOURCE)
      next.timelines[e->slot].source = e->source;
    if (e->fields & (BK_CLIP_EDIT_CHAIN | BK_CLIP_EDIT_NEXT))
      touched[e->slot] = 1;
  }
  for (unsigned i = 0; i < BK_CLIP_SLOTS; ++i) {
    int32_t chain, target;
    if (!touched[i])
      continue;
    bk_clip_link(&next, i, &chain, &target);
    if (chain && (target < 0 || target >= BK_CLIP_SLOTS ||
                  (!p->set->clips[target].active && !p->set->empty[target]) ||
                  (p->set->clips[i].loop && p->set->clips[i].chain_after < 0)))
      return fail(error, "invalid runtime chain target/threshold");
  }
  *p = next;
  return 1;
}
int bk_clip_reset_sources(BkClipPlayer *p, const unsigned *slots, size_t count,
                          char error[256]) {
  if (!p || (count && !slots))
    return fail(error, "missing source reset/player");
  if (!count)
    return 1;
  BkClipPlayer next = *p;
  for (size_t i = 0; i < count; ++i) {
    unsigned slot = slots[i];
    if (slot >= BK_CLIP_SLOTS)
      return fail(error, "invalid source reset slot");
    next.timelines[slot].source = p->set->clips[slot].start;
  }
  *p = next;
  return 1;
}
static BkClipPlayer *authored(const BkClipSet *s, int empty, char error[256]) {
  if (!s) {
    fail(error, "missing clip set");
    return NULL;
  }
  const BkClipState *seed = &s->authored;
  if (seed->slot < 0 || seed->slot >= BK_CLIP_SLOTS || seed->requested < 0 ||
      seed->requested >= BK_CLIP_SLOTS ||
      (!s->clips[seed->slot].active && !(empty && s->empty[seed->slot])) ||
      (!s->clips[seed->requested].active &&
       !(empty && s->empty[seed->requested])) ||
      (seed->ended != 0 && seed->ended != 1) ||
      (seed->looped != 0 && seed->looped != 1) ||
      (seed->blend_done != 0 && seed->blend_done != 1) ||
      !valid_time(seed->blend_elapsed) || !valid_time(seed->blend_from) ||
      !valid_time(seed->blend_to))
    goto bad_seed;
  for (unsigned i = 0; i < BK_CLIP_SLOTS; i++) {
    const Timeline *t = &s->saved[i];
    if ((s->clips[i].active || (empty && (i == (unsigned)seed->slot ||
                                          i == (unsigned)seed->requested))) &&
        (!valid_time(t->elapsed) || !valid_time(t->source) ||
         (!isfinite(t->rate) &&
          !(empty && i == (unsigned)seed->slot && !s->clips[i].active &&
            t->rate == -INFINITY && !s->clips[i].duration)) ||
         t->loops < 0))
      goto bad_seed;
  }
  BkClipPlayer *p = bk_clip_player_create(s, error);
  if (!p)
    return NULL;
  p->slot = seed->slot;
  p->allow_empty = empty;
  p->requested = seed->requested;
  p->ended = seed->ended;
  p->looped = seed->looped;
  p->blend_done = seed->blend_done;
  p->blend_elapsed = seed->blend_elapsed;
  p->blend_from = seed->blend_from;
  p->blend_to = seed->blend_to;
  memcpy(p->timelines, s->saved, sizeof(p->timelines));
  return p;
bad_seed:
  fail(error, "invalid authored playback seed");
  return NULL;
}
BkClipPlayer *bk_clip_player_create_authored(const BkClipSet *s,
                                             char error[256]) {
  return authored(s, 0, error);
}
BkClipPlayer *bk_clip_player_create_loaded(const BkClipSet *s,
                                           char error[256]) {
  return authored(s, 1, error);
}
static void select_clip(BkClipPlayer *p, unsigned slot) {
  const BkClipDefinition *d = &p->set->clips[slot];
  Timeline *t = &p->timelines[slot];
  t->elapsed = 0;
  t->source = d->start;
  t->rate =
      d->duration
          ? (float)(((double)d->end - (double).1f - d->start) / d->duration)
          : 0;
  t->loops = 0;
  t->frame_counter = 0;
  p->slot = (int32_t)slot;
  p->ended = p->looped = 0;
}
static void transition(BkClipPlayer *p, unsigned slot, float duration) {
  p->blend_from = p->slot < 0 ? 0 : p->timelines[p->slot].source;
  select_clip(p, slot);
  p->requested = (int32_t)slot;
  p->blend_done = 0;
  p->blend_elapsed = 0;
  p->timelines[slot].blend = duration;
  const BkClipDefinition *d = &p->set->clips[slot];
  p->blend_to = (float)((double)d->start + p->timelines[slot].blend);
  if (p->blend_to >= d->end)
    p->blend_to = d->end;
}
int bk_clip_select(BkClipPlayer *p, unsigned slot, int instant,
                   char error[256]) {
  if (!p || slot >= BK_CLIP_SLOTS ||
      (!p->set->clips[slot].active &&
       !(p->allow_empty && p->set->empty[slot])) ||
      (instant != 0 && instant != 1))
    return fail(error, "invalid clip selection");
  /* 401d24/401f71 ignore a zero-range descriptor. Unlike request, this
   * explicit selection must not reset an authored static timeline. */
  if (!p->set->clips[slot].active)
    return 1;
  transition(p, slot, instant ? 2e-6f : p->timelines[slot].blend);
  return 1;
}
int bk_clip_request_active(BkClipPlayer *p, unsigned slot, char error[256]) {
  if (!p || slot >= BK_CLIP_SLOTS ||
      (!p->set->clips[slot].active && !p->set->empty[slot]))
    return fail(error, "invalid active-slot request");
  /*40168c tests empty ranges BEFORE comparing active, not requested. */
  if (p->set->clips[slot].active && p->slot != (int32_t)slot)
    transition(p, slot, p->timelines[slot].blend);
  return 1;
}
int bk_clip_request(BkClipPlayer *p, unsigned slot, char error[256]) {
  return bk_clip_request_mode(p, slot, BK_CLIP_REQUEST_TEN_TICKS, error);
}
int bk_clip_request_mode(BkClipPlayer *p, unsigned slot, BkClipRequestMode mode,
                         char error[256]) {
  if (!p || slot >= BK_CLIP_SLOTS ||
      (!p->set->clips[slot].active &&
       !(p->allow_empty && p->set->empty[slot])) ||
      (mode != BK_CLIP_REQUEST_TEN_TICKS && mode != BK_CLIP_REQUEST_CONFIGURED))
    return fail(error, "invalid clip request");
  /*4018c8 ignores empty descriptors before the requested-slot comparison.
   * 401b0a does not: keep its ten-tick transition into loaded empty slots. */
  if (mode == BK_CLIP_REQUEST_CONFIGURED && !p->set->clips[slot].active)
    return 1;
  if (p->requested != (int32_t)slot)
    transition(p, slot,
               mode == BK_CLIP_REQUEST_TEN_TICKS ? 10
                                                 : p->timelines[slot].blend);
  return 1;
}
BkClipPlayer *bk_clip_player_create_authored_start(const BkClipSet *s,
                                                   unsigned slot, int instant,
                                                   char error[256]) {
  if (!s || slot >= BK_CLIP_SLOTS || !s->clips[slot].active ||
      (instant != 0 && instant != 1)) {
    fail(error, "invalid authored initial selection");
    return NULL;
  }
  const BkClipState *seed = &s->authored;
  if (!instant && seed->requested == (int32_t)slot)
    return bk_clip_player_create_authored(s, error);
  if (seed->slot < 0 || seed->slot >= BK_CLIP_SLOTS || seed->requested < 0 ||
      seed->requested >= BK_CLIP_SLOTS ||
      !valid_time(s->saved[seed->slot].source)) {
    fail(error, "invalid source for authored initial transition");
    return NULL;
  }
  BkClipPlayer *p = bk_clip_player_create(s, error);
  if (!p)
    return NULL;
  /* transition reads only the old slot's source, then replaces current
   * timeline and all global playback scalars. Keep other saved timelines;
   * selecting them later resets their phase but uses their authored blend. */
  p->slot = seed->slot;
  p->requested = seed->requested;
  memcpy(p->timelines, s->saved, sizeof(p->timelines));
  transition(p, slot, instant ? 2e-6f : 10);
  return p;
}
static int over_end(const Timeline *t, const BkClipDefinition *d) {
  /* x87's negative-rate branch tests C0 alone: unordered also clamps.
   * Real door rest clips use -Inf and produce 0*-Inf on loop reset. */
  return t->rate >= 0 ? t->source > d->end : !(t->source >= d->end);
}
static void chain_clip(BkClipPlayer *p, unsigned slot) {
  const BkClipDefinition *d = &p->set->clips[slot];
  /* 4025b9 ignores zero-range targets and, unlike explicit selection,
   * divides by zero for a nonzero static pose. Preserve its -Inf rate. */
  if (!d->active)
    return;
  select_clip(p, slot);
  if (!d->duration)
    p->timelines[slot].rate = -INFINITY;
}
static void plain_chain(BkClipPlayer *p, unsigned slot, int mode) {
  if (mode == BK_CLIP_PLAIN_FORCE_CHAIN) {
    /*4a8f2d neither checks an empty range nor substitutes a zero rate.
     * A static/empty zero-duration target retains its original -Inf rate. */
    select_clip(p, slot);
    if (!p->set->clips[slot].duration)
      p->timelines[slot].rate = -INFINITY;
  } else
    chain_clip(p, slot);
}
static int advance(BkClipPlayer *player, float seconds, BkClipSample *out,
                     int plain_mode, char error[256]) {
  if (!player || !out || player->slot < 0 || player->slot >= BK_CLIP_SLOTS ||
      !isfinite(seconds) || seconds < 0 || (double)seconds * 60 >= INT32_MAX)
    return fail(error, "invalid timestep or unselected player");
  /* Small fixed-size CPU state; commit only after checking all float results.
   */
  BkClipPlayer next = *player;
  BkClipPlayer *p = &next;
  float delta;
  if (!p->blend_done && p->blend_elapsed < 1e-6f) {
    delta = 0;
    p->blend_elapsed = 2e-6f;
  } else
    delta = (float)((double)seconds * 60);
  const BkClipDefinition *d = &p->set->clips[p->slot];
  Timeline *t = &p->timelines[p->slot];
  int32_t chain, target;
  if (!bk_clip_link(p, (unsigned)p->slot, &chain, &target))
    return fail(error, "invalid active clip link");
  /*4027cc clears the fixed-call counter even on a zero-duration step. */
  t->frame_counter = 0;
  t->source = (float)((double)delta * t->rate + t->source);
  if (over_end(t, d)) {
    t->source = d->end;
    delta = 0;
    t->elapsed = (float)d->duration;
  }
  if (plain_mode != BK_CLIP_PLAIN_SOURCE)
    t->elapsed = (float)((double)delta + t->elapsed);
  if (plain_mode != BK_CLIP_PLAIN_SOURCE &&
      (double)d->duration <= t->elapsed) {
    if (d->loop) {
      if (t->loops == INT32_MAX)
        return fail(error, "loop counter overflow");
      t->loops++;
      if (chain && t->loops >= d->chain_after) {
        plain_chain(p, (unsigned)target, plain_mode);
        p->ended = 1;
      } else {
        t->elapsed = (float)d->loop_start;
        p->ended = p->looped = 1;
        t->source = (float)((double)d->loop_start * t->rate + d->start);
        if (over_end(t, d))
          t->source = d->end;
      }
    } else if (chain) {
      plain_chain(p, (unsigned)target, plain_mode);
      p->ended = 1;
    } else {
      t->elapsed = (float)d->duration;
      p->ended = 1;
      t->source = d->end;
    }
  }
  /* t still points at the old clip even if an automatic chain selected another
   * slot. This order is observable in the original submitted source pose. */
  BkClipSample sample = {0};
  if (!p->blend_done) {
    float duration = p->timelines[p->slot].blend;
    if (duration < 1e-6f)
      p->blend_done = 1;
    else if (plain_mode < 0) {
      p->blend_elapsed = (float)((double)delta + p->blend_elapsed);
      if (p->blend_elapsed > duration) {
        p->blend_done = 1;
        t->source = p->blend_to;
      }
      sample.blend = 1;
      sample.from = p->blend_from;
      sample.to = p->blend_to;
      sample.weight = (float)((double)p->blend_elapsed / duration);
      if (sample.weight > 1)
        sample.weight = 1;
    }
  }
  if (!sample.blend)
    sample.from = sample.to = t->source;
  if (!valid_time(t->source) || !valid_time(sample.from) ||
      !valid_time(sample.to) || !isfinite(t->elapsed) ||
      !isfinite(p->blend_elapsed) || !isfinite(sample.weight))
    return fail(error, "timestep overflow");
  *player = next;
  *out = sample;
  return 1;
}
int bk_clip_advance(BkClipPlayer *player, float seconds, BkClipSample *out,
                     char error[256]) {
  return advance(player, seconds, out, -1, error);
}
int bk_clip_advance_plain(BkClipPlayer *player, float seconds,
                           BkClipPlainMode mode, BkClipSample *out,
                           char error[256]) {
  if (mode < BK_CLIP_PLAIN_SCHEDULED || mode > BK_CLIP_PLAIN_FORCE_CHAIN)
    return fail(error, "invalid plain scheduler mode");
  return advance(player, seconds, out, (int)mode, error);
}
int bk_clip_set_clock(BkClipPlayer *p, unsigned slot, float elapsed,
                       float source, char error[256]) {
  if (!p || slot >= BK_CLIP_SLOTS || !isfinite(elapsed) || elapsed < 0 ||
      (double)elapsed >= INT32_MAX || !valid_time(source))
    return fail(error, "invalid controller clock edit");
  p->timelines[slot].elapsed = elapsed;
  p->timelines[slot].source = source;
  return 1;
}
int bk_clip_frame_clock(const BkClipPlayer *p, unsigned slot, int32_t *interval,
                        int32_t *counter) {
  if (!p || slot >= BK_CLIP_SLOTS || !interval || !counter)
    return 0;
  *interval = p->set->frame_interval[slot];
  *counter = p->timelines[slot].frame_counter;
  return 1;
}
int bk_clip_advance_frame(BkClipPlayer *player, BkClipSample *out,
                          char error[256]) {
  if (!player || !out || player->slot < 0 || player->slot >= BK_CLIP_SLOTS)
    return fail(error, "unselected frame player");
  BkClipPlayer next = *player;
  BkClipPlayer *p = &next;
  const BkClipDefinition *d = &p->set->clips[p->slot];
  Timeline *t = &p->timelines[p->slot];
  int32_t chain, target;
  if (!bk_clip_link(p, (unsigned)p->slot, &chain, &target))
    return fail(error, "invalid frame clip link");
  /* Native add wraps32 bits, then compares signed with descriptor+68. */
  uint32_t counter = (uint32_t)t->frame_counter + 1u;
  memcpy(&t->frame_counter, &counter, 4);
  if (t->frame_counter >= p->set->frame_interval[p->slot]) {
    t->frame_counter = 0;
    t->source = (float)((double)t->source + t->rate);
    t->elapsed = (float)((double)t->elapsed + 1);
    if ((double)d->duration <= t->elapsed) {
      if (d->loop) {
        if (t->loops == INT32_MAX)
          return fail(error, "frame loop counter overflow");
        t->loops++;
        if (chain && t->loops >= d->chain_after)
          chain_clip(p, (unsigned)target);
        else {
          t->elapsed = (float)d->loop_start;
          p->ended = p->looped = 1;
          t->source = (float)((double)d->loop_start * t->rate + d->start);
        }
      } else if (chain)
        chain_clip(p, (unsigned)target);
      else {
        t->elapsed = (float)d->duration;
        p->ended = 1;
        t->source = d->end;
      }
    }
  }
  /*4021a1 always submits plain old-descriptor source. It never advances
   * blend clocks, suppresses a first delta, or clamps source by rate sign.
   * An automatic chain does not set ended after4025b9 clears it. */
  BkClipSample sample = {0, t->source, t->source, 0};
  if (!valid_time(t->source) || !isfinite(t->elapsed))
    return fail(error, "invalid frame sample");
  *player = next;
  *out = sample;
  return 1;
}
int bk_clip_state(const BkClipPlayer *p, BkClipState *s) {
  if (!p || !s || p->slot < 0)
    return 0;
  const Timeline *t = &p->timelines[p->slot];
  *s = (BkClipState){p->slot,          p->requested,  p->ended,
                     p->looped,        p->blend_done, t->loops,
                     t->elapsed,       t->source,     t->rate,
                     p->blend_elapsed, p->blend_from, p->blend_to};
  return 1;
}
int bk_clip_timing(const BkClipPlayer *p, unsigned slot, BkClipTiming *out) {
  if (!p || !out || slot >= BK_CLIP_SLOTS)
    return 0;
  *out = (BkClipTiming){p->set->clips[slot].start, p->set->clips[slot].end,
                        p->timelines[slot].source};
  return 1;
}
int bk_clip_loops(const BkClipPlayer *p, unsigned slot, int32_t *out) {
  if (!p || !out || slot >= BK_CLIP_SLOTS)
    return 0;
  *out = p->timelines[slot].loops;
  return 1;
}
int bk_clip_prediction(const BkClipPlayer *p, unsigned slot,
                       BkClipPrediction *out) {
  if (!p || !out || slot >= BK_CLIP_SLOTS)
    return 0;
  *out = (BkClipPrediction){p->set->clips[slot].duration,
                            p->set->clips[slot].end,
                            p->timelines[slot].source,
                            p->timelines[slot].rate};
  return 1;
}
