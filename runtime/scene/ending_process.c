#include "scene/ending_process.h"
#include <string.h>

void bk_ending_process_initialize(BkEndingProcess *p) {
  if (!p) return;
  memset(p, 0, sizeof(*p));
  p->selected_control = bk_ending_selected_control_initial();
  p->selected_action = bk_ending_selected_action_initial();
  p->selected_cycle = bk_ending_selected_cycle_initial();
  p->gallery_control = bk_ending_gallery_control_initial();
  p->gallery_normal = bk_ending_gallery_normal_initial();
  p->gallery_secondary = bk_ending_gallery_secondary_initial();
  p->gallery_selected = bk_ending_gallery_selected_initial();
  p->gallery_auxiliary = bk_ending_gallery_auxiliary_initial();
  p->gallery_effect = bk_ending_gallery_effect_initial();
  p->gallery_camera = bk_ending_gallery_camera_initial();
}
