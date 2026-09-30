#include "scene/game_frame.h"
#include "game/failure_frame.h"
#include "game/npc_detection.h"
#include <math.h>
#include <string.h>
int bk_scene_game_frame_boot_state(BkGameFrameState *state,
                                   BkEntryProgress *progress, uint32_t seed) {
  if (!state || !progress)
    return 0;
  *state = (BkGameFrameState){.random = seed, .player_view = {.lean = 10}};
  *progress = (BkEntryProgress){0};
  return 1;
}
int bk_scene_game_frame_initialize_entry(BkEntryAssets *entry,
                                         BkGameFrameState *state,
                                         uint32_t route_start,
                                         const uint32_t clocks[4],
                                         char error[256]) {
  if (!bk_scene_game_frame_initialize_actors(entry, state, route_start, clocks,
                                             error))
    return 0;
  if (!bk_entry_player_view_reset(&state->player_view,
                                  state->npc.path.position))
    return 0;
  state->player.spatial.scene.wall.camera_distance =
      state->player_view.target_distance;
  return 1;
}
typedef struct {
  const BkGameFrameServices *services;
  BkGameFrameState *state;
  const BkGameFrameInput *input;
  BkGameFrameResult *result;
  BkCollision *collision;
} Frame;
static int fail(char *error, const char *message) {
  snprintf(error, 256, "game frame: %s", message);
  return 0;
}
static void suppressed(int32_t out[6], const int32_t actions[21]) {
  const unsigned slots[6] = {11, 12, 13, 14, 16, 17};
  for (unsigned i = 0; i < 6; ++i)
    out[i] = actions[slots[i]];
}
static int background_clip(Frame *f, int32_t *slot, char *error) {
  BkClipState clip;
  const BkActorPose *pose =
      bk_background_assets_pose(f->services->background, 0);
  if (!pose || !bk_actor_pose_state(pose, &clip))
    return fail(error, "missing background clock");
  *slot = clip.slot;
  return 1;
}
static int consume(void *context, BkGameFrameEvent event, char *error) {
  Frame *f = context;
  const BkGameFrameServices *v = f->services;
  BkGameFrameState *s = f->state;
  const BkGameFrameInput *in = f->input;
  BkPlayerMovement *player = &s->player.spatial.movement;
  const BkBackgroundConfig *config = bk_background_assets_config(v->background);
  f->result->events[f->result->count++] = event;
  switch (event) {
  case BK_FRAME_COLLISION_BEGIN:
    f->result->static_meshes = bk_collision_count(f->collision);
    if (!bk_prop_assets_collision(v->props, f->collision, error))
      return 0;
    f->result->frame_meshes = bk_collision_count(f->collision);
    return 1;
  case BK_FRAME_COLLISION_END:
    bk_collision_end_props(f->collision);
    return 1;
  case BK_FRAME_BACKGROUND: {
    BkBackgroundInput bg = {.now = in->now_ms,
                            .seconds = in->seconds,
                            .group = (int32_t)s->group,
                            .area = (int32_t)s->area,
                            .music_master = in->music_volume,
                            .effect_master = in->effect_volume,
                            .player_yaw = player->yaw,
                            .weather_enabled = in->weather_enabled,
                            .ambient_gate = (int8_t)s->boundary.ambient_gate};
    memcpy(bg.player, player->position, 12);
    memcpy(bg.npc, s->npc.path.position, 12);
    BkBackgroundCommands commands;
    return bk_background_audio_step(v->background_audio, v->background,
                                    &s->background, &s->random, &bg, &commands,
                                    error);
  }
  case BK_FRAME_PLAYER_IDLE:
  case BK_FRAME_PLAYER_CONTROL: {
    BkPlayerSceneInput scene = in->player_scene;
    scene.npc_hidden = s->npc.ai.point.motion.hidden;
    scene.excluded_surface = config->names[0];
    if (!scene.excluded_surface)
      scene.excluded_surface = "";
    if (event == BK_FRAME_PLAYER_IDLE)
      return bk_entry_assets_step_player_idle(v->entry, &s->player,
                                              s->player_actions[0],
                                              f->collision, &scene, error);
    /*4c009b latches the OLD script phase. In4c0126 the hotkeys precede
     *4c20ee interaction and movement; the preceding local-vector copies do
     *not read these menu/camera/photo globals or invoke external services. */
    if (s->player.interaction.script_phase == 0 &&
        !bk_scene_player_hotkeys(
            &v->hotkeys, &s->hotkeys, &s->camera.transition, s->group,
            s->album_group, in->special_mode, in->hotkey_buttons, error))
      return 0;
    BkPlayerControlInput control = {0};
    control.spatial.scene = scene;
    control.spatial.movement.seconds = in->seconds;
    control.spatial.movement.buttons = in->movement_buttons;
    memcpy(control.spatial.movement.look, in->look, sizeof(in->look));
    memcpy(control.spatial.movement.actions, s->player_actions,
           sizeof(s->player_actions));
    control.interaction.buttons = in->interaction_buttons;
    for (unsigned i = 0; i < bk_prop_assets_count(v->props); ++i) {
      const BkPropState *p = bk_prop_assets_state(v->props, i);
      control.interaction.trigger.props[i].active = 1;
      control.interaction.trigger.props[i].kind = p->kind;
      memcpy(control.interaction.trigger.props[i].position, p->path.position,
             12);
    }
    BkPlayerControlEffects effects;
    /* Player+7c8 and global71bcd8 are the SAME outcome byte. */
    s->player.completion_requested = (int8_t)s->interaction.outcome;
    int ok = bk_entry_assets_step_player_control(
        v->entry, &s->player, f->collision, &control, &effects, error);
    s->interaction.outcome = (uint8_t)s->player.completion_requested;
    return ok;
  }
  case BK_FRAME_PLAYER_VIEW: {
    BkEntryPlayerViewInput view = {.camera_mode = (int8_t)s->camera.transition,
                                   .npc_hidden = s->npc.ai.point.motion.hidden,
                                   .npc_vertical = s->npc_vertical,
                                   .seconds = in->seconds,
                                   .buttons = in->cover_buttons};
    memcpy(view.actions, s->player_actions, sizeof(view.actions));
    return bk_entry_assets_step_player_view(
        v->entry, &s->player, &s->player_view, &s->player_hidden, &view, error);
  }
  case BK_FRAME_PLAYER_PRESENTATION: {
    BkEntryPlayerPresentation presentation = {
        .seconds = in->seconds,
        .action = player->action,
        .camera_mode = (int8_t)s->camera.transition,
        .interface_mode = in->interface_mode,
        .interaction_mode = player->interaction_mode,
        .hidden = s->player_hidden,
        .surface = s->player.spatial.scene.surface_name};
    memcpy(presentation.actions, s->player_actions,
           sizeof(presentation.actions));
    s->player_events.noise = s->npc.ai.stimulus;
    int ok = bk_player_audio_presentation(
        v->player_audio, v->entry, &s->player_events, s->player_latches,
        sizeof(s->player_latches), s->shared_latches, sizeof(s->shared_latches),
        &presentation, in->effect_volume, error);
    s->npc.ai.stimulus = s->player_events.noise;
    return ok;
  }
  case BK_FRAME_NPC_SPATIAL: {
    BkEntryNpcContext npc = {.player_action = player->action,
                             .short_range_action = s->player_actions[8],
                             /*71bcdf/e0 are inventory slots3/4. */
                             .interaction_df = (int8_t)s->pickup.collected[3],
                             .interaction_e0 = (int8_t)s->pickup.collected[4],
                             .excluded_surface =
                                 config->names[0] ? config->names[0] : ""};
    /*71b544 is live player action slot8 (actor+34, table starts+14). */
    suppressed(npc.suppressed_actions, s->player_actions);
    memcpy(npc.player_direction, player->velocity, 12);
    if (!background_clip(f, &npc.background_clip, error))
      return 0;
    BkNpcSpatialEffects effects;
    if (!bk_entry_assets_step_npc_spatial(
            v->entry, &s->npc, &s->interaction, &s->random, f->collision, &npc,
            in->seconds, in->now_ms, &effects, error))
      return 0;
    s->npc_vertical = effects.vertical_position;
    if (!bk_npc_event_audio_apply(v->npc_events, &effects, (int32_t)s->group,
                                  (int32_t)s->area, player->position,
                                  player->yaw, in->effect_volume, error))
      return 0;
    BkEntryNpcFootsteps steps;
    if (!bk_entry_assets_prepare_npc_footsteps(
            v->entry, &s->npc, &s->npc_actions, in->effect_volume,
            s->shared_latches, sizeof(s->shared_latches), &steps, error) ||
        !bk_npc_audio_footsteps(v->npc_audio, &steps, error))
      return 0;
    BkNpcShadow *shadow = bk_entry_assets_shadow(v->entry);
    return !shadow ||
           bk_npc_shadow_place(
               shadow, bk_actor_pose_placement(bk_entry_assets_actor(v->entry)),
               error);
  }
  case BK_FRAME_NPC_PRESENTATION: {
    BkEntryNpcPresentation presentation = {
        .seconds = in->seconds,
        .interface_mode = in->interface_mode,
        .phase = (int8_t)s->camera.phase,
        .timestamp_ms = in->face_clocks[0],
        .request_clock_ms = in->face_clocks[1],
        .mouth_clock_ms = in->face_clocks[2],
        .blink_clock_ms = in->face_clocks[3]};
    if (!bk_npc_audio_voice(v->npc_audio, &s->voice, in->seconds,
                            &presentation.voice_level, error))
      return 0;
    return bk_entry_assets_step_npc_presentation(
        v->entry, &s->npc, &s->face, &s->random, &presentation, error);
  }
  case BK_FRAME_PROP_SPATIAL: {
    BkPropMotionInput prop = {.seconds = in->seconds,
                              .player_action = player->action,
                              .npc_last_crossed =
                                  (int32_t)s->npc.path.last_crossed,
                              .player_mode = player->interaction_mode};
    memcpy(prop.player_actions, s->player_actions, sizeof(prop.player_actions));
    if (!background_clip(f, &prop.background_clip, error))
      return 0;
    if (prop.npc_last_crossed > 2)
      for (unsigned i = 0; i < 2; ++i) {
        const BkRoutePoint *point = bk_route_point(
            bk_entry_assets_route(v->entry), s->npc.path.last_crossed - 1 - i);
        if (!point)
          return fail(error, "NPC previous point out of range");
        prop.npc_previous_flags[i] = (int8_t)point->flags;
      }
    BkNpcSceneInput ground[16] = {0};
    for (unsigned i = 0; i < bk_prop_assets_count(v->props); ++i) {
      ground[i].sight = in->prop_head[i].sight;
      ground[i].cone =
          (BkSightInput){.head_distance = in->prop_head[i].sight.distance,
                         .facing = in->prop_head[i].facing,
                         .actor_kind = (int32_t)s->group,
                         .player_action = player->action,
                         .short_range_action = s->player_actions[8]};
      memcpy(ground[i].cone.actor_position, s->npc.path.position, 12);
      memcpy(ground[i].cone.player_position, player->position, 12);
      suppressed(ground[i].suppressed_actions, s->player_actions);
      ground[i].excluded_surface = config->names[0] ? config->names[0] : "";
    }
    BkPropMotionEffects effects[16];
    BkPropSoundInput sound = {.listener_yaw = player->yaw,
                              .effect_volume = in->effect_volume};
    memcpy(sound.listener, player->position, 12);
    return bk_prop_audio_step(
        v->prop_audio, v->props, &s->props, &prop, f->collision, ground,
        bk_prop_assets_count(v->props), effects, &sound, error);
  }
  case BK_FRAME_PROP_PRESENTATION:
    return bk_prop_assets_step_presentation(v->props, in->seconds, error);
  case BK_FRAME_ITEMS:
    return bk_item_assets_step(v->items, in->seconds, error);
  case BK_FRAME_PROP_INTERACTION: {
    BkPropInteractionInput input = {.wall = s->player.spatial.scene.wall_name,
                                    .now_ms = in->now_ms,
                                    .hud_blocked = in->hud_blocked};
    memcpy(input.player_actions, s->player_actions,
           sizeof(input.player_actions));
    BkPropInteractionCommands commands;
    return bk_prop_audio_interact(v->prop_audio, v->props, &s->player,
                                  &s->npc.ai.stimulus, &s->interaction.outcome,
                                  &s->prop_interaction, &input, &commands,
                                  error);
  }
  case BK_FRAME_DETECTION: {
    BkNpcDetectionInput input = {.area = (int32_t)s->area,
                                 .player_action = player->action};
    memcpy(input.suppressed_actions, s->player_actions + 11,
           sizeof(input.suppressed_actions));
    memcpy(input.forced_actions, s->npc_actions.special,
           sizeof(input.forced_actions));
    return bk_npc_detection_resolve(&s->npc, &s->interaction, &input);
  }
  case BK_FRAME_AREA: {
    BkPropState props[16] = {0};
    BkPropInteractionActor bindings[16];
    unsigned count = bk_prop_assets_count(v->props);
    for (unsigned i = 0; i < count; ++i) {
      if (!bk_prop_assets_bind_interaction(v->props, i, &bindings[i]))
        return fail(error, "missing prop state");
      props[i] = *bindings[i].motion;
    }
    BkAreaBoundaryInput input = {
        .group = s->group,
        .area = s->area,
        .props_present = (1u << count) - 1,
        .npc_present = 1,
        .npc_group = (int32_t)s->group,
        .effect_volume = in->effect_volume,
        .player_sound_suppressed = s->player_sound_suppressed,
        .player_yaw = player->yaw,
        .player_wall = s->player.spatial.scene.wall_name,
        .boundary_wall = config->names[1] ? config->names[1] : ""};
    memcpy(input.player_position, player->position, 12);
    memcpy(&input.bounds, config->bounds, sizeof(input.bounds));
    BkAreaBoundaryCommands commands;
    if (!bk_area_boundary_step(&s->boundary, props, &s->npc, &s->interaction,
                               &input, &commands))
      return fail(error, "invalid area rules");
    for (unsigned i = 0; i < count; ++i)
      *bindings[i].motion = props[i];
    return bk_area_audio_apply(v->area_audio, &commands, error);
  }
  case BK_FRAME_PICKUP: {
    BkItemPickupOps ops = bk_item_feedback_ops(v->item_feedback);
    return bk_item_assets_pickup(v->items, &s->pickup, player->position,
                                 player->previous, &ops, &f->result->pickups,
                                 error);
  }
  case BK_FRAME_CAMERA: {
    if (bk_game_camera_route(&s->camera) == BK_GAME_CAMERA_FOLLOW_ACTOR) {
      BkFollowObstacle obstacle = {
          .distance = s->player.spatial.scene.wall.camera_distance,
          .singular_intersections = s->singular_camera_intersections};
      memcpy(obstacle.point, s->player_view.probe, 12);
      BkActorPose *npc = bk_entry_assets_actor(v->entry);
      if (!bk_follow_camera_step_collision(
              bk_entry_assets_camera(v->entry),
              bk_actor_pose_placement(npc)->position, bk_actor_pose_head(npc),
              f->collision,
              bk_game_follow_obstacles_enabled((int32_t)s->group,
                                               (int32_t)s->area),
              in->seconds, &obstacle, error))
        return 0;
      s->player.spatial.scene.wall.camera_distance = obstacle.distance;
      memcpy(s->player_view.probe, obstacle.point, 12);
      s->singular_camera_intersections = obstacle.singular_intersections;
      return 1;
    }
    return bk_entry_assets_step_camera_distance(
        v->entry, &s->camera, NULL, in->seconds,
        &s->player.spatial.scene.wall.camera_distance, error);
  }
  }
  return fail(error, "unknown update stage");
}
int bk_scene_game_frame(const BkGameFrameServices *v, BkGameFrameState *s,
                        const BkGameFrameInput *in, BkGameFrameResult *out,
                        char error[256]) {
  if (!v || !s || !in || !out || !v->entry || !v->background || !v->props ||
      !v->items || !v->background_audio || !v->player_audio || !v->npc_audio ||
      !v->npc_events || !v->prop_audio || !v->area_audio || !v->item_feedback ||
      s->group >= 5 || s->area >= 9 || !isfinite(in->seconds) ||
      in->seconds < 0 || bk_prop_assets_count(v->props) > 16 ||
      bk_background_assets_config(v->background) !=
          bk_background_config(s->group, s->area))
    return fail(error, "invalid live bindings/state/step");
  *out = (BkGameFrameResult){0};
  /*51917c: latch BEFORE the main update. Standalone game diagnostics need
   * this too; the application latches the same field for all other flows. */
  s->album_group = s->group;
  Frame frame = {v, s, in, out, bk_background_assets_collision(v->background)};
  int ok = bk_game_frame_dispatch(
      &s->camera.phase, &s->npc.ai.point.motion.mode, consume, &frame, error);
  s->player.completion_requested = (int8_t)s->interaction.outcome;
  return ok;
}
int bk_scene_game_frame_initialize_actors(BkEntryAssets *entry,
                                          BkGameFrameState *state,
                                          uint32_t route_start,
                                          const uint32_t clocks[4],
                                          char error[256]) {
  if (!entry || !state || !clocks) {
    snprintf(error, 256, "game actors: missing entry/state/clocks");
    return 0;
  }
  if (!bk_entry_assets_initialize_player(entry, &state->player,
                                         state->player_actions,
                                         state->pickup.collected, error) ||
      !bk_entry_assets_initialize_npc(entry, &state->npc, &state->npc_actions,
                                      &state->npc_entry_route, route_start,
                                      &state->npc_vertical, error) ||
      !bk_face_assets_initialize(bk_entry_assets_face(entry), &state->face,
                                 clocks, &state->random, error))
    return 0;
  state->player_hidden = state->player_sound_suppressed = 0;
  state->group = bk_entry_assets_request(entry)->group;
  state->area = bk_entry_assets_request(entry)->area;
  state->boundary.player_sound_played = state->boundary.npc_sound_played = 0;
  state->interaction.response = state->interaction.outcome = 0;
  state->hotkeys.menu_request = 0; /*4bf1a0 player+7c9*/
  state->camera.phase = bk_entry_assets_selection(entry)->phase;
  state->npc.ai.point.motion.mode = (int8_t)state->camera.phase;
  return 1;
}

