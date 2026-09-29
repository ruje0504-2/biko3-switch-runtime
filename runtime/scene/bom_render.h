#ifndef BK_SCENE_BOM_RENDER_H
#define BK_SCENE_BOM_RENDER_H
#include "scene/actor_render.h"
#include "scene/bom_assets.h"
#include "scene/bom_dual_assets.h"
typedef struct BkBomRender BkBomRender;
/* Bind actual owned VIX/maps to retained GPU meshes. Borrows BOM and both
 * actor renderers until destruction, installs mesh callbacks. Destroy before
 * actors/BOM. Does not change skinning mode or read any GPU vertices to CPU. */
BkBomRender *bk_bom_render_create(BkRenderer *, const BkBomAssets *,
                                  BkActorRender *const actors[2],
                                  char error[256]);
/*4a52bc three-actor ownership and the combined eight-slot registry. No
 * duplication of CPU maps/effects. Each present CPU actor requires its own
 * GPU owner; absent auxiliaries remain absent. Five actual third-ending
 * bindings share the same native callback grouping/disable semantics. */
BkBomRender *bk_bom_render_create_dual(BkRenderer *, const BkBomDualAssets *,
                                       BkActorRender *const actors[3],
                                       char error[256]);
void bk_bom_render_destroy(BkBomRender *);
/* After ALL present actor prepares, before begin: snapshot live cached source worlds
 * and raw disable flags. Only exactly1 disables, as4aaebb. Callbacks repeat in
 * original order per actual target draw, including alpha0; hidden nodes do
 * not reach them. Later actor mutation rejects the snapshot. No allocations. */
int bk_bom_render_prepare(BkBomRender *, const int32_t *disabled, size_t count,
                          char error[256]);
#endif
