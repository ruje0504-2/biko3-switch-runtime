#ifndef BK_SCENE_AVI_TEXTURE_H
#define BK_SCENE_AVI_TEXTURE_H
#include "media/avi_clock.h"
#include "render/renderer.h"
typedef struct BkAviTexture BkAviTexture;
/* Owns AVI/decoder/clock/RGBA scratch/GPU texture. Input bytes may be freed
 * after creation. Clock is original signed process-elapsed milliseconds.
 * Portable device policy: opaque black until the first native frame request,
 * compact RGB555 DIB, original+44 copy/row order, zero tail; see avi_surface.
 * This selects native supported555 instead of emulating driver-dependent
 * palette/565 fallback. The choice is explicit, not Windows raster proof. */
BkAviTexture *bk_avi_texture_create(BkRenderer *, const void *bytes,
                                    size_t size, int32_t clock_ms,
                                    char error[256]);
void bk_avi_texture_destroy(BkAviTexture *);
/* Outside a GPU frame. Latch native request, decode/reconstruct, convert,
 * queue same-size texture update. Failure is fatal; do not retry a partial
 * clock request. unchanged returns success without uploading. */
int bk_avi_texture_step(BkAviTexture *, int32_t clock_ms,
                        int32_t restart_clock_ms, char error[256]);
BkTexture *bk_avi_texture_gpu(const BkAviTexture *);
/* Borrowed CPU copy of queued image, for inspection; black before request. */
const BkImage *bk_avi_texture_image(const BkAviTexture *);
uint32_t
bk_avi_texture_frame(const BkAviTexture *); /* UINT32_MAX before request */
#endif
