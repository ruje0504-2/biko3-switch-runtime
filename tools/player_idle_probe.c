/* Actual45 entry/background collision sets, idle placement and follow-distance
 * handoff. This is a component-composition probe, not an application loop. */
#include "scene/background_assets.h"
#include "scene/entry_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[1024];
  BkResourceStore *store = bk_resources_create(error);
  BkEntryAssets *entry = NULL;
  BkBackgroundAssets *background = NULL;
  float *locals = NULL, *worlds = NULL;
  int rc = 1;
  unsigned profiles = 0, frames = 0, handovers = 0;
  int32_t actions[21] = {0};
  assert(bk_player_actions_initialize(actions));
  uint64_t matrices = 0;
  if (!store)
    goto done;
  const char *packs[] = {"bk3_01", "bk3_02", "bk3_03", "bk3_04"};
  for (unsigned i = 0; i < 4; ++i) {
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >=
            (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, error))
      goto done;
  }
  if (!bk_resources_mount_directory(store, "faces", argv[1], 20480, error) ||
      !bk_resources_mount_directory(store, "routes", argv[1], 20480, error) ||
      !bk_resources_mount_directory(store, "collision", argv[1],
                                    BK_COLLISION_ATR_SIZE, error))
    goto done;
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned a = 0; a < 9; ++a) {
      BkEntryRequest request = {.group = g, .area = a, .previous_flow = 8};
      entry = bk_entry_assets_create(store, &request, error);
      background = bk_background_assets_create(store, g, a, 1, 0, error);
      if (!entry || !background ||
          !bk_entry_assets_load_player_shadow(entry, store, error))
        goto done;
      BkActorPose *player = bk_entry_assets_player(entry);
      const BkModel *model = bk_actor_pose_model(player);
      const BkActorPlacement *initial = bk_actor_pose_placement(player);
      BkActorPlacement shadow = *bk_actor_pose_placement(
          bk_npc_shadow_pose(bk_entry_assets_player_shadow(entry)));
      BkPlayerControl control = {.completion_mode = 2, .wall_available = 3};
      memcpy(control.spatial.movement.position, initial->position, 12);
      control.spatial.movement.yaw = initial->yaw_degrees;
      control.spatial.vertical_position = initial->position[1] + 18;
      control.spatial.scene.wall.camera_distance = 177;
      BkPlayerSceneInput in = {
          .head_distance = 100,
          .projected_depth = .5f,
          .screen_scale = {.625f, .625f},
          .screen_position = {320, 240},
          .excluded_surface =
              bk_background_assets_config(background)->names[0]};
      if (!in.excluded_surface)
        in.excluded_surface = "";
      BkGameCameraState camera = {.phase = 0, .stage = 0};
      BkFollowCamera *follow = bk_entry_assets_camera(entry);
      locals = malloc((size_t)model->frame_count * 64);
      worlds = malloc((size_t)model->frame_count * 64);
      if (!locals || !worlds)
        goto done;
      for (unsigned step = 0; step < 24; ++step) {
        if (step % 6 == 0 &&
            (!bk_actor_pose_request(player, (unsigned)actions[1], error) ||
             !bk_actor_pose_advance(player, -1, .05f, error) ||
             !bk_actor_pose_advance(player, -1, .05f, error)))
          goto done;
        for (unsigned f = 0; f < model->frame_count; ++f) {
          memcpy(locals + 16 * f, bk_actor_pose_local(player, f), 64);
          memcpy(worlds + 16 * f, bk_actor_pose_frame(player, f), 64);
        }
        control.spatial.movement.interaction_mode = 2;
        float old[3];
        memcpy(old, control.spatial.movement.position, 12);
        const BkCameraFollowPose *pose = bk_follow_camera_pose(follow);
        memcpy(in.wall.camera, pose->world + 12, 12);
        for (unsigned r = 0; r < 7; ++r)
          memcpy(in.wall.rays[r], pose->world + 12, 12);
        if (!bk_entry_assets_step_player_idle(
                entry, &control, 0, bk_background_assets_collision(background),
                &in, error))
          goto done;
        assert(control.spatial.movement.action == 0 &&
               !control.spatial.movement.interaction_mode &&
               control.completion_mode == 2 && control.wall_available == 3);
        assert(!memcmp(old, control.spatial.movement.previous, 12));
        for (unsigned f = 0; f < model->frame_count; ++f)
          if (model->frames[f].parent_index != BK_MODEL_NONE) {
            assert(
                !memcmp(locals + 16 * f, bk_actor_pose_local(player, f), 64));
            assert(
                !memcmp(worlds + 16 * f, bk_actor_pose_frame(player, f), 64));
            matrices += 2;
          }
        assert(!memcmp(control.spatial.movement.position,
                       bk_actor_pose_placement(player)->position, 12));
        assert(!memcmp(&shadow,
                       bk_actor_pose_placement(bk_npc_shadow_pose(
                           bk_entry_assets_player_shadow(entry))),
                       sizeof(shadow)));
        BkClipState before, after;
        assert(bk_actor_pose_state(player, &before));
        assert(bk_actor_pose_request(player, 0, error) &&
               bk_actor_pose_state(player, &after) &&
               !memcmp(&before, &after, sizeof(before)));
        assert(before.requested == 0 &&
               (step % 6 || before.blend_elapsed == 0));
        const float *track = bk_follow_camera_track(follow);
        const float *npc =
            bk_actor_pose_placement(bk_entry_assets_actor(entry))->position;
        double dx = (double)npc[0] - track[12], dz = (double)npc[2] - track[14];
        float expected = (float)sqrt(dx * dx + dz * dz);
        if (!bk_entry_assets_step_camera_distance(
                entry, &camera, NULL, 1.f / 60,
                &control.spatial.scene.wall.camera_distance, error))
          goto done;
        assert(fabsf(expected - control.spatial.scene.wall.camera_distance) <
               1e-4f);
        /* The subsequent presentation is a separate actual clock update. */
        if (!bk_actor_pose_advance(player, 0, 1.f / 120, error))
          goto done;
        bk_actor_pose_publish(player);
        bk_follow_camera_publish(follow);
        ++frames;
      }
      float distance = control.spatial.scene.wall.camera_distance;
      camera.stage = 1;
      camera.transition = 0;
      if (!bk_entry_assets_step_camera_distance(
              entry, &camera, NULL, .5f,
              &control.spatial.scene.wall.camera_distance, error))
        goto done;
      assert(camera.stage == 2 &&
             distance == control.spatial.scene.wall.camera_distance);
      if (!bk_entry_assets_step_camera_distance(
              entry, &camera, NULL, .1f,
              &control.spatial.scene.wall.camera_distance, error))
        goto done;
      assert(distance == control.spatial.scene.wall.camera_distance);
      BkPlayerView view = {.lean = 10, .distance = 40};
      BkEntryPlayerViewInput view_in = {.seconds = 1.f / 60, .camera_mode = 0};
      uint8_t hidden = 0;
      assert(bk_player_actions_initialize(view_in.actions));
      if (!bk_entry_assets_step_player_view(entry, &control, &view, &hidden,
                                            &view_in, error))
        goto done;
      float weight = (float)((double)view_in.seconds * 3);
      float delta = (float)(((double)distance - 40) * weight);
      assert(view.distance == (float)(40.0 + delta));
      assert(view.target_distance == 40 &&
             control.spatial.scene.wall.camera_distance == 40);
      ++handovers;
      ++profiles;
      free(locals);
      free(worlds);
      locals = worlds = NULL;
      bk_entry_assets_destroy(entry);
      entry = NULL;
      bk_background_assets_destroy(background);
      background = NULL;
    }
  printf("PASS idle/follow binding: %u actual entry/collision profiles, %u "
         "idle frames, %" PRIu64
         " unchanged child matrices, %u distance handovers; shadow retained\n",
         profiles, frames, matrices, handovers);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "FAIL profile%u frame%u: %s\n", profiles, frames, error);
  free(locals);
  free(worlds);
  bk_entry_assets_destroy(entry);
  bk_background_assets_destroy(background);
  bk_resources_destroy(store);
  return rc;
}
