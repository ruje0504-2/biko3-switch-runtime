/* Real CPU assets and production special-scene adapter. Draw callbacks run
 * forest publication and inspect state; they do not claim GPU rendering. */
#include "scene/ending_normal_assets.h"
#include "scene/ending_special_scene.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line %d: %s\n", __LINE__, e);                           \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static const float I[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
typedef struct {
  BkActorForest *forest;
  uint64_t hash;
  unsigned draws, events, walks, calls, fail_at;
} Probe;
static void hash(Probe *p, const void *data, size_t n) {
  const unsigned char *b = data;
  while (n--)
    p->hash = (p->hash ^ *b++) * UINT64_C(1099511628211);
}
static int stage(Probe *p, char e[256]) {
  if (++p->calls != p->fail_at)
    return 1;
  snprintf(e, 256, "intentional renderer boundary failure");
  return 0;
}
static int snapshot(Probe *p, char e[256]) {
  const BkFrameTree *tree = bk_actor_forest_tree(p->forest);
  for (uint32_t n = 0; n < bk_frame_tree_count(tree); n++)
    hash(p, bk_actor_forest_world(p->forest, n), 64);
  hash(p, bk_actor_forest_view(p->forest), 64);
  BkNodeReference ref;
  if (!bk_actor_forest_anchor_reference(p->forest, 1, &ref, e))
    return 0;
  hash(p, &ref, sizeof(ref));
  return 1;
}
static int draw(void *ctx, const BkDrawDispatch *d, const BkMenuCamera *c,
                char e[256]) {
  Probe *p = ctx;
  if (!stage(p, e))
    return 0;
  if (memcmp(c->pose.world, bk_actor_forest_world(p->forest, 1), 64))
    return 0;
  p->draws++;
  hash(p, d, sizeof(*d));
  for (unsigned i = d->mode == 1 ? 0 : 20; i < (d->mode == 1 ? 20 : 52); i++)
    if (d->objects[i]) {
      const BkFrameVisit *visits;
      uint32_t count;
      if (!bk_actor_forest_draw(p->forest, d->objects[i], &visits, &count, e))
        return 0;
      for (uint32_t j = 0; j < count; j++) {
        hash(p, &visits[j].node, sizeof(visits[j].node));
        hash(p, &visits[j].submit, sizeof(visits[j].submit));
      }
      p->walks++;
    }
  return snapshot(p, e);
}
static int render(void *ctx, BkEndingSpecialRenderEvent event, unsigned arg,
                  char e[256]) {
  Probe *p = ctx;
  if (!stage(p, e))
    return 0;
  p->events++;
  hash(p, &event, sizeof(event));
  hash(p, &arg, sizeof(arg));
  return snapshot(p, e);
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkResourceStore *store = bk_resources_create(e);
  BkEndingNormalAssets *owner = NULL;
  BkMaterialPose *material = NULL;
  Probe p = {.hash = UINT64_C(14695981039346656037)};
  unsigned frames = 0, failures = 0;
  CHECK(store);
  const char *packs[] = {"bk3_08", "bk3_03", "bk3_04", "fambom"};
  for (unsigned i = 0; i < 4; i++) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  for (unsigned g = 0; g < 5; g++)
    for (unsigned v = 0; v < 2; v++) {
      BkMenuCamera camera = {0};
      BkEndingCameraPresets presets;
      memcpy(camera.pose.world, I, 64);
      memcpy(camera.matrix, I, 64);
      uint32_t rng = 123;
      owner = bk_ending_normal_assets_create(store, g, v,
                                             (uint32_t[]){100, 110, 120, 130},
                                             &rng, &camera, &presets, e);
      CHECK(owner && bk_ending_normal_assets_load_background(owner, store, e));
      p.forest = bk_ending_normal_assets_forest(owner);
      BkActorPose *poses[5];
      uint32_t roots[5];
      for (unsigned i = 0; i < 5; i++) {
        poses[i] = bk_ending_normal_assets_pose(owner, i);
        const BkModel *m = bk_actor_pose_model(poses[i]);
        roots[i] = BK_FRAME_NONE;
        for (uint32_t f = 0; f < m->frame_count; f++)
          if (m->frames[f].parent_index == BK_MODEL_NONE)
            roots[i] = bk_actor_forest_node(p.forest, i, f);
        CHECK(roots[i] != BK_FRAME_NONE);
      }
      material = bk_material_pose_create(bk_actor_pose_model(poses[0]), e);
      CHECK(material);
      BkEndingSpecialMaterial binding = {material, 0};
      BkEndingSpecialScene scene = {p.forest, &camera, &binding, 1,
                                    &p,       draw,    render};
      float table[BK_ENDING_SPECIAL_CAMERAS][4];
      CHECK(bk_ending_special_cameras(table, g));
      BkEndingFrameState f = {.group = g};
      int32_t action = v, index = 0, mode = 0;
      uint8_t variant = 0, restore = 0;
      uint32_t head = bk_actor_forest_node(
          p.forest, 0, bk_ending_normal_assets_node(owner, 0));
      BkEndingSpecialBindings b = {
          &f,        &action,
          &variant,  &restore,
          &index,    &mode,
          table,     bk_actor_forest_world(p.forest, head) + 12,
          &roots[0], &roots[1]};
      BkDrawDispatch descriptor = {0};
      descriptor.mode = 1;
      descriptor.objects[0] = roots[4];
      descriptor.objects[4] = roots[0];
      descriptor.objects[5] = roots[1];
      BkDrawDispatch saved = descriptor;
      BkMenuCamera old = camera;
      unsigned calls = p.calls;
      scene.draw = NULL;
      CHECK(!bk_ending_special_scene_draw(&scene, &b, &descriptor, e));
      failures++;
      scene.draw = draw;
      binding.index = UINT32_MAX;
      CHECK(!bk_ending_special_scene_draw(&scene, &b, &descriptor, e));
      failures++;
      CHECK(p.calls == calls && !memcmp(&saved, &descriptor, sizeof(saved)) &&
            !memcmp(&old, &camera, sizeof(old)));
      binding.index = 0;
      for (unsigned step = 0; step < 120; step++) {
        for (unsigned i = 0; i < 5; i++)
          CHECK(bk_actor_pose_advance(poses[i], -1, step % 3 ? .016f : 0, e));
        CHECK(bk_bom_assets_advance(bk_ending_normal_assets_bom(owner), .016f,
                                    e));
        CHECK(bk_bom_assets_follow_references(
            bk_ending_normal_assets_bom(owner), e));
        for (unsigned i = 0; i < 2; i++)
          CHECK(bk_actor_forest_visibility(p.forest, roots[i], 0, e));
        f.phase = (int[]){1, 3, 4, 5, 6, 8}[step % 6];
        f.state_721ee0 = (int[]){4, 6, 7, 8}[step % 4];
        variant = step % 10;
        restore = (uint8_t[]){0, 1, 255}[step % 3];
        index = (step * 7) % 108;
        mode = step % 3;
        if (step % 5 == 0) {
          index = g == 4 ? 47 : 104;
          mode = g == 4 ? 1 : 2;
        }
        descriptor = saved;
        old = camera;
        CHECK(bk_ending_special_scene_draw(&scene, &b, &descriptor, e));
        CHECK(descriptor.mode == 2 && descriptor.objects[20] == roots[0]);
        CHECK(descriptor.objects[21] ==
              ((f.phase == 1 || f.phase == 8) ? roots[1] : 0));
        CHECK(!memcmp((const char *)&old + 64, (const char *)&camera + 64,
                      sizeof(camera) - 64));
        CHECK(
            !memcmp(camera.pose.world, bk_actor_forest_world(p.forest, 1), 64));
        frames++;
      }
      /* Fail after the first actual draw. Prefix is retained, no second draw or
       * later renderer event runs. Destroy this aborted owner immediately. */
      unsigned draws = p.draws, events = p.events;
      p.fail_at = p.calls + 2;
      descriptor = saved;
      CHECK(!bk_ending_special_scene_draw(&scene, &b, &descriptor, e));
      failures++;
      CHECK(p.draws == draws + 1 && p.events == events && descriptor.mode == 1);
      p.fail_at = 0;
      bk_material_pose_destroy(material);
      material = NULL;
      bk_ending_normal_assets_destroy(owner);
      owner = NULL;
    }
  printf("{\"passed\":true,\"profiles\":10,\"frames\":%u,\"draws\":%u,\"render_"
         "boundaries\":%u,\"walks\":%u,\"failures\":%u,\"hash\":\"%016llx\"}\n",
         frames, p.draws, p.events, p.walks, failures,
         (unsigned long long)p.hash);
  rc = 0;
done:
  bk_material_pose_destroy(material);
  bk_ending_normal_assets_destroy(owner);
  bk_resources_destroy(store);
  return rc;
}
