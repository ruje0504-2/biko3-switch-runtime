#ifndef BK_SCENE_FLOW_LOADING_RENDER_H
#define BK_SCENE_FLOW_LOADING_RENDER_H
#include "render/renderer.h"
#include "resource/store.h"
#include "scene/flow_loading.h"
typedef struct BkFlowLoadingRender BkFlowLoadingRender;
/* Persistent bk3_00 loading images; special te_01 is loaded only on request.
 * Create/prepare outside the active render frame. Frame snapshots retain the
 * opacity seen before the loader mutates live shared UI/flow state. */
BkFlowLoadingRender *bk_flow_loading_render_create(BkRenderer *,
                                                   BkResourceStore *,
                                                   uint8_t special,
                                                   char error[256]);
void bk_flow_loading_render_destroy(BkFlowLoadingRender *);
int bk_flow_loading_render_prepare(BkFlowLoadingRender *,
                                   const BkFlowLoadingFrame *, unsigned width,
                                   unsigned height, char error[256]);
int bk_flow_loading_render_draw(BkFlowLoadingRender *, char error[256]);
#endif
