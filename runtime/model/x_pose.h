#ifndef BK_MODEL_X_POSE_H
#define BK_MODEL_X_POSE_H
#include "model/model.h"
/* Pose-only reader for the game's text X camera assets. Decodes Frame and
 * SRT AnimationKey (0/1/2), keeping authored handedness and key times.
 * Mesh/Material/Header bodies are bounded opaque data, NOT drawable geometry.
 * General bk_model_decode intentionally still rejects text X: callers must
 * opt into this camera-only contract. Unknown pose templates, matrix keys,
 * duplicate names/channels and malformed/truncated structure fail explicitly.
 * Output owns original text followed by a private ANIM representation used by
 * the existing sampler. IDs are stable ordinals; root0 is an identity anchor.
 * Destroy with bk_model_destroy. No GPU, file IO or source-file modifications.
 */
BkModelResult bk_model_x_pose_decode(const void *, size_t, BkModel **,
                                     char error[256]);
#endif
