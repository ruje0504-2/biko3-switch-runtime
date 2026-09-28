#ifndef BK_SCENE_FACE_ASSETS_H
#define BK_SCENE_FACE_ASSETS_H
#include "model/morph_pose.h"
#include "resource/face_config.h"
#include "resource/store.h"
#include "world/face_controller.h"
typedef struct BkFaceAssets BkFaceAssets;
/* CPU face binding from FAM + source XAN/OBJM + optional VIX resources.
 * Owns keys, mutable target vertices and independent per-slot selections.
 * Resources and target model need only live during construction. FAM row0
 * does not override the caller's target. File reads are pack scoped.
 * Missing/empty optional VIX leaves all vertices selected; corrupt reads
 * fail. Native primary builder only supplies three slots per group.
 * Eye material/bitmap fields are retained, not yet animated or uploaded. */
BkFaceAssets *
bk_face_assets_create(BkResourceStore *resources, const char *config_pack,
                      const char *config_name, const char *model_pack,
                      const BkModel *target, const char *target_filename,
                      char error[256]);
/* Ending4f2dae binds four eye slots and three mouth slots. Shares actual
 * MORP ownership/initialization; ordinary4f2971 retains its three-slot limit.
 */
BkFaceAssets *
bk_face_assets_create_ending(BkResourceStore *resources,
                             const char *config_pack, const char *config_name,
                             const char *model_pack, const BkModel *target,
                             const char *target_filename, char error[256]);
void bk_face_assets_destroy(BkFaceAssets *face);
const BkFaceConfig *bk_face_assets_config(const BkFaceAssets *face);
/* Returns NULL for a submesh with no face binding. */
const BkMorphMesh *bk_face_assets_mesh(const BkFaceAssets *face,
                                       uint32_t submesh);
int bk_face_assets_apply(BkFaceAssets *face, const BkFaceCommands *commands,
                         char error[256]);
/* 0x4f2971/0x4f2dae controller setup/warm-up after mesh assembly: range0..9,
 * transition .5s, expression0, blink(timestamp1000), mouth100 then mouth0.
 * clock_ms supplies separate request/blink/first-mouth/second-mouth reads.
 * No eye texture initializer, audio backend or GPU side effects. */
int bk_face_assets_initialize(BkFaceAssets *face, BkFaceState *state,
                              const uint32_t clock_ms[4],
                              uint32_t *random_state, char error[256]);
/* Request expression, mouth, blink (0x4fc36d order). External controller
 * counts must match bindings. Vertices/controller/shared RNG commit together.
 * Explicit clock reads keep original caller timestamp and clock distinct.
 * Initialization/warm-up remains an explicit separate caller operation. */
int bk_face_assets_step(BkFaceAssets *face, BkFaceState *state,
                        int32_t expression, float mouth_level,
                        uint32_t timestamp_ms, uint32_t request_clock_ms,
                        uint32_t mouth_clock_ms, uint32_t blink_clock_ms,
                        uint32_t *random_state, char error[256]);
#endif
