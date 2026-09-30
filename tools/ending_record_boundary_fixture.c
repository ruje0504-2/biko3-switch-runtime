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

int record_probe_inventory(BkScene *scene, const uint8_t inventory[5],
                            int check_cursor, char e[256]) {
  EndingNormalScene *s = bk_scene_custom_context(scene);
  if (!s || s->inventory != inventory)
    return fail(e, "inventory is not the application's live owner");
  if (check_cursor) {
    unsigned selected = inventory[0] == 1 ? 8 : 7;
    if (s->state->frame.phase != 3 || s->state->frame.camera_cached != 6 ||
        s->ui.sprites[selected].transform.fade.alpha < .99f ||
        s->ui.sprites[15 - selected].transform.fade.alpha > .01f)
      {
        if (e) snprintf(e, 256, "inventory cursor: phase%d target%d item%u alpha7/8=%g/%g",
            s->state->frame.phase, s->state->frame.camera_cached, inventory[0],
            s->ui.sprites[7].transform.fade.alpha, s->ui.sprites[8].transform.fade.alpha);
        return 0;
      }
  }
  return 1;
}

int record_probe_inventory_lifecycle(BkScene *scene, char e[256]) {
  EndingNormalScene *s = bk_scene_custom_context(scene);
  if (!s || !s->stopped || !s->flow.inventory)
    return fail(e, "inventory lifecycle requires a stopped application owner");
  BkEndingNormalFlow flow = s->flow;
  uint8_t *inventory = flow.inventory, initial[5];
  memcpy(initial, inventory, 5);
  uint8_t unlocked[5][8]; memcpy(unlocked, s->state->working, sizeof(unlocked));
  BkScene *old = NULL, *next = NULL;
  BkResourceStore *empty = bk_resources_create(e);
  int result = 0;
  if (!empty) return 0;
  BkSceneServices missing = s->services;
  missing.resources = empty;
  char rejected[256] = {0};
  /* Dispatch sets bytes1/2 before this real loader fails. Rollback must
   * restore those bytes without broad resets of the application inventory. */
  old = bk_ending_scene_create_gallery(&missing, s->group, 0, 0, s->records,
      unlocked, &flow, rejected);
  if (old || !rejected[0] || memcmp(inventory, initial, 5)) {
    fail(e, "failed gallery construction changed inventory"); goto done;
  }
  old = bk_ending_scene_create_gallery(&s->services, s->group, 0, 0, s->records,
      unlocked, &flow, e);
  if (!old) goto done;
  uint8_t entered[5]; memcpy(entered, initial, 5); entered[1] = entered[2] = 1;
  if (memcmp(inventory, entered, 5)) {
    fail(e, "gallery entry did not set only its two inventory bytes"); goto done;
  }
  if (!bk_ending_normal_scene_stop(old, e)) goto done;
  entered[1] = entered[2] = 0;
  if (memcmp(inventory, entered, 5)) {
    fail(e, "logical gallery stop did not clear only its two inventory bytes"); goto done;
  }
  /* Start another real owner before the old GPU snapshot is destroyed. */
  inventory[1] = entered[1] = 0xa5;
  inventory[2] = entered[2] = 0x5a;
  next = bk_ending_normal_scene_create_story(&s->services, s->group, 1,
      s->records, unlocked, &flow, e);
  if (!next || memcmp(inventory, entered, 5)) goto done;
  if (!bk_ending_normal_scene_stop(old, e)) goto done;
  bk_scene_destroy(old); old = NULL;
  if (memcmp(inventory, entered, 5)) {
    fail(e, "late gallery retirement cleared the new owner's inventory"); goto done;
  }
  if (!bk_ending_normal_scene_stop(next, e)) goto done;
  bk_scene_destroy(next); next = NULL;
  if (memcmp(inventory, entered, 5)) {
    fail(e, "story exit changed inventory"); goto done;
  }
  memcpy(inventory, initial, 5);
  printf("PASS inventory-lifecycle group%u failed1 entries2 stops3 late-retirement1\n", s->group);
  result = 1;
done:
  bk_scene_destroy(old); bk_scene_destroy(next); bk_resources_destroy(empty);
  return result;
}

