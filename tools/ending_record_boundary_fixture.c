/* Test-only access to the actual loaded selected scene. No record or unlock
 * words are injected. The selected-stage request and initial drag pose are
 * explicit boundaries; complete story navigation is not claimed by this test. */
#include "../runtime/scene/ending_normal_session.c"
int record_probe_point(BkScene *scene, BkInput *in, char e[256]) {
  EndingNormalScene *s=bk_scene_custom_context(scene);int32_t point[2];
  if(!s || !s->selected_assets || !bk_ending_ui_project_target(&s->ui_pick,4,point,e)) return 0;
  *in=(BkInput){.pointer_active=1,.pointer_x=s->viewport.x+(float)point[0],
      .pointer_y=s->viewport.y+(float)point[1]};return 1;
}
int record_probe_drag_begin(BkScene *scene, char e[256]) {
  EndingNormalScene *s=bk_scene_custom_context(scene);
  if(!s || !s->selected_assets || s->state->auxiliary.gate!=1) return 0;
  if(!bk_actor_pose_select(scene_primary(s),17,1,e)) return 0;
  s->state->auxiliary.gate=3;s->state->ui_controller.auxiliary.mode=4;
  s->state->ui_controller.auxiliary.reset_c=0;s->selected_plain_scheduled=0;
  s->selected_action->counter=0;s->selected_action->previous_clock=0;
  s->selected_action->previous_progress=0;
  s->state->retained.auxiliary.word_6ea348=0;
  return 1;
}
