#include "model/clip.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void word(unsigned char *p, uint32_t x) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (unsigned char)(x >> (i * 8));
}
static void number(unsigned char *p, float x) {
  uint32_t bits;
  memcpy(&bits, &x, 4);
  word(p, bits);
}
static void runtime_edits(const unsigned char *raw) {
  unsigned char data[0x5190];
  memcpy(data, raw, sizeof(data));
  unsigned char *second = data + 512 + 0x190 + 156;
  word(second + 0x50, 10);
  number(second + 0x54, 30);
  number(second + 0x58, 40);
  char e[256];
  BkClipSet *set = bk_clip_set_decode(data, sizeof(data), e);
  assert(set);
  BkClipPlayer *a = bk_clip_player_create(set, e);
  BkClipPlayer *b = bk_clip_player_create(set, e);
  BkClipPlayer *clone = bk_clip_player_create(set, e);
  assert(a && b && clone && bk_clip_select(a, 0, 1, e));
  BkClipSample sample;
  assert(bk_clip_advance(a, 0, &sample, e));
  assert(bk_clip_advance(a, .01f, &sample, e));
  BkClipState old, state;
  assert(bk_clip_state(a, &old));
  BkClipEdit edits[] = {{0, BK_CLIP_EDIT_CHAIN, 1, 0, 0},
                        {0, BK_CLIP_EDIT_NEXT, 0, 1, 0},
                        {0, BK_CLIP_EDIT_SOURCE, 0, 0, 19}};
  assert(bk_clip_edit(a, edits, 3, e));
  assert(bk_clip_state(a, &state));
  old.source = 19;
  assert(!memcmp(&old, &state, sizeof(state)));
  int32_t chain, next;
  assert(bk_clip_link(a, 0, &chain, &next) && chain == 1 && next == 1);
  assert(bk_clip_link(b, 0, &chain, &next) && chain == 0 && next == 0);
  assert(bk_clip_definition(set, 0)->chain == 0);
  assert(bk_clip_player_copy(clone, a));
  assert(bk_clip_link(clone, 0, &chain, &next) && chain == 1 && next == 1);
  const BkClipEdit invalid[] = {{1, BK_CLIP_EDIT_SOURCE, 0, 0, 34},
                                {0, BK_CLIP_EDIT_NEXT, 0, 128, 0}};
  BkClipTiming timing, after;
  assert(bk_clip_timing(a, 1, &timing));
  assert(!bk_clip_edit(a, invalid, 2, e));
  assert(bk_clip_timing(a, 1, &after) &&
         !memcmp(&timing, &after, sizeof(timing)));
  assert(bk_clip_state(a, &state) && !memcmp(&old, &state, sizeof(state)));
  assert(!bk_clip_edit(a, (BkClipEdit[]){{0, BK_CLIP_EDIT_SOURCE, 0, 0, NAN}},
                       1, e));
  assert(!bk_clip_edit(a, (BkClipEdit[]){{128, BK_CLIP_EDIT_CHAIN, 0, 0, 0}}, 1,
                       e));
  assert(!bk_clip_edit(a, (BkClipEdit[]){{0, 8, 0, 0, 0}}, 1, e));
  assert(
      !bk_clip_edit(a, (BkClipEdit[]){{0, BK_CLIP_EDIT_CHAIN, 2, 0, 0}}, 1, e));
  assert(bk_clip_edit(a, NULL, 0, e));
  assert(bk_clip_advance(a, 1, &sample, e));
  assert(bk_clip_state(a, &state) && state.slot == 1 && state.requested == 0);
  assert(bk_clip_state(clone, &state) && state.slot == 0 && state.source == 19);
  /* 4025b9 ignores an empty target, even in a fresh instance. */
  assert(bk_clip_edit(clone, (BkClipEdit[]){{0, BK_CLIP_EDIT_NEXT, 0, 127, 0}},
                      1, e));
  assert(bk_clip_advance(clone, 1, &sample, e));
  assert(bk_clip_state(clone, &state) && state.slot == 0 && state.ended);
  bk_clip_player_destroy(a);
  bk_clip_player_destroy(b);
  bk_clip_player_destroy(clone);
  bk_clip_set_destroy(set);
}
int main(void) {
  unsigned char data[0x5190] = {0}, copy[sizeof(data)];
  memcpy(data, "camera.x", 9);
  memcpy(data + 256, "camera.x", 9);
  unsigned char *clip = data + 512 + 0x190;
  word(clip + 0x50, 4);
  number(clip + 0x54, 10);
  number(clip + 0x58, 20);
  memcpy(copy, data, sizeof(data));
  runtime_edits(copy);
  char error[256];
  BkClipSet *set = bk_clip_set_decode(data, sizeof(data), error);
  assert(set);
  BkClipPlayer *p = bk_clip_player_create(set, error);
  assert(p);
  BkClipSample out;
  BkClipState state, saved;
  assert(!bk_clip_advance(p, .01f, &out, error));
  assert(bk_clip_select(p, 0, 1, error));
  assert(bk_clip_advance(p, 1, &out, error));
  assert(out.blend && out.weight == 1 && fabsf(out.to - 10) < .00001f);
  assert(bk_clip_state(p, &state) && state.elapsed == 0 && !state.blend_done);
  assert(bk_clip_advance(p, 1.f / 120, &out, error));
  assert(out.blend && out.weight == 1);
  assert(bk_clip_state(p, &state) && state.blend_done);
  assert(bk_clip_advance(p, 1, &out, error));
  assert(!out.blend && out.from == 20);
  assert(bk_clip_state(p, &saved) && saved.ended && !saved.looped);
  const float bad[] = {-1, NAN, INFINITY, 1e30f};
  for (unsigned i = 0; i < 4; i++) {
    BkClipSample before = out;
    assert(!bk_clip_advance(p, bad[i], &out, error));
    assert(!memcmp(&before, &out, sizeof(out)));
    assert(bk_clip_state(p, &state) && !memcmp(&state, &saved, sizeof(state)));
  }
  assert(!bk_clip_select(p, 1, 1, error));
  assert(!bk_clip_select(p, 128, 1, error));
  assert(bk_clip_state(p, &state) && !memcmp(&state, &saved, sizeof(state)));
  assert(!memcmp(data, copy, sizeof(data)));
  bk_clip_player_destroy(p);
  bk_clip_set_destroy(set);
  /* A chain changes active slot but retains the last requested action. An
   * identical per-frame request must not restart the chain's source clip. */
  word(clip, 1);
  word(clip + 0x70, 1);
  word(clip + 0x74, 1);
  word(clip + 0x78, 0);
  unsigned char *second = clip + 156;
  word(second + 0x50, 100);
  number(second + 0x54, 30);
  number(second + 0x58, 40);
  set = bk_clip_set_decode(data, sizeof(data), error);
  assert(set);
  p = bk_clip_player_create(set, error);
  assert(p && bk_clip_request(p, 0, error));
  assert(bk_clip_advance(p, 0, &out, error));
  assert(bk_clip_advance(p, 1, &out, error));
  assert(bk_clip_state(p, &saved) && saved.slot == 1 && saved.requested == 0);
  int32_t loops = -99;
  assert(bk_clip_loops(p, 0, &loops) && loops == 1);
  assert(bk_clip_loops(p, 1, &loops) && loops == saved.loops && loops == 0);
  loops = 73;
  assert(!bk_clip_loops(p, 128, &loops) && loops == 73);
  assert(!bk_clip_loops(NULL, 0, &loops) && loops == 73);
  assert(!bk_clip_loops(p, 0, NULL));
  assert(bk_clip_state(p, &state) && !memcmp(&state, &saved, sizeof(state)));
  BkClipTiming retained, active;
  assert(bk_clip_timing(p, 0, &retained) && retained.start == 10 &&
         retained.end == 20);
  assert(bk_clip_timing(p, 1, &active) && active.source == saved.source);
  assert(bk_clip_advance(p, .1f, &out, error));
  BkClipTiming after_chain;
  assert(bk_clip_timing(p, 0, &after_chain) &&
         !memcmp(&retained, &after_chain, sizeof(retained)));
  assert(!bk_clip_timing(p, 128, &after_chain) &&
         !memcmp(&retained, &after_chain, sizeof(retained)));
  assert(bk_clip_state(p, &saved));
  for (unsigned i = 0; i < 10; i++) {
    assert(bk_clip_request(p, 0, error));
    assert(bk_clip_state(p, &state) && !memcmp(&saved, &state, sizeof(state)));
  }
  assert(bk_clip_request(p, 1, error));
  assert(bk_clip_state(p, &state) && state.requested == 1 &&
         state.elapsed == 0);
  assert(bk_clip_state(p, &saved));
  assert(!bk_clip_request_mode(p, 0, (BkClipRequestMode)2, error));
  assert(bk_clip_state(p, &state) && !memcmp(&saved, &state, sizeof(state)));
  assert(!bk_clip_request(p, 128, error));
  assert(!bk_clip_request(p, 2, error));
  bk_clip_player_destroy(p);
  bk_clip_set_destroy(set);
  /* Authored phase is explicit. Same-slot requests retain its stored rate;
   * forced selection instead recomputes rate from the clip definition. */
  memcpy(data, copy, sizeof(data));
  word(data + 512 + 0x164, 1);
  number(clip + 0x44, 2);
  number(clip + 0x60, 15);
  number(clip + 0x5c, .75f);
  set = bk_clip_set_decode(data, sizeof(data), error);
  assert(set);
  p = bk_clip_player_create_authored(set, error);
  assert(p && bk_clip_request(p, 0, error));
  assert(bk_clip_advance(p, 1.f / 60, &out, error));
  assert(!out.blend && fabsf(out.from - 15.75f) < .00001f);
  assert(bk_clip_select(p, 0, 1, error));
  assert(bk_clip_state(p, &state) && state.rate != .75f && state.elapsed == 0);
  bk_clip_player_destroy(p);
  bk_clip_set_destroy(set);
  const unsigned seed_offsets[] = {
      0x140, 0x148, 0x164,        0x168,        0x16c,        0x170,
      0x184, 0x188, 0x190 + 0x44, 0x190 + 0x5c, 0x190 + 0x60, 0x190 + 0x64};
  for (unsigned i = 0; i < sizeof(seed_offsets) / sizeof(*seed_offsets); i++) {
    memcpy(data, copy, sizeof(data));
    word(data + 512 + seed_offsets[i], 0xffffffff);
    set = bk_clip_set_decode(data, sizeof(data), error);
    assert(set && !bk_clip_player_create_authored(set, error));
    p = bk_clip_player_create(set, error);
    assert(p && bk_clip_select(p, 0, 1, error));
    bk_clip_player_destroy(p);
    bk_clip_set_destroy(set);
  }
  /* Costume files may save a currently inactive slot. Original callers load
   * those bytes and immediately select an active clip; keep the old source
   * as the transition's origin without exposing an invalid running player. */
  memcpy(data, copy, sizeof(data));
  word(data + 512 + 0x140, 3);
  word(data + 512 + 0x148, 3);
  number(clip + 3 * 156 + 0x60, 17.5f);
  word(clip + 3 * 156 + 0x64, 7);
  set = bk_clip_set_decode(data, sizeof(data), error);
  assert(set);
  assert(!bk_clip_player_create_authored(set, error));
  for (int instant = 0; instant < 2; instant++) {
    p = bk_clip_player_create_authored_start(set, 0, instant, error);
    assert(p);
    assert(bk_clip_loops(p, 3, &loops) && loops == 7);
    assert(bk_clip_loops(p, 0, &loops) && loops == 0);
    assert(bk_clip_state(p, &state) && state.slot == 0 &&
           state.requested == 0 && state.blend_from == 17.5f &&
           state.elapsed == 0);
    assert(bk_clip_advance(p, 0, &out, error));
    assert(out.blend && out.from == 17.5f);
    bk_clip_player_destroy(p);
  }
  assert(!bk_clip_player_create_authored_start(set, 3, 1, error));
  assert(!bk_clip_player_create_authored_start(set, 128, 1, error));
  assert(!bk_clip_player_create_authored_start(set, 0, 2, error));
  bk_clip_set_destroy(set);
  word(data + 512 + 0x148,
       0); /* Request0 is now a no-op: inactive3 must fail. */
  set = bk_clip_set_decode(data, sizeof(data), error);
  assert(set);
  assert(!bk_clip_player_create_authored_start(set, 0, 0, error));
  p = bk_clip_player_create_authored_start(set, 0, 1, error);
  assert(p);
  bk_clip_player_destroy(p);
  bk_clip_set_destroy(set);
  number(clip + 3 * 156 + 0x60, NAN);
  set = bk_clip_set_decode(data, sizeof(data), error);
  assert(set);
  assert(!bk_clip_player_create_authored_start(set, 0, 1, error));
  bk_clip_set_destroy(set);
  memcpy(data, copy, sizeof(data));
  /* Background door chains to an empty resting slot. Explicit selection
   * ignores it, but a different request resets its authored counters. */
  word(clip + 0x70, 1);
  word(clip + 0x74, 1);
  number(second + 0x44, 7);
  number(second + 0x5c, -INFINITY);
  set = bk_clip_set_decode(data, sizeof(data), error);
  assert(set);
  p = bk_clip_player_create_loaded(set, error);
  assert(p && bk_clip_state(p, &saved));
  for (int instant = 0; instant < 2; ++instant) {
    assert(bk_clip_select(p, 1, instant, error));
    assert(bk_clip_state(p, &state) && !memcmp(&saved, &state, sizeof(state)));
  }
  assert(bk_clip_request(p, 1, error));
  assert(bk_clip_state(p, &state) && state.slot == 1 && state.rate == 0 &&
         state.elapsed == 0);
  assert(bk_clip_advance(p, .1f, &out, error));
  assert(bk_clip_request(p, 0, error));
  assert(bk_clip_advance(p, 0, &out, error));
  assert(bk_clip_advance(p, 1, &out, error));
  assert(bk_clip_state(p, &state) && state.slot == 0 && state.requested == 0);
  bk_clip_player_destroy(p);
  bk_clip_set_destroy(set);
  /* Original x87 unordered comparison clamps a saved -Inf static rate. */
  word(data + 512 + 0x140, 1);
  word(data + 512 + 0x148, 0);
  word(data + 512 + 0x164, 1);
  word(second, 1);
  set = bk_clip_set_decode(data, sizeof(data), error);
  assert(set);
  p = bk_clip_player_create_loaded(set, error);
  assert(p && bk_clip_state(p, &saved) && !isfinite(saved.rate));
  assert(bk_clip_advance(p, .1f, &out, error) && out.from == 0);
  assert(bk_clip_state(p, &state) && state.rate == -INFINITY &&
         state.source == 0 && state.loops == 1);
  assert(bk_clip_request(p, 1, error));
  assert(bk_clip_advance(p, .1f, &out, error));
  bk_clip_player_destroy(p);
  bk_clip_set_destroy(set);
  /* A nonzero static pose has zero duration but remains selectable. */
  memcpy(data, copy, sizeof(data));
  word(clip + 0x50, 0);
  number(clip + 0x54, 30);
  number(clip + 0x58, 30);
  set = bk_clip_set_decode(data, sizeof(data), error);
  assert(set);
  p = bk_clip_player_create(set, error);
  assert(p && bk_clip_select(p, 0, 1, error));
  assert(bk_clip_state(p, &state) && state.rate == 0 && state.source == 30);
  bk_clip_player_destroy(p);
  bk_clip_set_destroy(set);
  memcpy(data, copy, sizeof(data));
  /* Reject unsupported/truncated layouts, unsafe names and broken chain refs.
   */
  for (unsigned size = 0; size < sizeof(data); size++)
    assert(!bk_clip_set_decode(data, size, error));
  const unsigned offsets[] = {0,
                              255,
                              512 + 0x190,
                              512 + 0x190 + 0x50,
                              512 + 0x190 + 0x54,
                              512 + 0x190 + 0x7c,
                              512 + 0x190 + 0x80};
  for (unsigned i = 0; i < sizeof(offsets) / sizeof(*offsets); i++) {
    memcpy(data, copy, sizeof(data));
    word(data + offsets[i], 0xffffffff);
    assert(!bk_clip_set_decode(data, sizeof(data), error));
  }
  memcpy(data, copy, sizeof(data));
  word(clip + 0x70, 1);
  word(clip + 0x74, 128);
  assert(!bk_clip_set_decode(data, sizeof(data), error));
  uint32_t random = 0x271004;
  for (unsigned n = 0; n < 6000; n++) {
    memcpy(data, copy, sizeof(data));
    for (unsigned j = 0; j < 1 + n % 9; j++) {
      random ^= random << 13;
      random ^= random >> 17;
      random ^= random << 5;
      data[random % sizeof(data)] ^= (unsigned char)(random >> 24);
    }
    set = bk_clip_set_decode(data, sizeof(data), error);
    if (!set)
      continue;
    p = bk_clip_player_create(set, error);
    assert(p);
    if (bk_clip_select(p, 0, 1, error)) {
      for (unsigned i = 0; i < 4; i++)
        bk_clip_advance(p, .5f, &out, error);
    }
    bk_clip_player_destroy(p);
    p = bk_clip_player_create_authored_start(set, 0, 1, error);
    if (p) {
      bk_clip_advance(p, .5f, &out, error);
      bk_clip_player_destroy(p);
    }
    bk_clip_set_destroy(set);
  }
  puts("PASS: clip init/end, transactional failure, immutable data, malformed "
       "layouts, 6000 mutations");
  return 0;
}
