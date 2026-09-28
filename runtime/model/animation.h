#ifndef BK_MODEL_ANIMATION_H
#define BK_MODEL_ANIMATION_H
#include "model/model.h"
/* ANIM adapter for the verified 220-byte S/R/T key layout. Missing channels
 * use original preprocessing; non-unit quaternions are retained unchanged.
 * Curved translation and legacy layouts are unsupported.
 * Missing ANIM or a valid72-byte zero-track chunk has zero SRT tracks:
 * its XAN timeline may
 * still advance independently. A present malformed ANIM remains an error.
 * The immutable model is borrowed and must outlive this object. */
typedef struct BkModelAnimation BkModelAnimation;
typedef struct {
  float from, to, weight;
  int blend, loop;
} BkModelPoseSample;
typedef struct {
  uint32_t frame;
  float world[16];
} BkModelRootTransform;
BkModelAnimation *bk_model_animation_create(const BkModel *model,
                                            char error[256]);
void bk_model_animation_destroy(BkModelAnimation *animation);
uint32_t bk_model_animation_track_count(const BkModelAnimation *animation);
float bk_model_animation_duration(const BkModelAnimation *animation);
/* Absolute source ticks, not seconds. loop follows the original per-track
 * integer remainder after (strictly) exceeding its last key. Output is world
 * matrices, frame_count*16 floats; unanimated nodes retain their base local
 * matrices. Failure leaves output unchanged. No original gameplay camera
 * dispatch, actor anchor, transition or clip scheduler is implied. */
int bk_model_animation_sample(const BkModelAnimation *animation, float ticks,
                              int loop, float *world, size_t float_count,
                              char error[256]);
/* Original 0x408927 transition: sample two source poses, slerp rotation,
 * smoothstep translation (with original float stores), lerp scale. Finite
 * weight is clamped to [0,1]. The same transactional output contract applies.
 */
int bk_model_animation_blend(const BkModelAnimation *animation, float from,
                             float to, float weight, int loop, float *world,
                             size_t float_count, char error[256]);
/* Evaluate SRT with an optional external root world transform, as used by
 * original actor/camera frame setters under an identity world parent. The
 * selected frame must be a root without an ANIM track. Its matrix replaces
 * the base root before composing descendants; post-multiplying already
 * composed world matrices would lose the original float rounding/order.
 * Finite constant-W roots are preserved, including inverse-parent rounding
 * from 42407a. Projective/zero-W roots are rejected; do not normalize W.
 * NULL root keeps the asset root. Failure leaves output unchanged. */
int bk_model_animation_pose(const BkModelAnimation *animation,
                            const BkModelPoseSample *sample,
                            const BkModelRootTransform *root, float *world,
                            size_t float_count, char error[256]);
/* Compose asset locals without sampling ANIM, using the same validated root
 * placement. Used before the first animation submission/traversal. */
int bk_model_animation_base_pose(const BkModelAnimation *animation,
                                 const BkModelRootTransform *root, float *world,
                                 size_t float_count, char error[256]);
/* Evaluate world and optionally local matrices in one atomic operation.
 * sample=NULL selects asset base locals. Output arrays must not overlap.
 * Locals include the supplied external root and precede hierarchy traversal;
 * gameplay uses them for native local head angles before world publication. */
int bk_model_animation_matrices(const BkModelAnimation *animation,
                                const BkModelPoseSample *sample,
                                const BkModelRootTransform *root, float *world,
                                float *local, size_t float_count,
                                char error[256]);
/* Incremental native submission: begin with previous locals, replace ONLY
 * tracked frames when sample!=NULL. Unanimated roots follow explicit root
 * placement as above; all other untracked edits persist. NULL sample holds
 * all tracked locals too. Inputs may alias an output; outputs may not overlap.
 * Failure leaves outputs unchanged. */
int bk_model_animation_update(const BkModelAnimation *animation,
                              const BkModelPoseSample *sample,
                              const BkModelRootTransform *root,
                              const float *previous_local, float *world,
                              float *local, size_t float_count,
                              char error[256]);
#endif
