/* Test-only read access to actual ending scenes. No record or unlock words
 * are injected here. The ordinary natural-input driver avoids the isolated
 * probe's selected-stage boundary; incoming story navigation remains explicit. */
#include "../runtime/scene/ending_normal_session.c"
#include "ending_record_natural_input.h"
#include "scene/ending_target.h"
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

static int natural_target(EndingNormalScene *s, BkInput *in, char e[256]) {
  BkClipState clip;
  BkNodeReference camera;
  if (!s->assets || !bk_actor_pose_state(scene_primary(s), &clip) ||
      !bk_actor_forest_anchor_reference(scene_forest(s), 1, &camera, e))
    return -1;
  BkEndingFrameState frame = s->state->frame;
  int32_t targets[39][2], kind, column;
  memcpy(targets, s->state->targets, sizeof(targets));
  BkEndingUiSprite ring = s->ui.sprites[50];
  BkEndingTargetBindings b = {&frame, &s->state->control,
      &s->state->auxiliary, &clip.slot,
      bk_ending_normal_assets_config(s->assets)->actions, targets,
      &kind, &column, &ring, camera.local, &s->ui_pick};
  int best = -1;
  for (unsigned y = 0; y < s->viewport.height; ++y)
    for (unsigned x = 0; x < s->viewport.width; ++x) {
      float point[2] = {(float)x, (float)y};
      int hit;
      if (!bk_ending_target_step(&b, point, &hit, e)) return -1;
      if (!hit) continue;
      int node = frame.camera_cached;
      int target = node == 1 ? 0 : node == 11 ? 1 : node == 12 ? 2 :
          node == 9 ? 3 : node == 10 ? 4 :
          (node == 26 || node == 27) ? 5 : node == 5 ? 6 : -1;
      int score = -1;
      if (node == 6 && s->state->auxiliary.progress > .39f) score = 100;
      else if (target >= 0)
        score = 2 - !!s->state->normal_processed[2 * target] -
            !!s->state->normal_processed[2 * target + 1];
      if (score > 0 && score > best) {
        best = score;
        *in = (BkInput){.pointer_active = 1,
            .pointer_x = s->viewport.x + (float)x,
            .pointer_y = s->viewport.y + (float)y};
      }
    }
  return best > 0;
}

static int natural_secondary_target(EndingNormalScene *s, BkInput *in,
                                      char e[256]) {
  if (!s->secondary_assets) return -1;
  uint32_t node = (uint32_t)s->state->retained.normal.word_719b40;
  const float *world = node ? bk_actor_forest_world(scene_forest(s), node) : NULL;
  if (!world) return -1;
  BkEndingFrameState frame = s->state->frame;
  int32_t kind, column, targets[39][2], alternate[2];
  memcpy(targets, s->state->targets, sizeof(targets));
  memcpy(alternate, s->state->alternate, sizeof(alternate));
  BkEndingUiSprite ring = s->ui.sprites[50];
  BkEndingSecondaryPickBindings b = {&frame, &kind, &column, targets,
      alternate, &ring, &s->ui_pick, world};
  BkEndingSecondaryMenuGeometry geometry = {s->viewport.width,
      s->viewport.height, (float)((double)s->viewport.width / 1280.),
      s->ui.sprites[51].rect[2]};
  for (unsigned y = 0; y < s->viewport.height; ++y)
    for (unsigned x = 0; x < s->viewport.width; ++x) {
      float point[2] = {(float)x, (float)y};
      int32_t hit, zone;
      if (!bk_ending_secondary_pick(&b, point, 0, &hit, e)) return -1;
      if (hit != 2) continue;
      if (!bk_ending_secondary_menu_zone(&geometry, alternate, &zone, e))
        return -1;
      /* An edge of the hit circle can be visible while its menu center is
       * outside the original allowed area. Use real camera input first. */
      if (zone == -1) continue;
      *in = (BkInput){.pointer_active = 1,
          .pointer_x = s->viewport.x + (float)x,
          .pointer_y = s->viewport.y + (float)y};
      return 1;
    }
  return 0;
}

