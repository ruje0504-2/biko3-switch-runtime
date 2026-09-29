#ifndef BK_MODEL_PLAYBACK_H
#define BK_MODEL_PLAYBACK_H
#include "model/animation.h"
#include "model/clip.h"

/* CPU model instance: immutable model/XAN are borrowed; decoded animation,
 * timeline and double-buffered world poses are owned. No actor AI or GPU. */
typedef struct BkModelPlayback BkModelPlayback;
typedef struct {
  BkClipSample pose;
  float source; /* Actual old-descriptor source, not the blend-from endpoint. */
} BkPlaybackEffects;
typedef struct {
  uint32_t frame;
  float local[16];
} BkModelLocalEdit;
BkModelPlayback *bk_model_playback_create(const BkModel *model,
                                          const BkClipSet *clips, int authored,
                                          char error[256]);
BkModelPlayback *bk_model_playback_create_loaded(const BkModel *,
                                                 const BkClipSet *,
                                                 char error[256]);
/* Authored load+initial actor request/instant selection before validating
 * the resulting playback. Does not sample or publish animation. */
BkModelPlayback *bk_model_playback_create_started(const BkModel *model,
                                                  const BkClipSet *clips,
                                                  unsigned slot, int instant,
                                                  char error[256]);
void bk_model_playback_destroy(BkModelPlayback *playback);
int bk_model_playback_request_active(BkModelPlayback *, unsigned slot,
                                     char error[256]);
/*4021a1 clock + ANIM only. Returns the plain source sample for caller-owned
 * MORP/material/effect services. Those are not implemented by this object.
 * Atomic timeline/local commit; same plain-time cache as seconds updates. */
int bk_model_playback_advance_frame(BkModelPlayback *,
                                    const BkModelRootTransform *,
                                    BkClipSample *sample, char error[256]);
int bk_model_playback_select(BkModelPlayback *playback, unsigned slot,
                             int instant, char error[256]);
int bk_model_playback_request(BkModelPlayback *playback, unsigned slot,
                              char error[256]);
int bk_model_playback_request_mode(BkModelPlayback *, unsigned slot,
                                   BkClipRequestMode, char error[256]);
/* Timeline edits only; no local/world publication or plain-time invalidation.
 */
int bk_model_playback_edit_clips(BkModelPlayback *, const BkClipEdit *, size_t,
                                 char error[256]);
int bk_model_playback_reset_sources(BkModelPlayback *, const unsigned *, size_t,
                                    char error[256]);
int bk_model_playback_set_clock(BkModelPlayback *, unsigned slot,
                                 float elapsed, float source, char error[256]);
/* Plain402e18/4e18ad/4a9019 clock plus actual ANIM submission, retaining the
 * existing plain-time cache. No MORP/material service or world publication
 * is implied. Timeline/locals and effect sample commit together. */
int bk_model_playback_advance_plain(BkModelPlayback *, float seconds,
                                     BkClipPlainMode,
                                     const BkModelRootTransform *,
                                     BkPlaybackEffects *, char error[256]);
int bk_model_playback_link(const BkModelPlayback *, unsigned slot,
                           int32_t *chain, int32_t *next);
/* Advance XAN and publish the composed pose atomically. Invalid input or
 * failed pose evaluation changes neither timeline nor published matrices.
 * Caller owns update order: consumers requiring the previous rendered pose
 * read it BEFORE advance; half-speed belongs to caller. Original ANIM tracks
 * loop individually, independently of a XAN clip's once/loop mode. The group
 * suppresses repeated plain times, including initial zero; blend submissions
 * do not invalidate the cached plain time. A skipped plain sample retains
 * the last submitted locals, while a new root still recomposes the hierarchy.
 */
int bk_model_playback_advance(BkModelPlayback *playback, float seconds,
                              const BkModelRootTransform *root,
                              char error[256]);
/* Atomic request + advance: a failed pose/step must also roll back the new
 * request. requested_slot=-1 keeps the existing request. */
int bk_model_playback_step(BkModelPlayback *playback, int requested_slot,
                           float seconds, const BkModelRootTransform *root,
                           char error[256]);
int bk_model_playback_step_mode(BkModelPlayback *playback, int requested_slot,
                                BkClipRequestMode mode, float seconds,
                                const BkModelRootTransform *root,
                                char error[256]);
/*4026fe clock/ANIM with explicit data for caller-owned MATA/MORP services.
 * Automatic chains keep the old descriptor's final source for this call.
 * On failure both playback and output remain unchanged. */
int bk_model_playback_step_effects(BkModelPlayback *, int requested_slot,
                                   BkClipRequestMode, float seconds,
                                   const BkModelRootTransform *,
                                   BkPlaybackEffects *, char error[256]);
/* Atomic request + placement without entering the scheduler (0x4026fe's
 * hidden-root early return). Unlike a zero-duration step, this preserves
 * first-step/blend bookkeeping and the last submitted local pose. */
int bk_model_playback_hold_mode(BkModelPlayback *playback, int requested_slot,
                                BkClipRequestMode mode,
                                const BkModelRootTransform *root,
                                char error[256]);
/* Recompose the last submitted pose (or asset base before first advance)
 * at a new root without consuming time or changing the clip state. NULL
 * restores asset roots. Failure leaves both timeline and pose unchanged. */
int bk_model_playback_place(BkModelPlayback *playback,
                            const BkModelRootTransform *root, char error[256]);
/* Initial published pose is the asset's base pose, just as before original
 * animation submission. Pointers remain valid only until next advance/place. */
const float *bk_model_playback_frame(const BkModelPlayback *playback,
                                     uint32_t frame);
/* Last submitted local pose, visible immediately after successful step/place.
 * Selecting/requesting a clip alone does not change it. */
const float *bk_model_playback_local(const BkModelPlayback *playback,
                                     uint32_t frame);
int bk_model_playback_state(const BkModelPlayback *playback,
                            BkClipState *state);
int bk_model_playback_timing(const BkModelPlayback *playback, unsigned slot,
                             BkClipTiming *timing);
int bk_model_playback_prediction(const BkModelPlayback *, unsigned slot,
                                 BkClipPrediction *);
int bk_model_playback_loops(const BkModelPlayback *, unsigned, int32_t *);
/* Ordered non-root local edits and hierarchy composition commit atomically;
 * timeline/plain-time cache are unchanged. Edits persist through placement,
 * holds and skipped ANIM submissions. A new ANIM submission overwrites its
 * tracked nodes only. Instance getters are invalidated by successful edits. */
int bk_model_playback_edit_locals(BkModelPlayback *playback,
                                  const BkModelLocalEdit *edits, size_t count,
                                  char error[256]);
#endif
