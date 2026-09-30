/* Test-only access to the actual loaded selected scene. No record or unlock
 * words are injected. Only the selected-stage request is an explicit
 * boundary; complete story navigation is not claimed by this test. */
#include "../runtime/scene/ending_normal_session.c"
int record_probe_point(BkScene *scene, BkInput *in, char e[256]) {
  EndingNormalScene *s=bk_scene_custom_context(scene);int32_t point[2];
  if(!s || !s->selected_assets || !bk_ending_ui_project_target(&s->ui_pick,4,point,e)) return 0;
  *in=(BkInput){.pointer_active=1,.pointer_x=s->viewport.x+(float)point[0],
      .pointer_y=s->viewport.y+(float)point[1]};return 1;
}
/* Search with scratch outputs, leaving the live frame/targets/UI untouched. */
int record_probe_alternate(BkScene *scene, BkInput *in, char e[256]) {
  EndingNormalScene *s=bk_scene_custom_context(scene);
  if(!s || !s->selected_assets) return 0;
  uint32_t node=(uint32_t)s->state->retained.normal.word_719b40;
  const float *world=node?bk_actor_forest_world(scene_forest(s),node):NULL;
  if(!world) return fail(e,"record probe: missing actual alternate anchor");
  BkEndingFrameState frame=s->state->frame;
  int32_t kind,column,targets[39][2],alternate[2];
  BkEndingUiSprite ring=s->ui.sprites[50];
  BkEndingSecondaryPickBindings b={&frame,&kind,&column,targets,alternate,
      &ring,&s->ui_pick,world};
  for(unsigned y=0;y<s->viewport.height;++y)
    for(unsigned x=0;x<s->viewport.width;++x) {
      int32_t picked=0;float pointer[2]={(float)x,(float)y};
      if(!bk_ending_secondary_pick(&b,pointer,4,&picked,e)) return 0;
      if(picked==2) {
        *in=(BkInput){.pointer_active=1,.pointer_x=s->viewport.x+(float)x,
            .pointer_y=s->viewport.y+(float)y};
        return 1;
      }
    }
  return fail(e,"record probe: alternate anchor has no visible hit");
}
int record_probe_active(BkScene *scene, int32_t *active, char e[256]) {
  return selected_active(bk_scene_custom_context(scene),active,e);
}