static int third_target(EndingNormalScene *s, BkInput *in, char e[256]) {
  BkClipState clip;
  BkNodeReference camera;
  if (!s->tertiary_assets || !bk_actor_pose_state(scene_primary(s), &clip) ||
      !bk_actor_forest_anchor_reference(scene_forest(s), 1, &camera, e)) return -1;
  const int32_t *actions = bk_ending_tertiary_assets_config(s->tertiary_assets)->actions;
  int wanted = -1;
  if (clip.slot == 1) wanted = bk_ending_tertiary_initial_targets()[s->state->frame.group];
  else if (clip.slot == 4) {
    if (s->state->auxiliary.progress >= .39f) wanted = 6;
    else if (!s->state->retained.stage2.word_6afd08) wanted = actions[5];
    else if (!s->state->unavailable[0]) wanted = actions[10];
    else if (!s->state->unavailable[1]) wanted = actions[15];
  }
  if (wanted < 0) return 2; /* Animation is not ready for another target. */
  BkEndingFrameState f = s->state->frame;
  int32_t points[39][2], kind, column;
  memcpy(points, s->state->targets, sizeof(points));
  BkEndingUiSprite ring = s->ui.sprites[50];
  BkEndingTargetBindings b = {&f, &s->state->control, &s->state->auxiliary,
      &clip.slot, actions, points, &kind, &column, &ring, camera.local, &s->ui_pick};
  BkEndingSecondaryMenuGeometry geometry = {s->viewport.width, s->viewport.height,
      (float)((double)s->viewport.width / 1280.), s->ui.sprites[51].rect[2]};
  /* Revalidate an existing aim through the same real picker before doing
   * another exhaustive search. This never reuses stale hit output. */
  if (in->pointer_active) {
    float point[2] = {in->pointer_x - s->viewport.x, in->pointer_y - s->viewport.y};
    int hit; int32_t zone;
    if (!bk_ending_target_step(&b, point, &hit, e)) return -1;
    if (hit && f.camera_cached == wanted) {
      if (!bk_ending_secondary_menu_zone(&geometry, points[wanted], &zone, e)) return -1;
      if (zone != -1) return 1;
    }
  }
  float closest = INFINITY;
  int found = 0;
  for (unsigned y = 0; y < s->viewport.height; ++y)
    for (unsigned x = 0; x < s->viewport.width; ++x) {
      float p[2] = {(float)x, (float)y};
      int hit;
      int32_t zone;
      if (!bk_ending_target_step(&b, p, &hit, e)) return -1;
      if (!hit || f.camera_cached != wanted) continue;
      if (!bk_ending_secondary_menu_zone(&geometry, points[wanted], &zone, e)) return -1;
      if (zone == -1) continue;
      float dx = (float)x - points[wanted][0], dy = (float)y - points[wanted][1];
      float distance = dx * dx + dy * dy;
      if (distance >= closest) continue;
      closest = distance;
      *in = (BkInput){.pointer_active = 1, .pointer_x = s->viewport.x + (float)x,
          .pointer_y = s->viewport.y + (float)y};
      found = 1;
    }
  return found;
}

int record_probe_third_input(BkScene *scene, BkRecordNaturalInput *d,
                             BkInput *in, char e[256]) {
  EndingNormalScene *s = bk_scene_custom_context(scene);
  if (!s || !d || !in || s->state->frame.phase != 3)
    return fail(e, "third input expects the actual phase3 owner");
  const BkEndingState *st = s->state;
  BkClipState clip;
  if (!bk_actor_pose_state(scene_primary(s), &clip)) return 0;
  unsigned frame = ++d->frames;
  if (frame == 1 || st->stage3_state != d->gate || clip.slot != d->secondary ||
      st->auxiliary.progress != d->progress) {
    printf("record-third frame%u state%d clip%d progress%.6f item%u\n",
        frame, st->stage3_state, clip.slot, st->auxiliary.progress, s->inventory[0]);
    d->gate = st->stage3_state; d->secondary = clip.slot;
    d->progress = st->auxiliary.progress; d->has_aim = -1;
  }
  d->phases |= 1u << 3;
  *in = (BkInput){0};
  if (st->stage3_state == 1) {
    if (frame % 30 == 1 || d->has_aim < 0) d->has_aim = third_target(s, &d->aimed, e);
    if (d->has_aim < 0) return 0;
    if (d->has_aim == 1) {
      *in = d->aimed;
      if (frame % 30 == 1) in->held = BK_BUTTON_CONFIRM;
    } else if (!d->has_aim) {
      *in = (BkInput){.pointer_active = 1,
          .pointer_x = s->viewport.x + s->viewport.width * .5f,
          .pointer_y = s->viewport.y + s->viewport.height * .5f,
          .held = BK_BUTTON_CAMERA_ADJUST, .look_x = -.5f};
    }
  } else if (st->stage3_state == 3) {
    in->pointer_active = 1;
    in->pointer_x = s->viewport.x + (float)st->points[0][0];
    in->pointer_y = s->viewport.y + (float)st->points[0][1];
    if (st->retained.stage2.word_6a3c20 == 1 && !st->ui_controller.hints.movement_ready) {
      in->pointer_active = 0; in->held = BK_BUTTON_CONFIRM;
      in->pointer_motion_x = frame % 16 < 8 ? 2 : -2;
    }
  }
  in->pressed = in->held & ~d->previous_buttons;
  in->released = d->previous_buttons & ~in->held;
  d->previous_buttons = in->held;
  return 1;
}

