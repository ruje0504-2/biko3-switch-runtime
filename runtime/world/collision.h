#ifndef BK_WORLD_COLLISION_H
#define BK_WORLD_COLLISION_H
#include "model/model.h"
#define BK_COLLISION_ATR_SIZE 4293124u
typedef struct {
  char name[260];
  uint32_t vertex_count, index_count;
  float (*vertices)[3];
  uint32_t *indices;
  float (*normals)[3];
} BkCollisionMesh;
typedef struct BkCollision BkCollision;
/* 0x4b2330/4b1b02: ATR selects model frames/submeshes in file order.
 * Names are original runtime submesh@model_name (caller supplies original
 * model filename case). Only world translation is applied, as in native
 * static construction. Owns a copy; all inputs may be released on return.
 * Per-frame prop geometry is added separately after this static prefix. */
BkCollision *bk_collision_create(const BkModel *model, const float *world,
                                 size_t world_floats, const char *model_name,
                                 const void *atr, size_t atr_size,
                                 char error[256]);
void bk_collision_destroy(BkCollision *collision);
uint32_t bk_collision_count(const BkCollision *collision);
const BkCollisionMesh *bk_collision_mesh(const BkCollision *collision,
                                         uint32_t index);
typedef struct {
  int32_t active, kind;
  const BkModel *model;
  const float *world; /* Already published frame world cache. */
  size_t world_floats;
  const char *model_name; /* Runtime filename case used for mesh names. */
  float position[3];
} BkCollisionProp;
/* Original4b6fa3/4b1ede vertex transformation: recover rounded Euler yaw,
 * discard world translation, multiply world * (vertex-translation * yaw).
 * Add prop X/Z afterwards; prop Y is intentionally unused. No world divide.
 * This is not a conventional full world point transform. Atomic output. */
int bk_collision_prop_point(float out[3], const float point[3],
                            const float world[16], const float position[3]);
/*4b26b8, first16 props in original order. Accepted kinds0,1,10,13..19. Select
 * original named collision frame, copy every submesh in order. Replace the
 * previous dynamic suffix atomically; no animation/publication. Missing named
 * frames fail instead of reproducing the native null deref. Mesh pointers may
 * be invalidated; vertex data is owned independently. */
int bk_collision_begin_props(BkCollision *, const BkCollisionProp *,
                             size_t count, char error[256]);
/*4b2f41: remove dynamic suffix, preserving static prefix and its vertex data.
 */
void bk_collision_end_props(BkCollision *);
/* Native signed +118: static=-1, dynamic=prop KIND, not array index. */
int bk_collision_kind(const BkCollision *, uint32_t index, int8_t *kind);
/* 4b66eb: dynamic obstacles only, excluding same KIND and kind10. Tests
 * every edge of triangles with normal.Y < float(cos(1.047)); no Y gate.
 * Does not move the prop; caller supplies the original 8x lookahead. */
int bk_collision_prop_blocked(const BkCollision *, int32_t kind,
                              const float start[3], const float end[3],
                              int *blocked, char error[256]);
/* 0x4ae586, including its 0x522922 near-unit/near-zero normalization. */
int bk_collision_normal(float out[3], const float triangle[3][3]);
/* 0x4b2f8e: hit means any projected triangle, even when no candidate is
 * within the +/-15 height window. In that case native writes -99999.
 * Height is preserved on miss; invalid math preserves both outputs. */
int bk_collision_ground(const BkCollisionMesh *mesh, const float point[3],
                        int *hit, float *height, char error[256]);
#endif
