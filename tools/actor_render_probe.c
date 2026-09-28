/* Real actors rendered in an explicit inspection camera, not a game loop. */
#include "scene/actor_render.h"
#include "scene/entry_assets.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define WIDTH 256
#define HEIGHT 256
static int mount(BkResourceStore *store, const char *directory,
                 const char *pack, char error[256]) {
  char path[1024];
  if (snprintf(path, sizeof(path), "%s/%s.pp", directory, pack) >=
      (int)sizeof(path))
    return 0;
  return bk_resources_mount(store, pack, path, error);
}
static int image(BkRenderer *r, uint8_t *pixels, int empty, char error[256]) {
  if (!bk_renderer_readback(r, pixels, WIDTH * HEIGHT * 4, error))
    return 0;
  unsigned count = 0;
  for (unsigned i = 0; i < WIDTH * HEIGHT; i++)
    count += pixels[i * 4] || pixels[i * 4 + 1] || pixels[i * 4 + 2];
  if (empty ? count != 0 : count < 100) {
    snprintf(error, 256, "actor image: %u colored pixels, expected %s", count,
             empty ? "zero" : "at least100");
    return 0;
  }
  return 1;
}
static int draw(BkActorRender *actor, BkRenderer *r, BkLightSet *lights,
                char error[256]) {
  return bk_renderer_begin(r, error) &&
         bk_actor_render_draw(actor, lights, error) &&
         bk_renderer_end(r, error);
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr,
            "usage: actor-render-probe DATA_DIRECTORY [OUTPUT_PREFIX]\n");
    return 2;
  }
  char error[256] = {0};
  int ok = 0;
  BkResourceStore *store = bk_resources_create(error);
  BkRenderer *r = NULL;
  BkLightSet *lights = NULL;
  BkEntryAssets *entry = NULL;
  BkActorRender *render = NULL;
  BkMaterialPose *materials = NULL;
  uint8_t *pixels = malloc(WIDTH * HEIGHT * 4),
          *first = malloc(WIDTH * HEIGHT * 4);
  if (!store || !pixels || !first)
    goto done;
  if (!mount(store, argv[1], "bk3_01", error) ||
      !mount(store, argv[1], "bk3_04", error) ||
      !bk_resources_mount_directory(store, "routes", argv[1], 20480, error) ||
      !bk_resources_mount_directory(store, "faces", argv[1], 20480, error))
    goto done;
  r = bk_renderer_create(WIDTH, HEIGHT, stderr, error);
  if (!r)
    goto done;
  /* Explicit neutral fixture lighting; not the game's per-pass light policy. */
  BkLighting lighting = {.ambient = {.65f, .65f, .65f}};
  lights = bk_light_set_create(r, &lighting, error);
  if (!lights)
    goto done;
  unsigned cases = 0, frames = 0, alternate_changes = 0, gaze_changes = 0,
           gaze_profiles = 0;
  for (unsigned group = 0; group < 5; group++)
    for (unsigned area = 0; area < 9; area++) {
      BkEntryRequest request = {group, area, 0, 8};
      entry = bk_entry_assets_create(store, &request, error);
      if (!entry)
        goto done;
      BkActorPose *pose = bk_entry_assets_actor(entry);
      const BkModel *model = bk_actor_pose_model(pose);
      BkFaceAssets *face = bk_entry_assets_face(entry);
      BkEyeAssets *eyes = bk_entry_assets_eyes(entry);
      render = bk_actor_render_create(r, store, "bk3_01", model, eyes, error);
      materials = bk_material_pose_create(model, error);
      if (!render || !materials)
        goto done;
      BkActorPlacement placement = *bk_actor_pose_placement(pose);
      bk_actor_pose_publish(pose);
      float height = bk_actor_pose_head(pose)[1] - placement.position[1];
      if (!(height > 1))
        goto done;
      float camera[16], target[3], view[16], projection[16];
      memcpy(camera, bk_identity, sizeof(camera));
      memcpy(target, placement.position, sizeof(target));
      target[1] += height * .5f;
      memcpy(camera + 12, target, sizeof(target));
      camera[14] -= height * 2;
      BkCameraLens lens = {1, 1, .5f, 10000};
      if (!bk_camera_aim(camera, camera, target) ||
          !bk_camera_view(view, camera) ||
          !bk_camera_projection(projection, &lens) ||
          !bk_light_set_view(r, lights, camera + 12, error))
        goto done;
      uint32_t random = 1, clocks[] = {1000, 1001, 1002, 1003};
      BkFaceState face_state;
      if (!bk_face_assets_initialize(face, &face_state, clocks, &random, error))
        goto done;
      int changed = 0;
      for (unsigned frame = 0; frame < 12; frame++) {
        uint32_t now = 1100 + frame * 137;
        if (!bk_actor_pose_step(pose, placement.position, placement.yaw_degrees,
                                -1, .5f / 30, error) ||
            !bk_face_assets_step(face, &face_state, 0, (float)(frame % 10), now,
                                 now + 1, now + 2, now + 3, &random, error) ||
            !bk_eye_assets_gaze(eyes, pose, (int8_t)(frame % 3), camera, .25f,
                                .4f, error))
          goto done;
        bk_actor_pose_publish(pose);
        if (!bk_actor_render_prepare(render, pose, materials, face, view,
                                     projection, error) ||
            !draw(render, r, lights, error) || !image(r, pixels, 0, error))
          goto done;
        if (!frame)
          memcpy(first, pixels, WIDTH * HEIGHT * 4);
        else
          changed |= memcmp(first, pixels, WIDTH * HEIGHT * 4) != 0;
        if (argc == 3 && area == 8 && (frame == 0 || frame == 11)) {
          char path[1024];
          if (snprintf(path, sizeof(path), "%s-g%u-f%02u.rgba", argv[2], group,
                       frame) >= (int)sizeof(path))
            goto done;
          FILE *out = fopen(path, "wb");
          if (!out)
            goto done;
          size_t n = fwrite(pixels, 4, WIDTH * HEIGHT, out);
          int closed = fclose(out);
          if (n != WIDTH * HEIGHT || closed)
            goto done;
        }
        frames++;
      }
      if (!changed) {
        snprintf(error, 256, "actor image never changed: group%u area%u", group,
                 area);
        goto done;
      }
      /* Same frozen pose, only the original eye target's texture changes. */
      memcpy(first, pixels, WIDTH * HEIGHT * 4);
      uint32_t wanted = bk_eye_assets_image(eyes, 1, NULL) ? 1 : 0;
      if (!bk_eye_assets_select(eyes, 1, error) ||
          bk_eye_assets_selected(eyes) != wanted ||
          !bk_eye_assets_select(eyes, 4, error) ||
          bk_eye_assets_selected(eyes) != wanted ||
          bk_eye_assets_select(eyes, 5, error) ||
          bk_eye_assets_selected(eyes) != wanted ||
          !bk_actor_render_prepare(render, pose, materials, face, view,
                                   projection, error) ||
          !draw(render, r, lights, error) || !image(r, pixels, 0, error))
        goto done;
      alternate_changes += memcmp(first, pixels, WIDTH * HEIGHT * 4) != 0;
      if (!bk_eye_assets_select(eyes, 0, error) ||
          bk_eye_assets_selected(eyes) != 0 ||
          !bk_actor_render_prepare(render, pose, materials, face, view,
                                   projection, error) ||
          !draw(render, r, lights, error) || !image(r, pixels, 0, error) ||
          memcmp(first, pixels, WIDTH * HEIGHT * 4)) {
        snprintf(error, 256, "eye texture base restoration differs");
        goto done;
      }
      const BkEyeBinding *binding = bk_eye_assets_binding(eyes);
      if (binding->frames[0] != BK_MODEL_NONE &&
          binding->frames[1] != BK_MODEL_NONE) {
        /* Freeze timeline/face vertices; only the two gaze variants change.
         * Real phase dispatch is covered by the CPU oracle, not inferred from
         * this inspection camera. */
        if (!bk_eye_assets_gaze(eyes, pose, 0, camera, .25f, .4f, error))
          goto done;
        bk_actor_pose_publish(pose);
        if (!bk_actor_render_prepare(render, pose, materials, face, view,
                                     projection, error) ||
            !draw(render, r, lights, error) || !image(r, first, 0, error))
          goto done;
        if (!bk_eye_assets_gaze(eyes, pose, 1, camera, .25f, .4f, error))
          goto done;
        bk_actor_pose_publish(pose);
        if (!bk_actor_render_prepare(render, pose, materials, face, view,
                                     projection, error) ||
            !draw(render, r, lights, error) || !image(r, pixels, 0, error))
          goto done;
        gaze_changes += memcmp(first, pixels, WIDTH * HEIGHT * 4) != 0;
        gaze_profiles++;
      }
      BkActorRenderStats stats;
      if (!bk_actor_render_stats(render, &stats) || !stats.skinned_parts ||
          !stats.morph_parts || !stats.visible_instances)
        goto done;
      uint32_t root = BK_MODEL_NONE;
      for (uint32_t i = 0; i < model->frame_count; i++)
        if (model->frames[i].parent_index == BK_MODEL_NONE)
          root = i;
      BkActorVisibilityEdit hide = {root, 1};
      if (!bk_actor_pose_visibility(pose, &hide, 1, error) ||
          !bk_actor_render_prepare(render, pose, materials, face, view,
                                   projection, error) ||
          !bk_actor_render_stats(render, &stats) || stats.submitted_instances ||
          !draw(render, r, lights, error) || !image(r, pixels, 1, error))
        goto done;
      hide.hidden = 0;
      BkMaterialAlphaEdit transparent = {root, 0, NULL, 0};
      if (!bk_actor_pose_visibility(pose, &hide, 1, error) ||
          !bk_material_pose_alpha(materials, &transparent, 1, error) ||
          !bk_actor_render_prepare(render, pose, materials, face, view,
                                   projection, error) ||
          !bk_actor_render_stats(render, &stats) ||
          !stats.submitted_instances || stats.visible_instances ||
          !draw(render, r, lights, error) || !image(r, pixels, 1, error))
        goto done;
      float invalid[16];
      memcpy(invalid, projection, sizeof(invalid));
      invalid[15] = NAN;
      if (bk_actor_render_prepare(render, pose, NULL, face, view, invalid,
                                  error) ||
          bk_actor_render_stats(render, &stats))
        goto done;
      if (!bk_actor_render_prepare(render, pose, NULL, face, view, projection,
                                   error) ||
          !draw(render, r, lights, error) || !image(r, pixels, 0, error))
        goto done;
      printf("PASS actor group%u area%u:12 moving frames, skin/face, hidden "
             "subtree, "
             "zero-alpha queue, failed snapshot/recovery\n",
             group, area);
      fflush(stdout);
      cases++;
      bk_actor_render_destroy(render);
      render = NULL;
      bk_material_pose_destroy(materials);
      materials = NULL;
      bk_entry_assets_destroy(entry);
      entry = NULL;
    }
  if (!alternate_changes || !gaze_changes) {
    snprintf(error, 256,
             "no visible eye texture/gaze change across all profiles");
    goto done;
  }
  printf("PASS %u real actor entry profiles, %u animated Vulkan readbacks, %u "
         "visible eye replacements, %u gaze profiles/%u visible gaze changes\n",
         cases, frames, alternate_changes, gaze_profiles, gaze_changes);
  ok = 1;
done:
  if (!ok)
    fprintf(stderr, "actor render probe FAILED: %s\n", error);
  bk_actor_render_destroy(render);
  bk_material_pose_destroy(materials);
  bk_entry_assets_destroy(entry);
  bk_light_set_destroy(r, lights);
  bk_renderer_destroy(r);
  bk_resources_destroy(store);
  free(pixels);
  free(first);
  return ok ? 0 : 1;
}