typedef struct {
  Frame frame;
  BkCameraLens *lens;
  BkPropState *prop;
} FailureFrame;
static int failure_camera(void *context, BkFailureCameraKind kind, int *arrived,
                          char error[256]) {
  FailureFrame *f = context;
  BkGameFrameState *s = f->frame.state;
  BkFailureCameraInput input = {.group = s->group,
                                .player_yaw = s->player.spatial.movement.yaw,
                                .npc_yaw = s->npc.path.yaw_degrees,
                                .seconds = f->frame.input->seconds};
  memcpy(input.player, s->player.spatial.movement.position, 12);
  memcpy(input.npc, s->npc.path.position, 12);
  if (f->prop) {
    memcpy(input.prop, f->prop->path.position, 12);
    input.prop_yaw = f->prop->path.yaw;
  }
  BkFailureCameraEffects effects;
  BkEntryAssets *entry = f->frame.services->entry;
  if (!bk_entry_assets_base_heights(entry, &input.player_height,
                                    &input.npc_height) ||
      !bk_follow_camera_failure(bk_entry_assets_camera(entry), kind, &input,
                                &effects, error))
    return 0;
  *arrived = effects.arrived;
  if (effects.write_fov)
    f->lens->fov_y = effects.fov;
  return 1;
}
static int failure_restart(void *context, int prop, char error[256]) {
  FailureFrame *f = context;
  return prop ? bk_prop_audio_restart(
                    f->frame.services->prop_audio,
                    (uint32_t)f->frame.state->prop_interaction.selected, error)
              : bk_npc_audio_restart_speech(f->frame.services->npc_audio, 0,
                                            error);
}
static int failure_present(void *context, BkFailureFrameStage stage,
                           char error[256]) {
  static const BkGameFrameEvent events[] = {
      BK_FRAME_PLAYER_PRESENTATION, BK_FRAME_NPC_PRESENTATION,
      BK_FRAME_BACKGROUND, BK_FRAME_PROP_PRESENTATION};
  return consume(&((FailureFrame *)context)->frame, events[stage], error);
}
int bk_scene_failure_frame(const BkGameFrameServices *v, BkGameFrameState *s,
                           const BkGameFrameInput *in, uint8_t *visible,
                           BkCameraLens *lens, BkGameFrameResult *out,
                           char error[256]) {
  if (!v || !s || !in || !visible || !lens || !out || !v->entry ||
      !v->background || !v->props || !v->background_audio || !v->player_audio ||
      !v->npc_audio || !v->prop_audio || s->group >= 5 || s->area >= 9 ||
      !isfinite(in->seconds) || in->seconds < 0)
    return fail(error, "invalid failure frame bindings/time");
  BkPropInteractionActor prop = {0};
  if (s->interaction.outcome == 5 &&
      (s->prop_interaction.selected < 0 ||
       !bk_prop_assets_bind_interaction(
           v->props, (uint32_t)s->prop_interaction.selected, &prop)))
    return fail(error, "missing failure prop");
  BkNpcMotionActions actions;
  if (!bk_npc_motion_actions(&actions, s->group))
    return fail(error, "missing NPC idle binding");
  BkFailureFrameBindings bindings = {visible,
                                     &s->player_hidden,
                                     &s->npc.ai.point.motion.hidden,
                                     &s->player.spatial.movement.action,
                                     &s->npc.ai.point.motion.action,
                                     &s->player_actions[0],
                                     &actions.idle,
                                     &s->npc_actions.special[0],
                                     prop.motion ? &prop.motion->action : NULL};
  *out = (BkGameFrameResult){0};
  s->album_group = s->group;
  BkGameFrameInput input = *in;
  input.interface_mode = 0x40;
  FailureFrame frame = {{v, s, &input, out, NULL}, lens, prop.motion};
  BkFailureFrameOps ops = {&frame, failure_camera, failure_restart,
                           failure_present};
  return bk_failure_frame_step(s->interaction.outcome, &bindings, &ops, error);
}
