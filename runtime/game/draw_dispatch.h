#ifndef BK_GAME_DRAW_DISPATCH_H
#define BK_GAME_DRAW_DISPATCH_H
#include "game/lighting_pass.h"
/* Original root slots retain address labels until the corresponding loader/
 * event ownership is recovered. They are opaque portable tokens, not pointers.
 * 0 means the source object is absent. */
typedef struct {
  uint32_t flow;
  int32_t group, event_state;
  uint32_t root_733e28, root_bef77c, root_bef780;
  uint32_t root_721b28, root_721b2c, root_721b34;
} BkDrawDispatchInput;
typedef struct {
  uint32_t objects[52];
  int32_t mode, shadow_mode;
  uint32_t event_stage;
} BkDrawDispatch;
/* 51c736 selects the descriptor passed by value to4a4701. event_stage means
 * caller must execute4e1473 then4d9733 with this mutable descriptor; nonzero
 * return from4d9733 suppresses the regular lighting pass. No callback is
 * silently treated as implemented. Flow56 requires both gallery roots.
 * No world publication, resource loading or render side effects. Atomic. */
int bk_draw_dispatch_select(const BkDrawDispatchInput *, BkDrawDispatch *);
/* Transfer an already finalized descriptor into the lighting input. Retain
 * the caller's scene root and current light registry. Do not call this until
 * any required event stage has completed and returned0. */
int bk_draw_dispatch_lighting(const BkDrawDispatch *, BkLightingPassInput *);
typedef struct {
  int32_t mode; /* 721ec4: 0 conditional scene,1 always scene,2 audio only. */
  uint32_t buffer_present; /* 722454 exists; status query result is separate. */
} BkEventDrawState;
typedef struct {
  void *context;
  int (*prepare_event)(void *);                 /*4e1473*/
  int (*event_scene)(void *, BkDrawDispatch *); /*4d9898*/
  int (*buffer_status)(void *, int32_t *query_result, uint32_t *flags);
  int (*duck_audio)(void *, uint32_t enabled);    /*4e01e4*/
  int (*regular)(void *, const BkDrawDispatch *); /*4a4701*/
} BkDrawDispatchOps;
/*51c736+4d9733 ordered service dispatch. Event mode1 draws the special scene
 * BEFORE querying the sound buffer; mode0 queries first. Buffer is playing iff
 * its query returns0 and flag bit0 is set. Callback edits to the descriptor
 * persist. Required missing callbacks fail; no unimplemented event/audio
 * fallback. Callback failure stops subsequent work; already performed work is
 * not rolled back. Native's always-zero AL is not an application success code.
 */
int bk_draw_dispatch_run(const BkDrawDispatchInput *, const BkEventDrawState *,
                         const BkDrawDispatchOps *);
#endif
