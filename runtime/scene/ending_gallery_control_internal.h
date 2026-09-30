/* Included by ending_normal_session.c. These adapters borrow the active
 * loader; the five CPU controllers never own models, sounds or UI sprites. */
#include "game/ending_sound.h"

static int gallery_present(void *p, unsigned slot, int *out, char e[256]) {
  return bk_ending_audio_present(((EndingNormalScene *)p)->audio, slot, out)
      ? 1 : fail(e, "gallery audio slot outside actual owners");
}
static int gallery_status(void *p, unsigned slot, int *out, char e[256]) {
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_STATUS, .slot = slot};
  return selected_audio(p, &call, out, e);
}
static int gallery_expression(void *p, int32_t a, int32_t b, int32_t mode,
                               char e[256]) {
  EndingNormalScene *s = p;
  s->state->auxiliary.expression_a = a;
  s->state->auxiliary.expression_b = b;
  return mode >= 0 ? bk_eye_assets_select(scene_eyes(s), (unsigned)mode, e)
                  : fail(e, "negative gallery eye texture mode");
}
static int gallery_fov(void *p, float value, char e[256]) {
  if (!isfinite(value) || value <= 0 || value >= 3.14159265f)
    return fail(e, "invalid gallery field of view");
  ((EndingNormalScene *)p)->camera.fov = value;
  return 1;
}
static int gallery_target(void *p, unsigned node, uint32_t out[3], char e[256]) {
  EndingNormalScene *s = p;
  if (!out || (node != 0 && node != 5 && node != 13))
    return fail(e, "unknown gallery target node");
  uint32_t frame = scene_node(s, node);
  uint32_t id = bk_actor_forest_node(scene_forest(s), scene_registry(s, 0), frame);
  const float *world = bk_actor_forest_world(scene_forest(s), id);
  if (!world) return fail(e, "gallery target is not published");
  memcpy(out, world + 12, sizeof(uint32_t) * 3);
  return 1;
}
static int gallery_target5(void *p, uint32_t out[3], char e[256]) {
  return gallery_target(p, 5, out, e);
}
static int gallery_target0(void *p, uint32_t out[3], char e[256]) {
  return gallery_target(p, 0, out, e);
}
static int gallery_camera(void *p, BkEndingOpeningCamera kind, int32_t choice,
    const uint32_t words[3], uint32_t extra, uint32_t *result, char e[256]) {
  EndingNormalScene *s = p;
  const uint32_t tracks[] = {scene_registry(s, 3), scene_registry(s, 4)};
  int complete = 0, ok;
  if (!words || !result) return fail(e, "missing gallery camera arguments");
  if (kind == BK_ENDING_OPENING_TRACK) {
    uint32_t target = s->state->retained.normal.follow_target;
    if (!target || !bk_actor_forest_world(scene_forest(s), target))
      return fail(e, "gallery opening target is absent");
    BkEndingCameraOpeningInput input = {s, control_key};
    ok = bk_ending_camera_assets_opening(scene_cameras(s), scene_forest(s),
        tracks, &s->camera, target, s->active_seconds, &input, &complete, e);
  } else if (kind == BK_ENDING_OPENING_PRESET) {
    float offset[3];
    memcpy(offset, words, sizeof(offset));
    BkEndingCameraPresetGate gate = {
      (uint8_t)s->previous_flow, s->state->frame.phase, s->state->selected,
      s->state->frame.state_721ee0, s->state->frame.state_721ee4,
      s->state->control.state_721eec, s->state->auxiliary.gate, s->state->next_mode};
    ok = bk_ending_camera_assets_preset(scene_cameras(s), scene_forest(s),
        tracks, &s->camera, &s->camera_transitions, &s->presets,
        BK_ENDING_PRESET, (unsigned)choice, offset, &gate, 0x10,
        s->active_seconds, &complete, e);
  } else return fail(e, "unknown gallery camera service");
  (void)extra; /*4E0ECB does not read its sixth argument.*/
  if (ok) *result = (uint32_t)complete;
  return ok;
}
static int gallery_play(void *p, unsigned slot, int32_t flags, int32_t volume,
                         char e[256]) {
  int ignored;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_RESTART,
      .slot = slot, .flags = flags, .volume = volume};
  return selected_audio(p, &call, &ignored, e);
}
static int gallery_play_once(void *p, unsigned slot, int32_t volume, char e[256]) {
  return gallery_play(p, slot, 0, volume, e);
}
static int gallery_pause(void *p, unsigned slot, char e[256]) {
  int ignored;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_PAUSE, .slot = slot};
  return selected_audio(p, &call, &ignored, e);
}
static int gallery_voice_name(void *p, char name[32], char e[256]) {
  return bk_ending_sound_contact_voice(((EndingNormalScene *)p)->state->frame.group,
                                        0, 0, 0, name, e);
}
static int gallery_tertiary_voice(void *p, int32_t cue, unsigned slot,
                                   int32_t bank, int32_t select, char e[256]) {
  EndingNormalScene *s = p;
  (void)bank; /*479739's third argument is unused.*/
  if (slot >= 2) return fail(e, "gallery speech slot outside name owners");
  return bk_ending_sound_tertiary_voice(s->state->frame.group, cue, select,
      s->state->speech_names[slot], e) &&
      selected_load(p, slot, s->state->speech_names[slot], e);
}
static int gallery_cue(void *p, int32_t cue, int32_t bank, unsigned slot,
                        int32_t flags, char e[256]) {
  EndingNormalScene *s = p;
  int ignored;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_CUE, .cue = cue,
      .bank = bank, .slot = slot, .flags = flags, .volume = s->voice_volume};
  return selected_audio(p, &call, &ignored, e);
}
static int gallery_selected_voice(void *p, unsigned cue, unsigned slot,
                                   int32_t flags, char e[256]) {
  EndingNormalScene *s = p;
  if (cue > INT32_MAX) return fail(e, "gallery voice cue exceeds native range");
  int ignored;
  BkEndingAudioCall call = {.operation = BK_ENDING_AUDIO_VOICE, .cue = (int32_t)cue,
      .slot = slot, .flags = flags, .volume = s->voice_volume};
  return selected_audio(p, &call, &ignored, e);
}
static int gallery_secondary_voice(void *p, unsigned cue, unsigned slot,
                                    int32_t flags, char e[256]) {
  EndingNormalScene *s = p;
  if (cue > INT32_MAX) return fail(e, "gallery secondary cue exceeds native range");
  BkEndingSecondarySpeechOps ops = {s, selected_load, gallery_play};
  return bk_ending_secondary_speech(s->state->frame.group, (int32_t)cue, slot,
      (uint32_t)flags, s->state->speech_names, &s->voice_volume, &ops, e);
}
static int gallery_auxiliary_voice(void *p, unsigned cue, unsigned slot,
                                    int32_t flags, char e[256]) {
  EndingNormalScene *s = p;
  if (slot >= 2 || cue > 99) return fail(e, "gallery auxiliary voice out of range");
  snprintf(s->state->speech_names[slot], 32, "PH%u33%02u.wav",
             s->state->frame.group + 1, cue);
  return selected_load(p, slot, s->state->speech_names[slot], e) &&
      gallery_play(p, slot, flags, s->voice_volume, e);
}
static int gallery_active(void *p, int32_t *out, char e[256]) {
  BkClipState state;
  if (!out || !bk_actor_pose_state(scene_primary(p), &state))
    return fail(e, "gallery active clip unavailable");
  *out = state.slot;
  return 1;
}
static int gallery_timing(void *p, int32_t clip, BkEndingClipTiming *out,
                           char e[256]) {
  BkClipTiming timing;
  if (!out || clip < 0 || !bk_actor_pose_timing(scene_primary(p),
                                               (unsigned)clip, &timing))
    return fail(e, "gallery clip timing unavailable");
  *out = (BkEndingClipTiming){timing.start, timing.end, timing.source};
  return 1;
}
static int gallery_timing_unsigned(void *p, unsigned clip, BkEndingClipTiming *out,
                                    char e[256]) {
  return clip <= INT32_MAX ? gallery_timing(p, (int32_t)clip, out, e)
                          : fail(e, "gallery clip exceeds signed range");
}
static int gallery_actor_request(void *p, unsigned actor, int32_t clip,
                                  char e[256]) {
  BkActorPose *pose = actor < 3 ? scene_actor(p, actor) : NULL;
  return pose && clip >= 0 ? bk_actor_pose_request_mode(pose, (unsigned)clip,
      BK_CLIP_REQUEST_CONFIGURED, e) : fail(e, "gallery request has no actor/clip");
}
static int gallery_request(void *p, int32_t clip, char e[256]) {
  return gallery_actor_request(p, 0, clip, e);
}
static int gallery_restart(void *p, int32_t clip, char e[256]) {
  return clip >= 0 ? bk_actor_pose_select(scene_primary(p), (unsigned)clip, 0, e)
                  : fail(e, "negative gallery restart clip");
}
static int gallery_source(void *p, int32_t clip, float source, char e[256]) {
  BkClipEdit edit = {.slot = (unsigned)clip, .fields = BK_CLIP_EDIT_SOURCE,
                     .source = source};
  return clip >= 0 ? bk_actor_pose_edit_clips(scene_primary(p), &edit, 1, e)
                  : fail(e, "negative gallery source clip");
}
static int gallery_chain(void *p, int32_t clip, int32_t chain, char e[256]) {
  BkClipEdit edit = {.slot = (unsigned)clip, .fields = BK_CLIP_EDIT_CHAIN,
                     .chain = chain};
  return clip >= 0 ? bk_actor_pose_edit_clips(scene_primary(p), &edit, 1, e)
                  : fail(e, "negative gallery chain clip");
}
static int gallery_selected_clip(void *p, int32_t clip,
                                  BkEndingGallerySelectedClip *out, char e[256]) {
  BkEndingClipTiming t;
  int32_t chain, next;
  int completed;
  if (!out || !gallery_timing(p, clip, &t, e) ||
      !bk_actor_pose_clip_link(scene_primary(p), (unsigned)clip, &chain, &next) ||
      !bk_actor_pose_completed_chain(scene_primary(p), (unsigned)clip, &completed))
    return fail(e, "gallery selected descriptor unavailable");
  *out = (BkEndingGallerySelectedClip){t.start, t.end, t.source, chain, completed};
  return 1;
}
static int gallery_secondary_clip(void *p, int32_t clip,
                                   BkEndingGallerySecondaryClip *out, char e[256]) {
  BkEndingGallerySelectedClip t;
  int32_t loop;
  BkClipState active;
  BkClipPrediction prediction;
  if (!out || !gallery_selected_clip(p, clip, &t, e) ||
      !bk_actor_pose_loop_mode(scene_primary(p), (unsigned)clip, &loop) ||
      !bk_actor_pose_state(scene_primary(p), &active) ||
      !bk_actor_pose_prediction(scene_primary(p), (unsigned)clip, &prediction))
    return fail(e, "gallery secondary descriptor unavailable");
  /*48BCBB advances this actor at half speed. A duration20 loop can wrap
   *before4843AA's fixed .3*60*2 source window is ever observable. Keep that
   *window, also accepting the actual next loop boundary; never fake source.*/
  float seconds = (float)((double)((EndingNormalScene *)p)->active_seconds * .5);
  float delta = !active.blend_done && active.blend_elapsed < 1e-6f
      ? 0 : (float)((double)seconds * 60);
  int crossing = active.slot == clip && loop && prediction.duration > 0 &&
      prediction.rate > 0 && delta > 0 &&
      ((float)((double)delta + active.elapsed) >= (double)prediction.duration ||
       (float)((double)delta * prediction.rate + t.source) > t.end);
  *out = (BkEndingGallerySecondaryClip){t.end, t.source, t.chain, loop, crossing};
  return 1;
}
static int gallery_hidden(void *p, unsigned actor, int hidden, char e[256]) {
  EndingNormalScene *s = p;
  uint32_t root = actor < 3 ? scene_root(s, actor) : 0;
  return root ? bk_actor_forest_visibility(scene_forest(s), root, hidden, e)
              : fail(e, "gallery actor root unavailable");
}
static int gallery_material(void *p, const char *name, uint32_t hidden,
                             float alpha, char e[256]) {
  EndingNormalScene *s = p;
  if (s->tertiary_assets)
    return bk_ending_tertiary_assets_material_alpha(s->tertiary_assets, name, hidden, alpha, e);
  if (s->selected_assets)
    return bk_ending_selected_assets_material_alpha(s->selected_assets, name, hidden, alpha, e);
  if (s->auxiliary_assets)
    return bk_ending_auxiliary_assets_material_alpha(s->auxiliary_assets, name, hidden, alpha, e);
  /*The registry retains the loader's original order and actual mutable
   *instances. All non-alpha fields survive this scalar material operation.*/
  for (size_t i = 0; i < s->special_material_count; ++i) {
    const BkEndingSpecialMaterial *r = &s->special_materials[i];
    const BkModelMaterial *m = bk_material_pose_material(r->pose, r->index);
    if (!m) return fail(e, "stale gallery material registry");
    if (strcmp(m->name, name)) continue;
    BkMaterialValuesEdit edit = {.index = r->index, .id = m->id};
    memcpy(edit.values.diffuse, m->diffuse, 16);
    memcpy(edit.values.ambient, m->ambient, 16);
    memcpy(edit.values.specular, m->specular, 16);
    memcpy(edit.values.emissive, m->emissive, 16);
    edit.values.power = m->power;
    edit.values.diffuse[3] = hidden ? 0 : alpha;
    return bk_material_pose_values(r->pose, &edit, 1, e);
  }
  /*A match in another loaded owner must not become a false name miss.*/
  const unsigned remaining[] = {1, 5};
  for (unsigned i = 0; i < 2; ++i) {
    const BkModel *model = bk_actor_pose_model(scene_actor(s, remaining[i]));
    if (!model) continue;
    for (uint32_t m = 0; m < model->material_count; ++m)
      if (!strcmp(model->materials[m].name, name))
        return fail(e, "gallery material requires another mutable rendering owner");
  }
  return 1; /*The native name lookup permits an absent material.*/
}
static int gallery_fade(void *p, uint8_t request, char e[256]) {
  EndingNormalScene *s = p;
  return bk_fade_sprite_request(&s->ui.sprites[52].transform.fade, request)
      ? 1 : fail(e, "gallery flash request rejected");
}
static int gallery_child(void *p, unsigned child, char e[256]) {
  EndingNormalScene *s = p;
  BkEndingProcess *process = s->process;
  switch (child) {
  case 4: {
    BkEndingGalleryNormalBindings b;
    if (!bk_ending_state_gallery_normal_bindings(s->state, &s->voice_volume,
          &s->effect_volume, &b)) return fail(e, "missing gallery normal bindings");
    BkEndingGalleryNormalOps ops = {s, selected_random, selected_load,
      gallery_play, gallery_expression, gallery_fov, gallery_camera, gallery_target5,
      gallery_hidden, gallery_actor_request, gallery_timing_unsigned,
      gallery_present, gallery_status, gallery_pause, selected_stop};
    return bk_ending_gallery_normal_step(&process->gallery_normal, &b,
                                          s->active_seconds, &ops, e);
  }
  case 5: {
    BkEndingGallerySecondaryBindings b;
    if (!bk_ending_state_gallery_secondary_bindings(s->state, &s->camera,
          &process->gallery_camera, &b)) return fail(e, "missing gallery secondary bindings");
    BkEndingGallerySecondaryOps ops = {s, frame_clock, selected_random,
      gallery_present, gallery_status, gallery_secondary_voice,
      gallery_expression, gallery_fov, gallery_camera, gallery_target0,
      gallery_active, gallery_secondary_clip, gallery_request, gallery_restart};
    return bk_ending_gallery_secondary_step(&process->gallery_secondary, &b,
                                             s->active_seconds, &ops, e);
  }
  case 6: {
    BkEndingGallerySelectedViews views = {&s->camera, &s->presets,
      &process->gallery_camera, &process->gallery_override,
      &s->ui.sprites[52].transform.fade.stage, &s->ui_flash_wanted,
      &s->state->frame.transition_action, &s->state->frame.curtain_wanted,
      &s->voice_volume, &s->effect_volume};
    BkEndingGallerySelectedBindings b;
    if (!bk_ending_state_gallery_selected_bindings(s->state, &views, &b))
      return fail(e, "missing gallery selected bindings");
    b.wait_for_intro_clip = 1;
    BkEndingGallerySelectedOps ops = {s, frame_clock, selected_random,
      gallery_present, gallery_status, gallery_selected_voice,
      selected_load, gallery_play, selected_stop, gallery_expression, gallery_fov,
      gallery_camera, gallery_target, gallery_active, gallery_selected_clip,
      gallery_source, gallery_chain, gallery_request, gallery_restart, gallery_fade};
    return bk_ending_gallery_selected_step(&process->gallery_selected, &b,
                                            s->active_seconds, &ops, e);
  }
  case 7: {
    BkEndingGalleryTertiaryViews views = {&s->selected_action->diagnostic_crossings,
      &process->gallery_override, &process->gallery_expression_latch,
      &s->voice_volume, &s->effect_volume};
    BkEndingGalleryTertiaryBindings b;
    if (!bk_ending_state_gallery_tertiary_bindings(s->state, &process->gallery_normal,
          &views, &b)) return fail(e, "missing gallery third bindings");
    BkEndingGalleryTertiaryOps ops = {s, selected_random, gallery_present,
      gallery_status, gallery_tertiary_voice, gallery_play_once,
      gallery_expression, gallery_material, gallery_hidden, gallery_actor_request,
      gallery_active, gallery_timing, gallery_target5};
    return bk_ending_gallery_tertiary_step(&b, &ops, e);
  }
  case 8: {
    BkEndingGalleryAuxiliaryViews views = {&s->camera, &s->presets,
      &process->gallery_camera, &process->gallery_override, &s->effect_volume};
    BkEndingGalleryAuxiliaryBindings b;
    if (!bk_ending_state_gallery_auxiliary_bindings(s->state, &views, &b))
      return fail(e, "missing gallery auxiliary bindings");
    BkEndingGalleryAuxiliaryOps ops = {s, gallery_present,
      gallery_status, gallery_auxiliary_voice, gallery_play, gallery_pause,
      gallery_expression, gallery_fov, gallery_camera, gallery_target,
      gallery_active, gallery_timing, gallery_source, gallery_request, gallery_restart};
    return bk_ending_gallery_auxiliary_step(&process->gallery_auxiliary, &b,
                                             s->active_seconds, &ops, e);
  }
  default: return fail(e, "unknown gallery child controller");
  }
}
static int gallery_control_step(EndingNormalScene *s, char e[256]) {
  BkEndingGalleryControlBindings b;
  if (!s->process || !bk_ending_state_gallery_bindings(s->state, s->records,
        &s->voice_volume, &b)) return fail(e, "missing recorded gallery owners");
  BkEndingGalleryControlOps ops = {s, gallery_expression, gallery_fov,
    gallery_camera, gallery_target5, gallery_present, gallery_status,
    selected_load, gallery_play_once, gallery_voice_name, gallery_tertiary_voice,
    gallery_cue, gallery_child};
  return bk_ending_gallery_control_step(&s->process->gallery_control, &b,
                                         s->active_seconds, &ops, e);
}
