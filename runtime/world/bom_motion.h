#ifndef BK_WORLD_BOM_MOTION_H
#define BK_WORLD_BOM_MOTION_H
#include "world/node_reference.h"
#include <stdint.h>
/* The three manual controllers have INDEPENDENT process-retained state;
 * zero once on boot, never implicitly reset at binding/actor changes. */
typedef struct {
  float offset[2], angles[2];
} BkBomManual;
typedef enum {
  BK_BOM_MANUAL_FIRST,
  BK_BOM_MANUAL_SECOND,
  BK_BOM_MANUAL_DIRECT
} BkBomManualKind;
/*49aa50/49ad16/49afde. A missing node/reference is the original no-op.
 * Exact clamp -> 2D normalization -> radius, even for a tiny nonzero input.
 * flip==0 adds pi to both stored angles. Position -> reference Y rotation ->
 * local X rotation, using sin(stored angle) as the axis-angle radians.
 * No publication, animation, topology or GPU mutation. Failure is atomic. */
int bk_bom_manual_step(BkBomManual *, BkBomManualKind, BkNodeReference *,
                       const float reference[16], int32_t dx, int32_t dy,
                       float radius, float degrees, int32_t flip,
                       char error[256]);
typedef struct {
  uint32_t fresh, direction;
  float amplitude, travel, bound, start;
} BkBomOscillator;
/*49b55f /49bde5 arithmetic for one validated axis; odd axes use slower Y
 * constants. Completion resets oscillator but DOES NOT write *offset. */
int bk_bom_oscillator_step(BkBomOscillator *, unsigned axis, float distance,
                           uint32_t milliseconds, float *offset, int *done,
                           char error[256]);
typedef struct {
  BkBomOscillator axes[4];
  float offset[2][2];
  uint32_t complete[2][2];
} BkBomReturn;
/* Two independent retained states: one49b28f and one49b900. CRT fresh=1. */
void bk_bom_return_init(BkBomReturn *);
/*49b28f: reset clears only completion latches, aligns the node and returns1;
 * it does not clear retained offsets or oscillator internals. */
int bk_bom_return_single(BkBomReturn *, BkNodeReference *,
                         const float reference[16], const float scene_world[16],
                         float degrees, int32_t flip, uint32_t milliseconds,
                         int32_t reset, int *done, char error[256]);
/*49b900 supports the actual0/1 last-index calls (1 or2 bindings). Caller passes
 * nodes in original order; nullable entries return done0 after prior effects.
 * The original sixth reset argument is unused. Late error keeps earlier nodes.
 * No guessed behaviour for native indices past the four-axis globals. */
int bk_bom_return_multiple(BkBomReturn *, BkNodeReference *const nodes[2],
                           const float *const references[2], unsigned count,
                           const float scene_world[16], float degrees,
                           int32_t flip, uint32_t milliseconds, int *done,
                           char error[256]);
#endif
