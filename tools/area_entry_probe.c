/* Actual retained actors through all40 next-area CKP/track replacements. */
#include "scene/game_frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "area-entry line%d: %s\n", __LINE__, error);             \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static float *snapshot(BkActorPose *a, size_t *count) {
  const BkModel *m = bk_actor_pose_model(a);
  *count = (size_t)m->frame_count * 32;
  float *out = malloc(*count * sizeof(float));
  if (!out)
    return NULL;
  for (uint32_t i = 0; i < m->frame_count; ++i) {
    memcpy(out + i * 32, bk_actor_pose_local(a, i), 64);
    memcpy(out + i * 32 + 16, bk_actor_pose_frame(a, i), 64);
  }
  return out;
}
static int held(BkActorPose *a, const float *data, size_t count) {
  const BkModel *m = bk_actor_pose_model(a);
  if (count != (size_t)m->frame_count * 32)
    return 0;
  for (uint32_t i = 0; i < m->frame_count; ++i)
    if (memcmp(data + i * 32, bk_actor_pose_local(a, i), 64) ||
        memcmp(data + i * 32 + 16, bk_actor_pose_frame(a, i), 64))
      return 0;
  return 1;
}
int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: area-entry-probe DATA\n");
    return 2;
  }
  char error[256] = {0}, path[2048];
  int status = 1;
  unsigned transitions = 0, rejections = 0;
  BkResourceStore *store = bk_resources_create(error),
                  *missing = bk_resources_create(error);
  BkEntryAssets *entry = NULL;
  float *player_frames = NULL, *npc_frames = NULL;
  REQUIRE(store && missing);
  const char *packs[] = {"bk3_00", "bk3_01", "bk3_04"};
  for (unsigned i = 0; i < 3; ++i) {
    REQUIRE(snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) <
            (int)sizeof(path));
    REQUIRE(bk_resources_mount(store, packs[i], path, error));
  }
  REQUIRE(bk_resources_mount_directory(store, "routes", argv[1], 20480, error));
  REQUIRE(
      bk_resources_mount_directory(store, "faces", argv[1], 1048576, error));
  for (unsigned group = 0; group < 5; ++group) {
    BkGameFrameState state;
    BkEntryProgress progress;
    REQUIRE(bk_scene_game_frame_boot_state(&state, &progress, 123));
    BkEntryRequest request = {group, 0, 0, 8};
    entry = bk_entry_assets_create(store, &request, error);
    REQUIRE(entry);
    const uint32_t clocks[] = {10, 20, 30, 40};
    REQUIRE(
        bk_scene_game_frame_initialize_entry(entry, &state, 0, clocks, error));
    REQUIRE(bk_entry_assets_load_player_shadow(entry, store, error));
    REQUIRE(bk_entry_assets_load_mesh_shadow(entry, store, error));
    BkActorPose *player = bk_entry_assets_player(entry),
                *npc = bk_entry_assets_actor(entry);
    REQUIRE(bk_actor_pose_step(player, state.player.spatial.movement.position,
                               state.player.spatial.movement.yaw, 0, .25f,
                               error));
    REQUIRE(bk_actor_pose_step(npc, state.npc.path.position,
                               state.npc.path.yaw_degrees, 1, .25f, error));
    bk_actor_pose_publish(player);
    bk_actor_pose_publish(npc);
    BkClipState player_clip, npc_clip;
    REQUIRE(bk_actor_pose_state(player, &player_clip) &&
            bk_actor_pose_state(npc, &npc_clip));
    size_t player_count, npc_count;
    player_frames = snapshot(player, &player_count);
    npc_frames = snapshot(npc, &npc_count);
    REQUIRE(player_frames && npc_frames);
    BkFaceAssets *face = bk_entry_assets_face(entry);
    BkEyeAssets *eyes = bk_entry_assets_eyes(entry);
    BkNpcShadow *shadow = bk_entry_assets_shadow(entry),
                *player_shadow = bk_entry_assets_player_shadow(entry);
    BkGameFrameState original = state;
    for (unsigned area = 1; area < 9; ++area) {
      BkEntryRequest next = {group, area, 0, 2};
      BkGameFrameState saved = state;
      if (area % 2)
        bk_entry_assets_unload_track(entry);
      const BkRoute *old_route = bk_entry_assets_route(entry);
      BkFollowCamera *old_camera = bk_entry_assets_camera(entry);
      REQUIRE(!bk_entry_assets_advance_area(
          entry, missing, &next, &state.player, &state.npc, &state.npc_vertical,
          &state.player_view, &state.camera, &state.npc_entry_route,
          &state.npc_actions, error));
      REQUIRE(!memcmp(&state, &saved, sizeof(state)) &&
              old_route == bk_entry_assets_route(entry) &&
              old_camera == bk_entry_assets_camera(entry));
      ++rejections;
      error[0] = 0;
      REQUIRE(bk_entry_assets_advance_area(
          entry, store, &next, &state.player, &state.npc, &state.npc_vertical,
          &state.player_view, &state.camera, &state.npc_entry_route,
          &state.npc_actions, error));
      REQUIRE(player == bk_entry_assets_player(entry) &&
              npc == bk_entry_assets_actor(entry) &&
              face == bk_entry_assets_face(entry) &&
              eyes == bk_entry_assets_eyes(entry) &&
              shadow == bk_entry_assets_shadow(entry) &&
              player_shadow == bk_entry_assets_player_shadow(entry));
      REQUIRE(old_route != bk_entry_assets_route(entry) &&
              old_camera != bk_entry_assets_camera(entry));
      REQUIRE(held(player, player_frames, player_count) &&
              held(npc, npc_frames, npc_count));
      BkClipState p, n;
      REQUIRE(bk_actor_pose_state(player, &p) && bk_actor_pose_state(npc, &n));
      REQUIRE(!memcmp(&p, &player_clip, sizeof(p)) &&
              !memcmp(&n, &npc_clip, sizeof(n)));
      REQUIRE(!memcmp(&original.face, &state.face, sizeof(state.face)) &&
              original.random == state.random);
      REQUIRE(!memcmp(&original.npc_entry_route, &state.npc_entry_route,
                      sizeof(state.npc_entry_route)));
      REQUIRE(!memcmp(&original.pickup, &state.pickup, sizeof(state.pickup)));
      REQUIRE(state.camera.phase == (area == 8 ? 3 : 2) &&
              state.npc.path.cursor == 0);
      REQUIRE(bk_entry_assets_request(entry)->area == area);
      state.area = area;
      ++transitions;
    }
    free(player_frames);
    player_frames = NULL;
    free(npc_frames);
    npc_frames = NULL;
    bk_entry_assets_destroy(entry);
    entry = NULL;
  }
  printf(
      "PASS area-entry profiles=40 transitions=%u missing-resource-atomic=%u "
      "actors/poses/clips/face/eyes/shadows/metadata/inventory held\n",
      transitions, rejections);
  status = 0;
done:
  free(player_frames);
  free(npc_frames);
  bk_entry_assets_destroy(entry);
  bk_resources_destroy(missing);
  bk_resources_destroy(store);
  return status;
}
