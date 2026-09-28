/* Explicit asset inspection: real dialogue bodies/faces/fixed camera,
 * fixture ambient light/envelope; not complete flow8 or actual voice playback.
 */
#include "scene/actor_render.h"
#include "scene/dialogue_actor_assets.h"
#include "world/menu_camera.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#define W 320
#define H 240
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char error[256] = {0}, path[1024];
  int rc = 1;
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *r = NULL;
  BkLightSet *light = NULL;
  BkDialogueActorAssets *actor = NULL;
  BkActorRender *render = NULL;
  uint8_t *pixels = malloc(W * H * 4);
  uint64_t hash = 1469598103934665603ull;
  unsigned frames = 0, hidden = 0;
  CHECK(store && pixels);
  snprintf(path, sizeof(path), "%s/bk3_01.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_01", path, error));
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  r = bk_renderer_create(W, H, stderr, error);
  CHECK(r);
  BkLighting lighting = {.ambient = {.65f, .65f, .65f}};
  light = bk_light_set_create(r, &lighting, error);
  CHECK(light);
  BkMenuCamera camera = {0};
  CHECK(bk_menu_camera_dialogue(&camera));
  float view[16], projection[16];
  CHECK(bk_camera_view(view, camera.pose.world));
  CHECK(bk_camera_projection(projection,
                             &(BkCameraLens){camera.fov, .75f, .5f, 126384}));
  CHECK(bk_light_set_view(r, light, camera.pose.world + 12, error));
  for (unsigned group = 0; group < 5; group++) {
    uint32_t seed = 98765, clocks[4] = {1000, 1001, 1002, 1003};
    actor = bk_dialogue_actor_assets_create(store, group, clocks, &seed, error);
    CHECK(actor);
    BkActorPose *pose = bk_dialogue_actor_assets_pose(actor);
    render =
        bk_actor_render_create(r, store, "bk3_01", bk_actor_pose_model(pose),
                               bk_dialogue_actor_assets_eyes(actor), error);
    CHECK(render);
    BkDialogueActorState state = {0};
    CHECK(bk_dialogue_actor_initialize(&state, group));
    BkDialogue dialogue = {.code_c = group + 1, .code_e = 9};
    uint8_t phase = 0;
    BkTimer timer = {0};
    for (unsigned f = 0; f < 120; f++) {
      BkDialogueActorInput input = {
          .game_seconds = .25f,
          .mouth_level = f % 10,
          .timestamp_ms = 1100 + f * 137,
          .timer_clock_ms = 1101 + f * 137,
          .face_clocks = {1102 + f * 137, 1103 + f * 137, 1104 + f * 137}};
      memcpy(input.camera_world, camera.pose.world, 64);
      if (f % 30 == 0) {
        dialogue.code_f = (f / 30) * 10 + (f / 30);
        dialogue.code_m = 9 + f / 30;
        phase = 0;
      }
      if (f % 30 == 20) {
        state.rules.expression = -1;
        phase = 1;
      }
      if (f % 30 == 25) {
        state.rules.expression = 2;
        phase = 5;
      }
      CHECK(bk_dialogue_actor_assets_step(actor, &state, &dialogue, &phase,
                                          &timer, &input, &seed, error));
      bk_actor_pose_publish(pose);
      CHECK(bk_actor_render_prepare(render, pose, NULL,
                                    bk_dialogue_actor_assets_face(actor), view,
                                    projection, error));
      CHECK(bk_renderer_begin(r, error) &&
            bk_actor_render_draw(render, light, error) &&
            bk_renderer_end(r, error));
      CHECK(bk_renderer_readback(r, pixels, W * H * 4, error));
      unsigned colored = 0;
      for (unsigned i = 0; i < W * H; i++)
        colored += pixels[i * 4] || pixels[i * 4 + 1] || pixels[i * 4 + 2];
      if (state.visibility <= 0) {
        CHECK(!colored);
        hidden++;
      } else
        CHECK(colored > 100);
      for (unsigned i = 0; i < W * H * 4; i++) {
        hash ^= pixels[i];
        hash *= 1099511628211ull;
      }
      frames++;
    }
    bk_actor_render_destroy(render);
    render = NULL;
    bk_dialogue_actor_assets_destroy(actor);
    actor = NULL;
  }
  printf("PASS dialogue GPU:5 actors %u frames %u hidden; RGBA %016" PRIx64
         "\n",
         frames, hidden, hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_actor_render_destroy(render);
  bk_dialogue_actor_assets_destroy(actor);
  bk_light_set_destroy(r, light);
  bk_renderer_destroy(r);
  bk_resources_destroy(store);
  free(pixels);
  return rc;
}
