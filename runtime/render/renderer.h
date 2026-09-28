#ifndef BK3_RENDER_VK_H
#define BK3_RENDER_VK_H
#include "core/camera.h"
#include "core/fog.h"
#include "core/lighting.h"
#include "resource/assets.h"
typedef struct BkRenderer BkRenderer;
typedef struct BkTexture BkTexture;
typedef struct BkGpuMesh BkGpuMesh;
typedef struct BkLightSet BkLightSet;
typedef struct BkSkinPalette BkSkinPalette;
typedef struct BkVertexTransfer BkVertexTransfer;
typedef struct {
  double fence_seconds, acquire_seconds, present_seconds;
  uint64_t draws, mesh_updates, uploaded_bytes;
  uint64_t skin_dispatches, skin_vertices;
  uint64_t live_allocations, peak_allocations, live_bytes, peak_bytes;
} BkRenderStats;
/* Cumulative CPU wait durations and work counters, sampled without GPU waits.
 * Fence/acquire/present are separate; these are not GPU execution timers. */
BkRenderStats bk_renderer_stats(const BkRenderer *);
typedef enum { BK_WRAP_CLAMP, BK_WRAP_REPEAT } BkTextureWrap;
typedef enum { BK_CULL_NONE, BK_CULL_COUNTER_CLOCKWISE } BkCull;
typedef enum {
  BK_BLEND_UI_ALPHA, /* Retains opaque destination alpha for the title/UI. */
  BK_BLEND_ALPHA,
  BK_BLEND_ADDITIVE,
  BK_BLEND_INVERSE_COLOR,
  BK_BLEND_OPAQUE,
  BK_BLEND_COUNT
} BkBlend;
typedef struct {
  BkBlend blend;
  int depth_write; /* Scene policy, not inferred from a material alpha value. */
  BkCull cull;
} BkDrawState;
typedef struct {
  float x, y, z, u, v, r, g, b, a;
} BkVertex;
typedef struct {
  BkVertex base;
  float normal[3], ambient[3], emissive[3];
  float specular[3],
      power; /* Original material power >= .01 enables highlights. */
} BkLitVertex;
typedef struct {
  uint32_t bone, reset;
  float weight, position[3], normal[3];
} BkGpuSkinWeight;
/* GPU deformation uses precomputed ordered weights. reset starts a new
 * accumulator; source attributes remain unchanged for unweighted vertices.
 * Scene supplies original weight policy, renderer contains no game rules.
 * Palette and mesh changes occur outside frames; pending compute executes
 * before the next render pass. Mesh retains palette until destruction. */
BkSkinPalette *bk_skin_palette_create(BkRenderer *, unsigned matrix_count,
                                      char error[256]);
int bk_skin_palette_update(BkRenderer *, BkSkinPalette *, const float *world,
                           unsigned matrix_count, char error[256]);
void bk_skin_palette_destroy(BkRenderer *, BkSkinPalette *);
int bk_lit_mesh_skin(BkRenderer *, BkGpuMesh *, BkSkinPalette *,
                     const uint32_t *offsets, const BkGpuSkinWeight *,
                     unsigned weight_count, char error[256]);
/* Diagnostic readback, outside frames and after a completed begin/end.
 * Waits/invalidate GPU writes. Never part of the production frame loop. */
int bk_lit_mesh_readback(BkRenderer *, BkGpuMesh *, BkLitVertex *, unsigned,
                         char error[256]);
typedef struct {
  uint32_t source, target;
} BkVertexPair;
/* Ordered GPU position/normal copies. Retains both lit meshes and owns indices.
 * Distinct buffers compact duplicate targets to their last write and run in
 * parallel; an aliased source/target retains sequential read-after-write.
 * No other vertex attributes change. Construction/update outside frames.
 * World transforms positions with native near-unit-W division and normals
 * with its3x3, without normalization. Caller guarantees defined finite results
 * and nonzero homogeneous W for selected source positions, as for GPU skin.
 */
BkVertexTransfer *bk_vertex_transfer_create(BkRenderer *, BkGpuMesh *source,
                                            BkGpuMesh *target,
                                            const BkVertexPair *,
                                            unsigned count, char error[256]);
/* Snapshot world and request a fresh target skin pass for the next begin,
 * even if its palette is unchanged. Rigid target geometry remains persistent.
 */
int bk_vertex_transfer_world(BkVertexTransfer *, const float world[16],
                             char error[256]);
/* Release outside frames. A surviving skinned target is refreshed next begin.
 */
void bk_vertex_transfer_destroy(BkVertexTransfer *);
/* Active frame: suspend color/depth pass, execute transfers in order, resume
 * with LOAD and held viewport. Each call is a new callback, including repeats.
 * No CPU wait/readback/allocation. Empty batch is a no-op; invalid batch fails
 * before recording commands. Pending skin ran at begin before these copies. */
int bk_vertex_transfers_apply(BkRenderer *, BkVertexTransfer *const *,
                              unsigned count, char error[256]);
/* Light set and lit vertex format, separate from title/UI shaders.
 * Point/spot/global ambient + diffuse + emissive, normalized transformed
 * normals. Camera-relative Gouraud specular is added after texturing, before
 * blending. Rigid camera assumed. Constant positive world W is retained through
 * clip transform and accounted for in eye-space lighting, not divided early.
 * Fog is disabled until explicitly configured below. */
BkLightSet *bk_light_set_create(BkRenderer *r, const BkLighting *lighting,
                                char error[256]);
/* Outside active frames, replace light values and preserve viewer/fog. Waits
 * for earlier GPU use; use distinct sets for different passes in one frame. */
int bk_light_set_update(BkRenderer *, BkLightSet *, const BkLighting *,
                        char error[256]);
