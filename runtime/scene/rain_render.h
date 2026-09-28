#ifndef BK_SCENE_RAIN_RENDER_H
#define BK_SCENE_RAIN_RENDER_H
#include "game/rain.h"
#include "render/renderer.h"
#include "resource/store.h"
typedef struct BkRainRender BkRainRender;
/* Owns rain.tga texture/quad and native rain state; borrows renderer.
 * Mount bk3_20. Unsupported profiles produce a valid empty rain instance. */
BkRainRender *bk_rain_render_create(BkRenderer *, BkResourceStore *,
                                    unsigned group, unsigned area, int enabled,
                                    char error[256]);
void bk_rain_render_destroy(BkRainRender *);
/* Immediately before overlay rendering, consume the shared RNG in native
 * order. Pixel extent is the game's viewport, not a projection/letterbox
 * canvas. Each call is one original render dispatch, independent of dt.
 * Failure preserves state, RNG and the prior complete draw snapshot. */
int bk_rain_render_prepare(BkRainRender *, uint32_t *shared_random, int enabled,
                           unsigned pixel_width, unsigned pixel_height,
                           char error[256]);
/* Requires a successful prepare; active GPU frame, viewport already set.
 * No light/fog descriptor: this is the original transformed sprite path. */
int bk_rain_render_draw(BkRainRender *, char error[256]);
const BkRainState *bk_rain_render_state(const BkRainRender *);
const BkRainDraw *bk_rain_render_snapshot(const BkRainRender *);
#endif
