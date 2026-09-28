#include "game/player_animation.h"
#include "model/material.h"
#include "scene/entry_assets.h"
#include "scene/player_audio.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Explicit offline player-effect sink, never a physical-device assertion. */
typedef struct {
  uint64_t submitted, consumed, nonzero, hash;
} PlayerSoundCapture;
static int capture_submit(void *context, const int16_t *pcm, size_t frames,
                          char *error) {
  (void)error;
  PlayerSoundCapture *c = context;
  for (size_t i = 0; i < frames * 2; ++i) {
    uint16_t v = (uint16_t)pcm[i];
    c->nonzero += v != 0;
    c->hash = (c->hash ^ (uint8_t)v) * UINT64_C(1099511628211);
    c->hash = (c->hash ^ (uint8_t)(v >> 8)) * UINT64_C(1099511628211);
  }
  c->submitted += frames;
  return 1;
}
static int capture_poll(void *context, uint64_t *consumed, char *error) {
  (void)error;
  *consumed = ((PlayerSoundCapture *)context)->consumed;
  return 1;
}
static int mount(BkResourceStore *store, const char *directory,
                 const char *pack, char error[256]) {
  char path[1024];
  if (snprintf(path, sizeof(path), "%s/%s.pp", directory, pack) >=
      (int)sizeof(path))
    return 0;
  return bk_resources_mount(store, pack, path, error);
}
static BkCollision *collision_fixture(BkResourceStore *store,
                                      const char *directory, char error[256]) {
  BkBlob model_bytes = {0}, atr = {0};
  BkModel *model = NULL;
  float *world = NULL;
  BkCollision *collision = NULL;
  if (!mount(store, directory, "bk3_03", error) ||
      !bk_resources_mount_directory(store, "collision", directory,
                                    BK_COLLISION_ATR_SIZE, error) ||
      bk_resources_read(store, "bk3_03", "m01_04.x", &model_bytes, error) !=
          BK_RESOURCE_OK ||
      bk_resources_read(store, "collision", "m01_04.atr", &atr, error) !=
          BK_RESOURCE_OK ||
      bk_model_decode(model_bytes.data, model_bytes.size, &model, error) !=
          BK_MODEL_OK)
    goto done;
  size_t count = (size_t)model->frame_count * 16;
  world = malloc(count * sizeof(float));
  if (!world) {
    snprintf(error, 256, "entry probe: collision fixture allocation failed");
    goto done;
  }
  if (bk_model_world_matrices(model, world, count, error))
    collision = bk_collision_create(model, world, count, "M01_04.X", atr.data,
                                    atr.size, error);
done:
  free(world);
  bk_model_destroy(model);
  bk_blob_free(&model_bytes);
  bk_blob_free(&atr);
  return collision;
}
int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: entry-probe DATA_DIRECTORY\n");
    return 2;
  }
  char error[256] = {0};
  BkResourceStore *store = bk_resources_create(error);
  BkEntryAssets *assets = NULL;
  BkCollision *collision = NULL;
  BkAudio *audio = NULL;
  BkPlayerAudio *player_audio = NULL;
  PlayerSoundCapture capture = {.hash = UINT64_C(14695981039346656037)};
  int rc = 1;
  unsigned cases = 0;
  if (!store)
    goto done;
  BkEntryRequest request = {
      .group = 0, .area = 8, .route_cursor = 0, .previous_flow = 8};
  if (bk_entry_assets_create(store, &request, error)) {
    snprintf(error, 256, "unexpected missing-route success");
    goto done;
  }
  if (!bk_resources_mount_directory(store, "routes", argv[1], 20480, error))
    goto done;
  if (bk_entry_assets_create(store, &request, error)) {
    snprintf(error, 256, "unexpected missing-model success");
    goto done;
  }
  if (!mount(store, argv[1], "bk3_01", error))
    goto done;
  if (bk_entry_assets_create(store, &request, error)) {
    snprintf(error, 256, "unexpected missing-camera success");
    goto done;
  }
  if (!mount(store, argv[1], "bk3_04", error))
    goto done;
  if (bk_entry_assets_create(store, &request, error)) {
    snprintf(error, 256, "unexpected missing-face success");
    goto done;
  }
  if (!bk_resources_mount_directory(store, "faces", argv[1], 20480, error))
    goto done;
  /* Shared office query fixture, not entry/background selection. */
  collision = collision_fixture(store, argv[1], error);
  if (!collision)
    goto done;
  BkAudioSink sink = {&capture, 48000, 480, 1920, capture_submit, capture_poll};
  if (!mount(store, argv[1], "bk3_02", error) ||
      !(audio = bk_audio_create(&sink, error)) ||
      !(player_audio = bk_player_audio_create(store, audio, 2, error)))
    goto done;
  for (unsigned group = 0; group < 5; group++)
    for (unsigned area = 0; area < 9; area++) {
      request = (BkEntryRequest){
          .group = group, .area = area, .route_cursor = 0, .previous_flow = 8};
      assets = bk_entry_assets_create(store, &request, error);
      if (!assets)
        goto done;
      if (!bk_entry_assets_load_mesh_shadow(assets, store, error))
        goto done;
      BkNpcShadow *shadow = bk_entry_assets_shadow(assets);
      if (!shadow || bk_actor_pose_head(bk_npc_shadow_pose(shadow)) ||
          !bk_entry_assets_load_mesh_shadow(assets, store, error) ||
          shadow != bk_entry_assets_shadow(assets))
        goto done;
      BkFaceAssets *face = bk_entry_assets_face(assets);
      BkFaceState face_state;
      uint32_t face_random = 1, face_clocks[4] = {1000, 1001, 1002, 1003};
      if (!bk_face_assets_initialize(face, &face_state, face_clocks,
                                     &face_random, error))
        goto done;
      for (unsigned frame = 0; frame < 32; frame++) {
        uint32_t now = 1100 + frame * 137;
        if (!bk_face_assets_step(face, &face_state, 0, (float)(frame % 10), now,
                                 now + 1, now + 2, now + 3, &face_random,
                                 error))
          goto done;
      }
      const BkEntrySelection *selection = bk_entry_assets_selection(assets);
      BkActorPose *actor = bk_entry_assets_actor(assets),
                  *player = bk_entry_assets_player(assets);
      BkFollowCamera *camera = bk_entry_assets_camera(assets);
      BkActorPlacement placement = *bk_actor_pose_placement(actor);
      if (!bk_npc_shadow_place(shadow, &placement, error) ||
          !bk_npc_shadow_step(shadow, 0, 1.f / 60, error))
        goto done;
      bk_npc_shadow_publish(shadow);
      float base_height = bk_actor_pose_head(actor)[1];
      const BkRoutePoint *start =
          bk_route_point(bk_entry_assets_route(assets), 0);
      if (selection->phase != 0 || !selection->dialogue ||
          placement.position[0] != start->position[0] ||
          placement.position[2] != start->position[2] ||
          placement.position[1] != selection->player_position[1])
        goto mismatch;
      for (unsigned i = 0; i < 3; i++)
        if (bk_follow_camera_pose(camera)->position[i] !=
                placement.position[i] ||
            bk_actor_pose_placement(player)->position[i] !=
                selection->player_position[i])
          goto mismatch;
      BkClipState before, after;
      BkGameCameraState camera_state = {
          .phase = selection->phase, .stage = 0, .transition = 0};
      if (!bk_actor_pose_state(actor, &before))
        goto mismatch;
      bk_actor_pose_publish(actor);
      bk_actor_pose_publish(player);
      bk_follow_camera_publish(camera);
      if (!bk_actor_pose_state(actor, &after) ||
          memcmp(&before, &after, sizeof(before)))
        goto mismatch;
      /* Explicit stationary pose fixture: exercises real asset wiring, not AI.
       * The camera reads cached head/track before this frame's publication. */
      for (unsigned step = 0; step < 12; step++) {
        float cached[3];
        memcpy(cached, bk_actor_pose_head(actor), sizeof(cached));
        if (!bk_actor_pose_step(actor, placement.position,
                                placement.yaw_degrees, 1, 1.0f / 120, error))
          goto done;
        if (memcmp(cached, bk_actor_pose_head(actor), sizeof(cached)))
          goto mismatch;
        BkNpcHeadInput head_input;
        BkNpcHeadState head_state;
        if (!bk_entry_assets_head_input(assets, placement.yaw_degrees,
                                        &head_input, error) ||
            !bk_npc_head_update(&head_state, &head_input, error))
          goto done;
        if (memcmp(cached, head_state.sight.start, sizeof(cached)) ||
            head_input.actor_kind != (int32_t)group ||
            memcmp(head_state.sight.end, bk_actor_pose_head(player), 12))
          goto mismatch;
        if (!bk_entry_assets_step_camera(assets, &camera_state, NULL, 1.0f / 60,
                                         error))
          goto done;
        bk_actor_pose_publish(actor);
        bk_follow_camera_publish(camera);
      }
      if (!bk_follow_camera_clip_state(camera, &before))
        goto mismatch;
      for (unsigned mode = 0; mode < 2; mode++) {
        camera_state =
            (BkGameCameraState){.phase = 0, .stage = 1, .transition = mode};
        if (!bk_entry_assets_step_camera(assets, &camera_state, NULL, .5f,
                                         error) ||
            camera_state.stage != 2)
          goto mismatch;
        BkCameraFollowPose held = *bk_follow_camera_pose(camera);
        if (!bk_entry_assets_step_camera(assets, &camera_state, NULL, .5f,
                                         error) ||
            memcmp(&held, bk_follow_camera_pose(camera), sizeof(held)))
          goto mismatch;
      }
      if (!bk_follow_camera_clip_state(camera, &after) ||
          memcmp(&before, &after, sizeof(before)))
        goto mismatch;
      BkCameraFollowPose held = *bk_follow_camera_pose(camera);
      camera_state =
          (BkGameCameraState){.phase = 1, .stage = 0, .transition = 0};
      if (bk_entry_assets_step_camera(assets, &camera_state, NULL, 1.0f / 60,
                                      error) ||
          camera_state.stage != 0 ||
          memcmp(&held, bk_follow_camera_pose(camera), sizeof(held)))
        goto mismatch;
      BkNpcSpatialState spatial = {
          .ai = {.point = {.motion = {.action = 1, .behavior = 1},
                           .action_wait = {.duration = 2000}}},
          .alpha = 1};
      memcpy(spatial.path.position, placement.position, 12);
      spatial.path.yaw_degrees = placement.yaw_degrees;
      BkNpcInteractionState shared = {0};
      uint32_t random = 123;
      BkEntryNpcContext context = {
          .player_action = 18,
          .suppressed_actions = {18, 19, 20, 21, 23, 27},
          .short_range_action = 1,
          .player_direction = {0, 0, 1},
          .excluded_surface = ""};
      for (unsigned step = 0; step < 8; step++) {
        float cached[3];
        memcpy(cached, bk_actor_pose_head(actor), 12);
        if (!bk_actor_pose_state(actor, &before))
          goto mismatch;
        BkNpcSpatialEffects effects;
        if (!bk_entry_assets_step_npc_spatial(
                assets, &spatial, &shared, &random, collision, &context,
                1.0f / 60, step * 17, &effects, error))
          goto done;
        if (!bk_actor_pose_state(actor, &after) ||
            memcmp(&before, &after, sizeof(before)) ||
            memcmp(cached, bk_actor_pose_head(actor), 12) ||
            memcmp(effects.placement.world,
                   bk_actor_pose_placement(actor)->world, 64) ||
            effects.vertical_position !=
                (float)((double)spatial.path.position[1] + base_height))
          goto mismatch;
      }
      for (unsigned fade = 0; fade < 3; fade++) {
        spatial.ai.point.fade_out = fade == 1;
        spatial.ai.point.motion.hidden = fade == 2;
        float alpha = spatial.alpha;
        const BkModelMaterial first =
            *bk_entry_assets_actor_material(assets, 0);
        if (bk_entry_assets_step_npc_fade(assets, &spatial, NAN, error) ||
            spatial.alpha != alpha ||
            memcmp(&first, bk_entry_assets_actor_material(assets, 0),
                   sizeof(first)))
          goto mismatch;
        if (!bk_entry_assets_step_npc_fade(assets, &spatial, 1, error))
          goto done;
        if (spatial.alpha != (fade == 1 ? 0 : 1))
          goto mismatch;
        for (uint32_t m = 0;; m++) {
          const BkModelMaterial *material =
              bk_entry_assets_actor_material(assets, m);
          if (!material)
            break;
          BkMaterialState applied;
          if (!bk_material_state(material, 0, &applied, error))
            goto done;
        }
      }
      float held_head[3];
      memcpy(held_head, bk_actor_pose_head(actor), 12);
      spatial.ai.point.motion.hidden = 1;
      spatial.path.position[0] += 1;
      if (!bk_entry_assets_step_npc_visibility(assets, &spatial, 2, 1, error) ||
          !bk_actor_pose_step(actor, spatial.path.position,
                              spatial.path.yaw_degrees, 1, .1f, error))
        goto done;
      bk_actor_pose_publish(actor);
      if (memcmp(held_head, bk_actor_pose_head(actor), 12))
        goto mismatch;
      spatial.ai.point.motion.hidden = 0;
      if (!bk_entry_assets_step_npc_visibility(assets, &spatial, 2, 1, error))
        goto done;
      bk_actor_pose_publish(actor);
      if (!memcmp(held_head, bk_actor_pose_head(actor), 12))
        goto mismatch;
      uint8_t event_latches[BK_NPC_FOOTSTEP_LATCH_COUNT] = {0};
      /* Explicit live action-map fixture, separate from pending full actor
       * initialization. Footstep source must precede presentation advance. */
      const BkNpcFootstepActions foot_actions = {
          {1, 2}, {4, 5}, {10, 12}, {7, 9}};
      for (unsigned frame = 0; frame < 32; frame++) {
        uint32_t now = 8000 + frame * 31;
        spatial.ai.point.motion.action = frame % 8 < 4 ? 1 : 4;
        spatial.ai.point.motion.hidden = frame % 5 == 2;
        spatial.ai.point.fade_out = frame % 8 < 4;
        BkEntryNpcFootsteps foot;
        if (!bk_actor_pose_state(actor, &before) ||
            !bk_entry_assets_prepare_npc_footsteps(
                assets, &spatial, &foot_actions, 0, event_latches,
                sizeof(event_latches), &foot, error) ||
            !bk_actor_pose_state(actor, &after))
          goto done;
        if (memcmp(&before, &after, sizeof(before)) || foot.footsteps.count > 8)
          goto mismatch;
        BkEntryNpcPresentation input = {.seconds = 1.f / 60,
                                        .voice_level = (float)(frame % 10),
                                        .interface_mode = 2,
                                        .phase = (int8_t)(frame % 4),
                                        .timestamp_ms = now,
                                        .request_clock_ms = now + 1,
                                        .mouth_clock_ms = now + 2,
                                        .blink_clock_ms = now + 3};
        memcpy(held_head, bk_actor_pose_head(actor), 12);
        if (!bk_npc_shadow_place(shadow, bk_actor_pose_placement(actor),
                                 error) ||
            !bk_entry_assets_step_npc_presentation(
                assets, &spatial, &face_state, &face_random, &input, error))
          goto done;
        if (memcmp(held_head, bk_actor_pose_head(actor), 12))
          goto mismatch;
        bk_actor_pose_publish(actor);
        bk_npc_shadow_publish(shadow);
        if (spatial.ai.point.motion.hidden &&
            memcmp(held_head, bk_actor_pose_head(actor), 12))
          goto mismatch;
      }
      BkPlayerSpatial player_state = {0};
      const BkActorPlacement *player_initial = bk_actor_pose_placement(player);
      memcpy(player_state.movement.position, player_initial->position, 12);
      player_state.movement.yaw = player_initial->yaw_degrees;
      player_state.scene.wall.camera_distance = 200;
      BkPlayerSpatialInput player_input = {
          .movement = {.seconds = 1.f / 60, .controls_allowed = 1},
          .scene = {.head_distance = 20,
                    .screen_scale = {.625f, .625f},
                    .screen_position = {320, 240},
                    .projected_depth = .8f,
                    .excluded_surface = ""}};
      BkPlayerAnimationActions player_actions;
      if (!bk_player_actions_initialize(player_input.movement.actions) ||
          !bk_player_animation_actions(&player_actions))
        goto mismatch;
      for (unsigned frame = 0; frame < 32; ++frame) {
        float cached[3];
        memcpy(cached, bk_actor_pose_head(player), 12);
        /* Explicit inspection-view fixture projects the held NPC head. */
        float screen_view[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                 0, 0, 1, 0, 0, 0, 0, 1};
        const float *npc_head = bk_actor_pose_head(actor);
        screen_view[12] = -npc_head[0];
        screen_view[13] = -npc_head[1];
        screen_view[14] = 30 - npc_head[2];
        const BkCameraLens screen_lens = {1, .75f, .5f, 126384};
        const BkViewport screen_viewport = {0, 0, 640, 480};
        BkScreenPoint screen_point;
        if (!bk_entry_assets_project_actor_head(assets, &screen_point,
                                                screen_view, &screen_lens,
                                                &screen_viewport))
          goto mismatch;
        memcpy(player_input.scene.screen_position, screen_point.position, 8);
        player_input.scene.projected_depth = screen_point.depth;
        if (!bk_actor_pose_state(player, &before))
          goto mismatch;
        player_input.movement.buttons = frame % 8 < 4 ? BK_PLAYER_FORWARD : 0;
        if (frame % 4 == 0)
          player_input.movement.buttons |= BK_PLAYER_SLOW;
        player_input.movement.look[0] = (float)((int)(frame % 7) - 3);
        for (unsigned k = 0; k < 3; ++k)
          player_input.scene.wall.camera[k] = player_state.movement.position[k];
        player_input.scene.wall.camera[0] += 7;
        player_input.scene.wall.camera[2] += 20;
        for (unsigned ray = 0; ray < 7; ++ray)
          memcpy(player_input.scene.wall.rays[ray],
                 player_input.scene.wall.camera, 12);
        if (!bk_entry_assets_step_player_spatial(
                assets, &player_state, collision, &player_input, error))
          goto done;
        if (!bk_actor_pose_state(player, &after) ||
            memcmp(&before, &after, sizeof(before)) ||
            memcmp(cached, bk_actor_pose_head(player), 12) ||
            memcmp(player_state.movement.position,
                   bk_actor_pose_placement(player)->position, 12))
          goto mismatch;
        if (!bk_player_animation_step(
                player, player_state.movement.position,
                player_state.movement.yaw, &player_actions,
                player_state.movement.action, 1.f / 60, error))
          goto done;
        bk_actor_pose_publish(player);
      }
      /* Explicit nearby-prop fixture, using the player's actual retained
       * XAN slots. Exercise entry, end hold, exit and ordinary resumption. */
      if (!bk_entry_assets_load_player_shadow(assets, store, error) ||
          !bk_player_audio_stop(player_audio, error))
        goto done;
      BkNpcShadow *player_shadow = bk_entry_assets_player_shadow(assets);
      BkPlayerEventState player_event_state = {0};
      uint8_t player_steps[BK_PLAYER_EVENT_LATCH_COUNT] = {0},
              shared_ticks[426] = {0};
      BkPlayerControl control = {.spatial = player_state};
      control.spatial.movement.action = 0;
      control.spatial.movement.interaction_mode = 0;
      BkPlayerControlInput control_input = {.spatial = player_input};
      control_input.spatial.movement.seconds = .1f;
      control_input.spatial.movement.buttons = 0;
      control_input.spatial.movement.look[0] = 0;
      control_input.interaction.trigger.props[0] =
          (BkPlayerTriggerProp){.active = 1, .kind = 10};
      memcpy(control_input.interaction.trigger.props[0].position,
             control.spatial.movement.position, 12);
      control_input.interaction.trigger.props[0].position[0] += 10;
      BkPlayerView view = {.distance = 40, .target_distance = 40, .lean = 10};
      memcpy(view.probe, control_input.spatial.scene.wall.camera, 12);
      memcpy(view.rays, control_input.spatial.scene.wall.rays, 7 * 12);
      BkEntryPlayerViewInput view_input = {
          .camera_mode = 0,
          .npc_hidden = spatial.ai.point.motion.hidden,
          .npc_vertical =
              (float)((double)spatial.path.position[1] + base_height),
          .seconds = .1f};
      memcpy(view_input.actions, control_input.spatial.movement.actions,
             sizeof(view_input.actions));
      uint8_t player_hidden = 0;
      unsigned phases = 0, scripted_writes = 0;
      for (unsigned frame = 0; frame < 160; ++frame) {
        float cached[3];
        memcpy(cached, bk_actor_pose_head(player), 12);
        if (!bk_actor_pose_state(player, &before))
          goto mismatch;
        int phase_before = control.interaction.script_phase;
        control_input.interaction.buttons =
            frame == 0 || frame == 70 ? BK_PLAYER_INTERACT : 0;
        memcpy(control_input.spatial.scene.wall.camera, view.probe, 12);
        memcpy(control_input.spatial.scene.wall.rays, view.rays, 7 * 12);
        BkPlayerControlEffects effects;
        if (!bk_entry_assets_step_player_control(
                assets, &control, collision, &control_input, &effects, error))
          goto done;
        BkClipState camera_before, camera_after;
        if (!bk_follow_camera_clip_state(bk_entry_assets_camera(assets),
                                         &camera_before) ||
            !bk_entry_assets_step_player_view(
                assets, &control, &view, &player_hidden, &view_input, error))
          goto done;
        if (!bk_follow_camera_clip_state(bk_entry_assets_camera(assets),
                                         &camera_after) ||
            memcmp(&camera_before, &camera_after, sizeof(camera_before)) ||
            memcmp(&view.pose,
                   bk_follow_camera_pose(bk_entry_assets_camera(assets)),
                   sizeof(view.pose)))
          goto mismatch;
        phases |= 1u << control.interaction.script_phase;
        scripted_writes +=
            (phase_before == 1 || phase_before == 2) && effects.placements == 2;
        if (!bk_actor_pose_state(player, &after) ||
            memcmp(&before, &after, sizeof(before)) ||
            memcmp(cached, bk_actor_pose_head(player), 12) ||
            memcmp(control.spatial.movement.position,
                   bk_actor_pose_placement(player)->position, 12))
          goto mismatch;
        if (effects.shadow_place &&
            memcmp(&effects.shadow,
                   bk_actor_pose_placement(bk_npc_shadow_pose(player_shadow)),
                   sizeof(effects.shadow)))
          goto mismatch;
        uint64_t advance = capture.submitted - capture.consumed;
        if (advance > 960)
          advance = 960;
        capture.consumed += advance;
        if (!bk_audio_poll(audio, error))
          goto done;
        BkEntryPlayerPresentation presentation = {
            .seconds = .1f,
            .action = control.spatial.movement.action,
            .camera_mode = view_input.camera_mode,
            .interface_mode = 0,
            .interaction_mode = control.spatial.movement.interaction_mode,
            .hidden = player_hidden,
            .surface = control.spatial.scene.surface_name};
        memcpy(presentation.actions, view_input.actions,
               sizeof(presentation.actions));
        if (!bk_player_audio_presentation(
                player_audio, assets, &player_event_state, player_steps,
                sizeof(player_steps), shared_ticks, sizeof(shared_ticks),
                &presentation, 0, error) ||
            !bk_audio_fill(audio, error))
          goto done;
        if (memcmp(cached, bk_actor_pose_head(player), 12))
          goto mismatch;
        bk_actor_pose_publish(player);
        bk_npc_shadow_publish(player_shadow);
      }
      if (phases != 7 || !scripted_writes ||
          control.interaction.script_phase != 0)
        goto mismatch;
      /* All nine camera routes on actual entry assets; these are explicit
       * controller fixtures, not automatic mission-state selection. */
      /* TRACK aims from the previous camera position. Run it after ORBIT:
       * WALL -> TRACK puts the camera exactly at its aim target, for which
       * the original aim math is undefined (the unit test checks rejection). */
      const int8_t view_modes[9] = {0, 3, 1, 2, 2, 2, 5, 6, 7};
      const unsigned view_slots[9] = {0, 0, 8, 11, 13, 16, 20, 0, 0};
      view_input.npc_hidden = 0;
      for (unsigned route_case = 0; route_case < 9; ++route_case) {
        control.spatial.movement.interaction_mode = view_modes[route_case];
        control.spatial.movement.action =
            view_input.actions[view_slots[route_case]];
        BkClipState clock_before, clock_after;
        float cached[3], view_matrix[16];
        memcpy(cached, bk_actor_pose_head(player), 12);
        if (!bk_follow_camera_clip_state(bk_entry_assets_camera(assets),
                                         &clock_before) ||
            !bk_entry_assets_step_player_view(
                assets, &control, &view, &player_hidden, &view_input, error)) {
          fprintf(stderr, "entry %u/%u camera route %u mode %d\n", group, area,
                  route_case, view_modes[route_case]);
          goto done;
        }
        if (!bk_follow_camera_clip_state(bk_entry_assets_camera(assets),
                                         &clock_after) ||
            (view_modes[route_case] != 3 &&
             memcmp(&clock_before, &clock_after, sizeof(clock_before))) ||
            memcmp(cached, bk_actor_pose_head(player), 12) ||
            !bk_camera_view(view_matrix, view.pose.world))
          goto mismatch;
      }
      printf("PASS entry %u/%u %s %s %u points\n", group, area,
             selection->route_file, selection->camera_clip,
             bk_route_count(bk_entry_assets_route(assets)));
      cases++;
      bk_entry_assets_destroy(assets);
      assets = NULL;
    }
  request = (BkEntryRequest){
      .group = 3, .area = 8, .route_cursor = 1, .previous_flow = 8};
  assets = bk_entry_assets_create(store, &request, error);
  if (assets)
    goto mismatch;
  printf("PASS %u profiles, partial-load cleanup, one-point bounds, "
         "1440 combined presentation frames,1440 player spatial frames and7200 "
         "player control/view/presentation/audio frames and 405 camera-route "
         "fixtures; CPU "
         "pose "
         "and explicit shared collision/spatial fixture only\n",
         cases);
  if (!capture.nonzero)
    goto mismatch;
  printf("PASS offline player PCM frames=%" PRIu64 " nonzero=%" PRIu64
         " fnv1a=%016" PRIx64 "\n",
         capture.submitted, capture.nonzero, capture.hash);
  rc = 0;
  goto done;
mismatch:
  snprintf(error, 256, "entry binding mismatch");
done:
  bk_player_audio_destroy(player_audio);
  bk_audio_destroy(audio);
  bk_entry_assets_destroy(assets);
  bk_collision_destroy(collision);
  bk_resources_destroy(store);
  if (rc)
    fprintf(stderr, "FAIL: %s\n", error);
  return rc;
}
