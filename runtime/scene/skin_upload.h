#ifndef BK_SCENE_SKIN_UPLOAD_H
#define BK_SCENE_SKIN_UPLOAD_H
#include "model/skin.h"
#include "render/renderer.h"
/* Transpose native bone-major ENVL operations to per-vertex order, without
 * reordering contributions. Resolve beta thresholds/remaining weights once
 * on CPU: they depend only on immutable authored weights, not pose/MORP.
 * Renderer copies the plan. No per-frame CPU deformation remains. */
int bk_skin_upload(BkRenderer *, BkGpuMesh *, BkSkinPalette *,
                   const BkModelSkin *, unsigned entry, char error[256]);
#endif
