#ifndef BK_GAME_RAIN_H
#define BK_GAME_RAIN_H
#include <stdint.h>
#define BK_RAIN_SPRITES 16
/* The rain-specific sprite lifecycle uses effects0 and phases1->2->3.
 * Pixel positions and RNG are updated at render dispatch (51a190/4fac10),
 * not by the background simulation timestep. */
typedef struct {
  uint32_t present;
  uint8_t phase[BK_RAIN_SPRITES];
  float x[BK_RAIN_SPRITES], y[BK_RAIN_SPRITES];
} BkRainState;
typedef struct {
  uint32_t slot;
  float x, y;
} BkRainSprite;
typedef struct {
  uint32_t count;
  BkRainSprite sprites[BK_RAIN_SPRITES];
} BkRainDraw;
/* Clear/create the16 rain slots only for enabled group4,area0..6.
 * Resource ownership stays in scene; this function performs no I/O. */
int bk_rain_initialize(BkRainState *, unsigned group, unsigned area,
                       int enabled);
/* Resolution scale is original721ad0 (window width/1280). Width/height of
 * each sprite remain256x512 native pixels. First rand chooses Y modulo
 * trunc(1280*scale); second chooses X modulo trunc(960*scale), minus100.
 * Disabled/absent sprites consume no randomness. Failure is atomic. */
int bk_rain_draw(BkRainState *, uint32_t *shared_random, int enabled,
                 float resolution_scale, BkRainDraw *);
#endif