/* Set world-space camera position (initially zero), outside an active frame.
 * Waits for prior use before writing the existing mapped uniform buffer.
 * Invalid input preserves it; no allocation or descriptor replacement. */
int bk_light_set_view(BkRenderer *r, BkLightSet *lights,
                      const float camera_position[3], char error[256]);
/* Outside active frames; original vertex/range or pixel eye-depth fog.
 * View supplies eye-depth Z; set_view supplies the camera for range fog.
 * Color affects RGB after texture/specular; alpha is unchanged. */
int bk_light_set_fog(BkRenderer *, BkLightSet *, const BkFog *,
                     const float view[16], char error[256]);
void bk_light_set_destroy(BkRenderer *r, BkLightSet *lights);
BkGpuMesh *bk_lit_mesh_create(BkRenderer *r, const BkLitVertex *vertices,
                              unsigned vertex_count, const uint16_t *indices,
                              unsigned index_count, char error[256]);
/* Replace vertex contents, retaining topology/count. Outside active frames:
 * waits for this renderer's previous submission before writing mapped memory.
 * Copies data and flushes noncoherent memory; no per-frame allocation.
 * Invalid inputs leave the mesh intact. GPU/flush errors propagate to caller.
 */
int bk_lit_mesh_update(BkRenderer *r, BkGpuMesh *mesh,
                       const BkLitVertex *vertices, unsigned vertex_count,
                       char error[256]);
int bk_renderer_draw_lit_mesh(BkRenderer *r, BkTexture *texture,
                              BkGpuMesh *mesh, BkLightSet *lights,
                              const float matrix[16], const float world[16],
                              BkDrawState state, char error[256]);
/* Switch: loaderless Mesa NVK + NWindow. Host: offscreen Vulkan for tests. */
BkRenderer *bk_renderer_create(unsigned width, unsigned height, FILE *log,
                               char error[256]);
void bk_renderer_destroy(BkRenderer *r);
BkTexture *bk_texture_create(BkRenderer *r, const BkImage *image,
                             char error[256]);
BkTexture *bk_texture_create_sampled(BkRenderer *r, const BkImage *image,
                                     BkTextureWrap wrap, char error[256]);
/* Outside active frames, copy a same-size RGBA image into retained staging.
 * Waits for preceding GPU use. Next begin records all pending transfers
 * before drawing; repeated updates before begin keep only the latest image.
 * Preserves image/descriptor/sampler/sort identity and allocates staging only
 * on the first update. CPU image may be freed immediately. Invalid input
 * preserves the pending/current image; GPU/flush failures are fatal. */
int bk_texture_update(BkRenderer *, BkTexture *, const BkImage *,
                      char error[256]);
void bk_texture_destroy(BkRenderer *r, BkTexture *texture);
/* Monotonic texture creation identity in this renderer; zero means absent.
 * Replaces process-address keys for deterministic ordinary-queue ordering.
 * This is not a recovered Windows allocator address. */
uint32_t bk_texture_sort_key(const BkTexture *texture);
int bk_texture_owned_by(const BkTexture *, const BkRenderer *);
/* Upload copies CPU data; mesh/texture belong to the creating renderer and
 * must be destroyed outside an active frame, before that renderer. Matrices
 * are column-major for GLSL. The ordinary mesh path consumes caller-supplied
 * shaded color; use the separate lit mesh path for supported point/spot
 * lighting. */
BkGpuMesh *bk_mesh_create(BkRenderer *r, const BkVertex *vertices,
                          unsigned vertex_count, const uint16_t *indices,
                          unsigned index_count, char error[256]);
int bk_mesh_update(BkRenderer *r, BkGpuMesh *mesh, const BkVertex *vertices,
                   unsigned vertex_count, char error[256]);
void bk_mesh_destroy(BkRenderer *r, BkGpuMesh *mesh);
int bk_renderer_draw_mesh(BkRenderer *r, BkTexture *texture, BkGpuMesh *mesh,
                          const float matrix[16], BkDrawState state,
                          char error[256]);
int bk_renderer_begin(BkRenderer *r, char error[256]);
void bk_renderer_extent(const BkRenderer *r, unsigned *width, unsigned *height);
/* Active frame only. Viewport and scissor change together. NULL restores the
 * full target. begin also resets both, so state cannot leak between scenes. */
int bk_renderer_viewport(BkRenderer *r, const BkViewport *viewport,
                         char error[256]);
/* Active frame: clear only depth inside the current viewport/scissor.
 * Keep color, depth outside the viewport, queued geometry and all bindings.
 * depth must be finite in[0,1]. Records GPU commands only; no allocation,
 * readback, wait or presentation. Used between distinct camera passes. */
int bk_renderer_clear_depth(BkRenderer *, float depth, char error[256]);
int bk_renderer_draw(BkRenderer *r, BkTexture *texture,
                     const BkVertex *vertices, unsigned count,
                     const float matrix[16], char error[256]);
/* Synchronously read the current active frame BEFORE later HUD draws. Ends
 * the current render pass, submits/waits/copies RGBA, then resumes with both
 * color and depth preserved. Retains viewport/scissor and submitted vertices;
 * does not present, reacquire, clear, or advance the frame. Supports multiple
 * captures in one frame on host and Switch. Only requested captures should
 * call this blocking operation. Size must be exactly the full target RGBA.
 * Invalid arguments leave recording intact; GPU failure is fatal to renderer.
 */
int bk_renderer_capture(BkRenderer *, uint8_t *rgba, size_t size,
                        char error[256]);
int bk_renderer_end(BkRenderer *r, char error[256]);
int bk_renderer_readback(BkRenderer *r, uint8_t *rgba, size_t size,
                         char error[256]);
extern const float bk_identity[16];
#endif