static int auxiliary_target(EndingNormalScene *s, BkInput *in, char e[256]) {
  if (!s || !s->auxiliary_assets)
    return -1;
  uint32_t node = (uint32_t)s->state->retained.normal.word_719b40;
  const float *alternate = node ? bk_actor_forest_world(scene_forest(s), node) : NULL;
  BkEndingFrameState frame = s->state->frame;
  int32_t kind, column, targets[39][2], alternate_point[2];
  BkEndingUiSprite ring = s->ui.sprites[50];
  BkEndingSecondaryPickBindings bindings = {
      &frame, &kind, &column, targets, alternate_point, &ring, &s->ui_pick,
      alternate};
  BkEndingSecondaryMenuGeometry geometry = {
      s->viewport.width, s->viewport.height,
      (float)((double)s->viewport.width / 1280.0), s->ui.sprites[51].rect[2]};
  memcpy(targets, s->state->targets, sizeof(targets));
  memcpy(alternate_point, s->state->alternate, sizeof(alternate_point));

  /* State1's preferred target is the auxiliary camera slot4. Search through
   * the same picker and menu-zone services as the live controller. The
   * scratch frame prevents this observer from publishing a second target. */
  for (unsigned y = 0; y < s->viewport.height; ++y)
    for (unsigned x = 0; x < s->viewport.width; ++x) {
      float point[2] = {(float)x, (float)y};
      int32_t hit, zone;
      if (!bk_ending_secondary_pick(&bindings, point, 4, &hit, e))
        return -1;
      if (hit != 1 || frame.camera_cached != 4)
        continue;
      if (!bk_ending_secondary_menu_zone(&geometry, targets[4], &zone, e))
        return -1;
      if (zone < 0)
        continue;
      *in = (BkInput){
          .pointer_active = 1,
          .pointer_x = s->viewport.x + (float)x,
          .pointer_y = s->viewport.y + (float)y};
      return 1;
    }
  return 0;
}

int record_probe_auxiliary_input(BkScene *scene, BkRecordNaturalInput *d,
                                 BkInput *in, char e[256]) {
  EndingNormalScene *s = bk_scene_custom_context(scene);
  if (!s || !d || !in || s->state->frame.phase != 4)
    return fail(e, "auxiliary input expects the actual phase4 owner");
  ++d->frames;
  int state = s->state->control.state_721eec;
  d->phases |= 1u << 4;
  if (d->phase != 4 || d->gate != state) {
    d->phase = 4;
    d->gate = state;
    d->held_frames = 0;
    d->has_aim = -1;
  }

  *in = (BkInput){0};
  if (state == 1) {
    int found = auxiliary_target(s, in, e);
    if (found < 0)
      return 0;
    if (!found)
      *in = (BkInput){.held = BK_BUTTON_CAMERA_ADJUST, .look_x = -.5f};
    else
      in->held = BK_BUTTON_CONFIRM;
  } else if (state == 3) {
    in->pointer_active = 1;
    in->pointer_x = s->viewport.x + (float)s->state->points[0][0];
    in->pointer_y = s->viewport.y + (float)s->state->points[0][1];
    in->held = ++d->held_frames < 60 ? BK_BUTTON_CONFIRM : 0;
  }
  in->pressed = in->held & ~d->previous_buttons;
  in->released = d->previous_buttons & ~in->held;
  d->previous_buttons = in->held;
  return 1;
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
