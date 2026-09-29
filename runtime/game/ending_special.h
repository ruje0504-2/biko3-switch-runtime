#ifndef BK_GAME_ENDING_SPECIAL_H
#define BK_GAME_ENDING_SPECIAL_H
#include "core/camera.h"
#include "game/draw_dispatch.h"
#include "game/ending_frame.h"
#define BK_ENDING_SPECIAL_CAMERAS 108
/* 4cc582 copies one complete authored table into mutable71944c. */
int bk_ending_special_cameras(float out[BK_ENDING_SPECIAL_CAMERAS][4],
                              unsigned group);
/*4e755e..4e75aa/4a9f10: scale from the fitted content width, then truncate
 * native (0,360,800,600) bounds. Add the content origin only at the platform
 * boundary. A viewport that cannot fit is rejected without changing out. */
int bk_ending_special_viewport(BkViewport *out, const BkViewport *content,
                               char error[256]);
const char *bk_ending_special_hidden_name(unsigned group, unsigned variant,
                                          unsigned slot);
typedef struct {
  const BkEndingFrameState *frame;
  const int32_t *action_variant; /*721e04, separate from721b3d*/
  const uint8_t *camera_variant, *restore_hidden;
  const int32_t *camera_index, *offset_mode; /*721edc/7220f4*/
  const float (*cameras)[4];
  const float *target; /* live cached721ef4 world XYZ */
  const uint32_t *primary_root, *auxiliary_root;
} BkEndingSpecialBindings;
typedef enum {
  BK_ENDING_SPECIAL_END,
  BK_ENDING_SPECIAL_VIEWPORT, /* argument0=stored main,1=stored special */
  BK_ENDING_SPECIAL_CLEAR,    /* exact42a08b argument2 */
  BK_ENDING_SPECIAL_BEGIN
} BkEndingSpecialRenderEvent;
typedef struct {
  void *context;
  int (*draw)(void *, const BkDrawDispatch *, char error[256]);
  /*422ea4 followed by423564, relative to the actual global root. */
  int (*camera_read)(void *, float position[3], float forward[3], float up[3],
                     char error[256]);
  int (*camera_position)(void *, const float[3], char error[256]);
  int (*camera_orientation)(void *, const float forward[3], const float up[3],
                            char error[256]);
  int (*camera_aim)(void *, const float target[3], char error[256]);
  int (*camera_publish)(void *, char error[256]);
  int (*render_event)(void *, BkEndingSpecialRenderEvent, unsigned argument,
                      char error[256]);
  /* First DFS name match; empty names are meaningful. Missing -> node0. */
  int (*find_node)(void *, uint32_t root, const char *, uint32_t *node,
                   char error[256]);
  int (*hide_node)(void *, uint32_t node, uint32_t hidden, char error[256]);
  /* Actual4a7d10: nonzero mode writes alpha0, zero writes supplied alpha.
   * Missing named material is a successful no-op, not missing service. */
  int (*material)(void *, const char *, int mode, float alpha, char error[256]);
} BkEndingSpecialOps;
/* Complete4d9898 ordered orchestration. First pass consumes original
 * descriptor by value; second changes mode/slots20..51 in caller's descriptor.
 * State is read live at native branch points. Duplicate/missing nodes and
 * group0's intentionally unrestored extra nodes retain original behavior.
 * All used services are mandatory. Failure stops at its prefix; no invented
 * recovery draw, automatic rollback or success for an unimplemented backend.
 * This CPU scheduler does not own GPU snapshots, viewport or render passes. */
int bk_ending_special_draw(const BkEndingSpecialBindings *, BkDrawDispatch *,
                           const BkEndingSpecialOps *, char error[256]);
#endif
