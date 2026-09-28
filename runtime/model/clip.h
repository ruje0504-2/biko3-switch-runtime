#ifndef BK_MODEL_CLIP_H
#define BK_MODEL_CLIP_H
#include <stddef.h>
#include <stdint.h>
/* Verified XAN layout: two model names and 128 156-byte clips. Original
 * pointers are never used. SRT sampling and actor ownership remain separate. */
#define BK_CLIP_SLOTS 128

typedef struct BkClipSet BkClipSet;
typedef struct BkClipPlayer BkClipPlayer;
typedef enum {
  BK_CLIP_REQUEST_TEN_TICKS,
  BK_CLIP_REQUEST_CONFIGURED
} BkClipRequestMode;
typedef struct {
  int32_t active, loop, loop_start, duration, chain, next, chain_after;
  float start, end, blend_ticks;
} BkClipDefinition;
typedef struct {
  int32_t slot, requested, ended, looped, blend_done, loops;
  float elapsed, source, rate, blend_elapsed, blend_from, blend_to;
} BkClipState;
typedef struct {
  int32_t blend;
  float from, to, weight;
} BkClipSample;
typedef struct {
  float start, end, source;
} BkClipTiming;
enum { BK_CLIP_EDIT_CHAIN = 1, BK_CLIP_EDIT_NEXT = 2, BK_CLIP_EDIT_SOURCE = 4 };
typedef struct {
  unsigned slot, fields;
  int32_t chain, next;
  float source;
} BkClipEdit;
/* Instance-only original direct writes to descriptor+70/+74 and clock+60.
 * Ordered batch, atomic on invalid input. Does not change elapsed/rate/loops,
 * requested/active slot, blend state, or shared resource definitions.
 * Does not sample or publish an actor. A later normal step consumes edits.
 * All final enabled links must reference valid descriptors; empty targets
 * retain4025b9's no-op behavior. */
int bk_clip_edit(BkClipPlayer *, const BkClipEdit *, size_t count,
                 char error[256]);
int bk_clip_link(const BkClipPlayer *, unsigned slot, int32_t *chain,
                 int32_t *next);

BkClipSet *bk_clip_set_decode(const void *bytes, size_t size, char error[256]);
void bk_clip_set_destroy(BkClipSet *set);
const char *bk_clip_model_name(const BkClipSet *set);
const BkClipDefinition *bk_clip_definition(const BkClipSet *set, unsigned slot);
/* Player borrows the immutable set, which must outlive it. No clip is selected
 * on creation. The first transition starts from source tick zero. */
BkClipPlayer *bk_clip_player_create(const BkClipSet *set, char error[256]);
/* Original XAN loader preserves authored playback scalars. NPCs can request
 * their already-selected slot without resetting its progress. This explicit
 * variant validates that seed; the fresh constructor above still ignores it.
 * No model pointers, effects or savegame state are restored. */
BkClipPlayer *bk_clip_player_create_authored(const BkClipSet *set,
                                             char error[256]);
/* Scene loader: additionally accepts well-formed zero-range descriptors.
 * Their saved counters are real state (h96_00), not discarded. Repeating
 * the same request is a no-op; a different request to a zero-duration
 * descriptor writes rate0, as in401be3. Entirely
 * static XANs (h98_00/h98_30) are valid decoded sets. A selected static
 * zero-duration descriptor may retain a saved -Inf rate (m03_04_door slot1).
 * The original negative-rate unordered comparison clamps its sample to the
 * end tick, including 0*-Inf during loop reset. Other saved clocks remain
 * validated; NaN/+Inf saved rates are rejected. */
BkClipPlayer *bk_clip_player_create_loaded(const BkClipSet *set,
                                           char error[256]);
/* Original load followed immediately by an actor's initial selection:
 * instant=1 forces 401d24; 0 makes a 401b0a request. A stored inactive current
 * slot is permitted ONLY when the call actually replaces it. Its source is
 * still validated and preserved as blend_from. Same-request no-ops require
 * a fully valid authored seed, exactly like create_authored. */
BkClipPlayer *bk_clip_player_create_authored_start(const BkClipSet *set,
                                                   unsigned slot, int instant,
                                                   char error[256]);
void bk_clip_player_destroy(BkClipPlayer *player);
/* Copy playback scalars between players borrowing the exact same set. Used
 * to commit a timeline only after dependent pose evaluation succeeds. */
int bk_clip_player_copy(BkClipPlayer *destination, const BkClipPlayer *source);
/* instant=1: 0x401d24's tiny transition; instant=0: 0x401f71's configured
 * transition. A loaded player's well-formed empty slot is a no-op; other
 * empty/out-of-range slots fail without modifying the player. */
int bk_clip_select(BkClipPlayer *player, unsigned slot, int instant,
                   char error[256]);
/* 0x401b0a: same requested slot is a no-op, even after an automatic chain.
 * A different request resets playback and installs a 10-tick transition. */
int bk_clip_request(BkClipPlayer *player, unsigned slot, char error[256]);
/*40168c: configured transition only when ACTIVE differs; empty descriptors
 * do nothing. Used by the original fixed-call animation path. */
int bk_clip_request_active(BkClipPlayer *, unsigned slot, char error[256]);
/* 0x4018c8 preserves the target slot's current transition duration. Both
 * policies compare requested (not active); identical requests are no-ops. */
int bk_clip_request_mode(BkClipPlayer *player, unsigned slot,
                         BkClipRequestMode mode, char error[256]);
/* Original 0x4026fe seconds*60 scheduler; camera half-speed belongs to caller.
 * Preserves first-step suppression, latched flags and old-clip submission on
 * an automatic chain. Finite nonnegative seconds required. Invalid/overflowing
 * steps leave both state and sample unchanged. No model/effect calls here. */
int bk_clip_advance(BkClipPlayer *player, float seconds, BkClipSample *sample,
                    char error[256]);
/*4021a1 clock only: increment signed call counter(+6c), advance one tick
 * when it reaches interval(+68), always submit plain old-slot source. No
 * blend/first-step suppression/sign clamp; automatic chain clears flags
 * without the seconds scheduler's later ended=1. Media/SRT dispatch and
 * hidden/disabled guards belong to the caller. Failure is atomic. */
int bk_clip_advance_frame(BkClipPlayer *, BkClipSample *, char error[256]);
int bk_clip_frame_clock(const BkClipPlayer *, unsigned slot, int32_t *interval,
                        int32_t *counter);
int bk_clip_state(const BkClipPlayer *player, BkClipState *state);
/* Read any slot without selecting it. Retains inactive authored timelines;
 * callers validate values actually used by their branch. No clock changes. */
int bk_clip_timing(const BkClipPlayer *player, unsigned slot,
                   BkClipTiming *timing);
/* Native per-slot +64 completion/loop counter; the requested slot may
 * differ from the active slot after chaining. Read without selecting. */
int bk_clip_loops(const BkClipPlayer *, unsigned slot, int32_t *loops);
#endif
