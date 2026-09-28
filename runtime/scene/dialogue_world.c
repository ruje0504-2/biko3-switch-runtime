#include "scene/dialogue_world.h"
#include "game/draw_dispatch.h"
#include <stdlib.h>
#include <string.h>
struct BkDialogueWorld {
  BkDialogueActorAssets *body;
  BkActorForest *forest;
  BkSceneLighting *lighting;
  BkMenuCamera *camera;
  uint32_t root;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "dialogue world: %s", why);
  return 0;
}
void bk_dialogue_world_destroy(BkDialogueWorld *s) {
  if (!s)
    return;
  bk_actor_forest_destroy(s->forest);
  bk_scene_lighting_destroy(s->lighting);
  bk_dialogue_actor_assets_destroy(s->body);
  free(s);
}
BkDialogueWorld *bk_dialogue_world_create(BkResourceStore *store,
                                          unsigned group, BkMenuCamera *camera,
                                          const uint32_t clocks[4],
                                          uint32_t *random, char e[256]) {
  if (!store || group >= 5 || !camera || !clocks || !random) {
    fail(e, "invalid construction input");
    return NULL;
  }
  BkDialogueWorld *s = calloc(1, sizeof(*s));
  if (!s) {
    fail(e, "allocation failed");
    return NULL;
  }
  s->camera = camera;
  uint32_t rng = *random;
  BkMenuCamera next = *camera;
  s->body = bk_dialogue_actor_assets_create(store, group, clocks, &rng, e);
  if (!s->body || !bk_menu_camera_dialogue(&next))
    goto bad;
  BkActorPose *pose = bk_dialogue_actor_assets_pose(s->body);
  const BkModel *model = bk_actor_pose_model(pose);
  BkActorPlacement placed = *bk_actor_pose_placement(pose);
  /*401074 attaches the loaded model before4efdd8 places its root. Restore
   * that load-time root for attach's mandatory refresh, then apply only the
   * placed root. Children must keep their pre-placement caches until draw. */
  if (!bk_actor_pose_root_local(
          pose, model->frames[bk_dialogue_actor_assets_root(s->body)].local, e))
    goto bad;
  s->forest = bk_actor_forest_create(&pose, 1, e);
  if (!s->forest)
    goto bad;
  s->root = bk_actor_forest_node(s->forest, 0,
                                 bk_dialogue_actor_assets_root(s->body));
  if (!bk_actor_forest_attach(s->forest, 0, s->root, e) ||
      !bk_actor_pose_place_exact(pose, &placed, e) ||
      !bk_actor_forest_anchor(s->forest, 1, next.pose.world, 0, e))
    goto bad;
  s->lighting = bk_scene_lighting_create(model, bk_actor_pose_frame(pose, 0),
                                         (size_t)model->frame_count * 16, e);
  if (!s->lighting)
    goto bad;
  *camera = next;
  *random = rng;
  return s;
bad:
  bk_dialogue_world_destroy(s);
  return NULL;
}
BkDialogueActorAssets *bk_dialogue_world_body(BkDialogueWorld *s) {
  return s ? s->body : NULL;
}
BkActorForest *bk_dialogue_world_forest(BkDialogueWorld *s) {
  return s ? s->forest : NULL;
}
BkSceneLighting *bk_dialogue_world_lighting(BkDialogueWorld *s) {
  return s ? s->lighting : NULL;
}
uint32_t bk_dialogue_world_root(const BkDialogueWorld *s) {
  return s ? s->root : BK_FRAME_NONE;
}
int bk_dialogue_world_step(BkDialogueWorld *s, BkDialogueActorState *actor,
                           BkDialogue *d, uint8_t *phase, BkTimer *timer,
                           const BkDialogueActorInput *in, uint32_t *rng,
                           char e[256]) {
  if (!s || !in)
    return fail(e, "missing frame input");
  if (!bk_menu_camera_dialogue(s->camera) ||
      !bk_actor_forest_anchor(s->forest, 1, s->camera->pose.world, 0, e))
    return 0;
  BkDialogueActorInput input = *in;
  memcpy(input.camera_world, s->camera->pose.world, sizeof(input.camera_world));
  return bk_dialogue_actor_assets_step(s->body, actor, d, phase, timer, &input,
                                       rng, e);
}
int bk_dialogue_world_pass(BkDialogueWorld *s, BkLightingPass *out,
                           char e[256]) {
  BkDrawDispatch selected;
  BkLightingPassInput input = {0};
  if (!s || !out ||
      !bk_draw_dispatch_select(
          &(BkDrawDispatchInput){.flow = 8, .root_733e28 = s->root},
          &selected) ||
      !bk_draw_dispatch_lighting(&selected, &input) ||
      !bk_scene_lighting_input(s->lighting, &input) ||
      !bk_lighting_pass(&input, out))
    return fail(e, "invalid light/root dispatch");
  return 1;
}