int record_probe_early_input(BkScene *scene, BkRecordNaturalInput *d,
                             BkInput *in, char e[256]) {
  EndingNormalScene *s = bk_scene_custom_context(scene);
  if (!s || !d || !in || s->state->auxiliary.variant != 0)
    return fail(e, "natural-input probe supports ordinary routes only");
  const BkEndingState *st = s->state;
  unsigned frame = ++d->frames;
  if (st->frame.phase != d->phase || st->frame.state_721ee4 != d->secondary) {
    printf("record-early phase%d secondary%d frame%u\n", st->frame.phase,
        st->frame.state_721ee4, frame);
    d->phase = st->frame.phase;
    d->secondary = st->frame.state_721ee4;
    d->has_aim = -1;
  }
  if (st->frame.phase < 0 || st->frame.phase >= 32)
    return fail(e, "natural-input phase outside trace mask");
  d->phases |= 1u << (unsigned)st->frame.phase;
  if (frame == 1 || st->frame.state_721ee0 != d->gate ||
      st->normal_ready != d->ready || st->auxiliary.progress != d->progress) {
    printf("record-early frame%u gate%d ready%d progress%.6f target%d\n",
        frame, st->frame.state_721ee0, st->normal_ready,
        st->auxiliary.progress, st->frame.camera_cached);
    d->gate = st->frame.state_721ee0;
    d->ready = st->normal_ready;
    d->progress = st->auxiliary.progress;
  }
  *in = (BkInput){0};
  if (st->frame.phase == 2) {
    if (st->frame.state_721ee4 == 1) {
      if (frame % 30 == 1 || d->has_aim < 0)
        d->has_aim = natural_secondary_target(s, &d->aimed, e);
      if (d->has_aim < 0) return 0;
      *in = d->aimed;
      if (d->has_aim && frame % 30 == 1) in->held = BK_BUTTON_CONFIRM;
      if (!d->has_aim)
        *in = (BkInput){.held = BK_BUTTON_CAMERA_ADJUST, .look_x = -.2f};
    } else if (st->frame.state_721ee4 == 3) {
      unsigned choice = 0;
      while (choice < 3 && st->choices[choice] != 4) ++choice;
      if (choice == 3) return fail(e, "real secondary menu lacks choice4");
      *in = (BkInput){.pointer_active = 1,
          .pointer_x = s->viewport.x + (float)st->points[choice][0],
          .pointer_y = s->viewport.y + (float)st->points[choice][1]};
    }
  } else if (st->frame.phase == 7) {
    if (frame % 30 == 1) in->held = BK_BUTTON_CONFIRM;
  } else if (st->frame.phase == 1) {
    if (st->frame.state_721ee0 == 1) {
      d->held_frames = 0;
      if (frame % 30 == 1 || d->has_aim < 0)
        d->has_aim = natural_target(s, &d->aimed, e);
      if (d->has_aim < 0) return 0;
      *in = d->aimed;
      if (d->has_aim && frame % 30 == 1) in->held = BK_BUTTON_CONFIRM;
      if (!d->has_aim) {
        *in = (BkInput){.pointer_active = 1,
            .pointer_x = s->viewport.x + s->viewport.width * .5f,
            .pointer_y = s->viewport.y + s->viewport.height * .5f,
            .held = BK_BUTTON_CAMERA_ADJUST, .look_x = -.8f};
      }
    } else if (st->frame.state_721ee0 == 3 &&
               (st->normal_ready == 0 || st->normal_ready == 1 ||
                st->normal_ready == 5)) {
      ++d->held_frames;
      int t = st->normal_target;
      int done = t >= 0 && t < 7 && st->normal_processed[2 * t] &&
          st->normal_processed[2 * t + 1];
      if (!done && d->held_frames < 360) {
        in->held = BK_BUTTON_CONFIRM;
        float magnitude = d->held_frames % 120 < 60 ? 3 : 12;
        in->pointer_motion_x = d->held_frames % 8 < 4 ? magnitude : -magnitude;
      }
    }
  } else return fail(e, "unexpected phase before natural selected entry");
  in->pressed = in->held & ~d->previous_buttons;
  in->released = d->previous_buttons & ~in->held;
  d->previous_buttons = in->held;
  return 1;
}
