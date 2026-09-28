#ifndef BK_MODEL_H
#define BK_MODEL_H
#include <stddef.h>
#include <stdint.h>
#define BK_MODEL_NONE UINT32_MAX
typedef enum {
  BK_MODEL_INVALID = -1,
  BK_MODEL_UNSUPPORTED = 0,
  BK_MODEL_OK = 1
} BkModelResult;
typedef struct {
  char tag[5];
  uint32_t offset, size;
} BkModelChunk;
/* File order, preserved exactly. The beta slot is not interpreted as a weight.
 * Only uv[0] is used by the current static material path. */
typedef struct {
  float position[3], beta, normal[3], uv[4][2];
} BkModelVertex;
typedef struct {
  char name[65];
  uint32_t id, first_submesh, submesh_count;
} BkModelMesh;
typedef struct {
  char name[65];
  uint32_t id, mesh_index, vertex_count, index_count;
  uint32_t material_id, texture_ids[4], texture_count;
  uint32_t material_index, texture_indices[4];
  uint32_t source_header_offset;
  BkModelVertex *vertices;
  uint16_t *indices;
} BkModelSubmesh;
typedef struct {
  char name[65];
  uint32_t id;
  float diffuse[4], ambient[4], specular[4], emissive[4];
  float power, unknown;
} BkModelMaterial;
typedef struct {
  char name[65], filename[65];
  uint32_t id;
} BkModelTexture;
typedef struct {
  char name[65];
  uint32_t id, parent_id, mesh_id, parent_index, mesh_index;
  float
      local[16]; /* Original row-major row-vector matrix; no axis conversion. */
} BkModelFrame;
typedef struct {
  uint8_t *source; /* Owned copy: opaque/unknown chunks remain available. */
  size_t source_size;
  BkModelChunk *chunks;
  uint32_t chunk_count;
  BkModelMesh *meshes;
  uint32_t mesh_count;
  BkModelSubmesh *submeshes;
  uint32_t submesh_count;
  BkModelMaterial *materials;
  uint32_t material_count;
  BkModelTexture *textures;
  uint32_t texture_count;
  BkModelFrame *frames;
  uint32_t frame_count;
  uint64_t vertex_count, triangle_count;
} BkModel;
/* out is NULL on failure. Caller can release input immediately after decode.
 * OBJM static CPU data only: animation/effects are retained, not executed.
 * DirectX text is classified separately and returns UNSUPPORTED. */
BkModelResult bk_model_decode(const uint8_t *data, size_t size, BkModel **out,
                              char error[256]);
void bk_model_destroy(BkModel *model);
const BkModelChunk *bk_model_chunk(const BkModel *model, const char tag[4]);
/* Original frame lookup removes the prefix through the first ASCII space
 * and compares case-sensitively. This binding API requires a unique match;
 * missing/ambiguous names fail without modifying index. */
int bk_model_find_frame(const BkModel *model, const char *name, uint32_t *index,
                        char error[256]);
/*425904 first depth-first match under a validated model root, in insertion
 * order. Empty names are meaningful; missing names succeed with MODEL_NONE.
 * Unlike find_frame, duplicate names do not reject a native binding. */
int bk_model_find_frame_first(const BkModel *, uint32_t root, const char *name,
                              uint32_t *index, char error[256]);
/* Native mesh registry names: parent[_ordinal]@UPPERCASE_MODEL_BASENAME.
 * Exported child names are not used. Lookup is case-sensitive and rejects
 * ambiguous names rather than borrowing an unrelated global object. */
int bk_model_submesh_name(const BkModel *model, uint32_t index,
                          const char *filename, char out[256], char error[256]);
int bk_model_find_submesh(const BkModel *model, const char *filename,
                          const char *name, uint32_t *index, char error[256]);
/* Output is frame_count * 16 floats in the original row-vector convention:
 * world = local * parent_world. Handles parents stored after their children. */
int bk_model_world_matrices(const BkModel *model, float *output,
                            size_t float_count, char error[256]);
/* Optional local pose array in original frame order. NULL uses base pose.
 * local and output must not overlap. */
int bk_model_pose_world_matrices(const BkModel *model, const float *local,
                                 float *output, size_t float_count,
                                 char error[256]);
/* Attach one model root beneath an external world matrix. Compose at each
 * hierarchy level (not a postmultiply of the already composed whole pose).
 * root_parent=NULL is the ordinary independent-root traversal. Like the
 * ordinary matrix helper, output is scratch and may change on failure. */
int bk_model_pose_world_matrices_under(const BkModel *, const float *local,
                                       uint32_t root,
                                       const float root_parent[16],
                                       float *output, size_t float_count,
                                       char error[256]);
#endif
