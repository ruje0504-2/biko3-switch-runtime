#ifndef BK_SCENE_ENDING_SPECIAL_SCENE_H
#define BK_SCENE_ENDING_SPECIAL_SCENE_H
#include "game/ending_special.h"
#include "model/material_pose.h"
#include "world/actor_forest.h"
#include "world/menu_camera.h"
typedef struct {
  BkMaterialPose *pose;
  uint32_t index;
} BkEndingSpecialMaterial;
typedef struct {
  BkActorForest *forest;
  BkMenuCamera *camera;
  /* Ordered actual material registry. Names come from each bound pose; first
   * exact match wins. Borrowed until draw returns. No filename inference. */
  const BkEndingSpecialMaterial *materials;
  size_t material_count;
  void *render_context;
  /* Called immediately at each original stage. Must capture independent GPU
   * state (including current materials/visibility/camera) for each view, or
   * perform the real drawing. Missing services fail; no placeholder backend. */
  int (*draw)(void *, const BkDrawDispatch *, const BkMenuCamera *,
              char error[256]);
  int (*render_event)(void *, BkEndingSpecialRenderEvent, unsigned argument,
                      char error[256]);
} BkEndingSpecialScene;
/* Bind4d9898 to the actual forest, node setters, material values and camera
 * view prepass. Only camera.pose.world follows the rendered anchor; filter
 * position, matrix, presets, lens and controller fields stay independent.
 * Uses caller's live state/target/roots. No allocation or timeline advance.
 * Failure preserves the original ordered prefix; owner must abort the frame. */
int bk_ending_special_scene_draw(const BkEndingSpecialScene *,
                                 const BkEndingSpecialBindings *,
                                 BkDrawDispatch *, char error[256]);
#endif
