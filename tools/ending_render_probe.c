/* Numeric GPU buffers only: production normal assets and regular draw path.
 * Explicit component/input fixtures do not implement the ending controller. */
#include "model/skin.h"
#include "scene/ending_normal_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line%d: %s: %s\n", __LINE__, #x, e);                    \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static const float I[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static uint64_t vertices;
static unsigned frames, queues, repeats, empty, movie_changes;
static double worst;
static int special, retained;
static unsigned equivalent_targets, equivalent_meshes;
static int apply_queues(BkEndingNormalRender *render, unsigned first,
                        unsigned end, BkBomAssets *bom, BkBomDeform *reference,
                        BkBomMeshView *views, unsigned mesh_count,
                        const BkBomDeformBinding *plans, unsigned count,
                        const int32_t *disabled, int tally, int seen[8],
                        char e[256]) {
  for (unsigned p = first; p < end; p++)
    for (uint32_t j = 0; j < bk_ending_normal_render_queue_count(render, p);
         j++) {
      uint32_t actor, frame, submesh;
      if (!bk_ending_normal_render_queue_item(render, p, j, &actor, &frame,
                                              &submesh))
        return 0;
      if (tally)
        queues++;
      for (unsigned i = 0; i < mesh_count; i++) {
        const BkBomAssetMesh *mesh = bk_bom_assets_mesh(bom, i);
        if (mesh->actor != actor || mesh->submesh != submesh)
          continue;
        if (seen)
          seen[i] = 1;
        int target = 0;
        for (unsigned k = 0; k < count; k++)
          target |= plans[k].target == i;
        if (target &&
            !bk_bom_deform_draw(reference, i, views, mesh_count, disabled, e))
          return 0;
      }
    }
  return 1;
}
static int compare(const BkLitVertex *got, const BkModelVertex *want,
                   unsigned count) {
  for (unsigned i = 0; i < count; i++) {
    float position[] = {got[i].base.x, got[i].base.y, got[i].base.z};
    for (unsigned j = 0; j < 6; j++) {
      float a = j < 3 ? position[j] : got[i].normal[j - 3];
      float b = j < 3 ? want[i].position[j] : want[i].normal[j - 3];
      double difference = fabs((double)a - b) / fmax(1, fabs(b));
      if (difference > worst)
        worst = difference;
      if (!isfinite(difference) || difference > 3e-5)
        return 0;
    }
    vertices++;
  }
  return 1;
}
static int profile(BkRenderer *renderer, BkResourceStore *store,
                   BkResourceStore *missing_movie, unsigned group,
                   unsigned variant, char e[256]) {
  int ok = 0;
  BkEndingNormalAssets *assets = NULL;
  BkEndingSecondaryAssets *previous = NULL;
  BkEndingNormalRender *render = NULL;
  BkMaterialPose *material = NULL;
  BkModelSkin *skin[2] = {0};
  BkSkinMesh *skin_mesh[8] = {0};
  BkBomMeshView views[8] = {0};
  BkModelVertex *base[8] = {0}, *first[8] = {0}, *native[8] = {0};
  BkBomDeformBinding plans[4];
  BkBomDeform *reference = NULL;
  BkLitVertex *readback = NULL;
  float *old_source_world = NULL;
  BkActorPose *poses[3];
  const BkModel *models[3];
  unsigned mesh_count = 0, max = 0;
  uint32_t rng = 123, clocks[] = {100, 110, 120, 130};
  BkMenuCamera camera = {.fov = 1};
  memcpy(camera.pose.world, I, 64);
  memcpy(camera.matrix, I, 64);
  BkEndingCameraPresets presets;
  if (retained) {
    previous = bk_ending_secondary_assets_create(store, group, variant, clocks,
                                                  &rng, &camera, &presets, e);
    CHECK(previous && bk_ending_secondary_assets_load_background(previous, store, e));
    assets = bk_ending_normal_assets_create_reloaded(store, group, variant,
        bk_ending_secondary_assets_background(previous), clocks, &rng,
        &camera, &presets, e);
  } else
    assets = bk_ending_normal_assets_create(store, group, variant, clocks, &rng,
                                            &camera, &presets, e);
  CHECK(assets);
  bk_ending_secondary_assets_destroy(previous);
  previous = NULL;
  CHECK(bk_ending_normal_assets_load_background(assets, store, e));
  BkActorForest *forest = bk_ending_normal_assets_forest(assets);
  BkBomAssets *bom = bk_ending_normal_assets_bom(assets);
  uint32_t roots[3];
  for (unsigned i = 0; i < 3; i++) {
    unsigned actor = i == 2 ? 4 : i;
    poses[i] = bk_ending_normal_assets_pose(assets, actor);
    models[i] = bk_actor_pose_model(poses[i]);
    roots[i] = BK_MODEL_NONE;
    for (uint32_t f = 0; f < models[i]->frame_count; f++)
      if (models[i]->frames[f].parent_index == BK_MODEL_NONE)
        roots[i] = f;
    CHECK(roots[i] != BK_MODEL_NONE);
    if (i < 2 && bk_model_chunk(models[i], "ENVL")) {
      skin[i] = bk_model_skin_create(models[i], e);
      CHECK(skin[i]);
    }
  }
  int staged = retained && group != 1;
  if (staged) {
    old_source_world = malloc((size_t)models[1]->frame_count * 64);
    CHECK(old_source_world);
  }
  mesh_count = bk_bom_assets_mesh_count(bom);
  CHECK(mesh_count <= 8);
  for (unsigned i = 0; i < mesh_count; i++) {
    const BkBomAssetMesh *mesh = bk_bom_assets_mesh(bom, i);
    const BkModelSubmesh *sub = &models[mesh->actor]->submeshes[mesh->submesh];
    views[i].count = sub->vertex_count;
    views[i].vertices =
        malloc((size_t)sub->vertex_count * sizeof(BkModelVertex));
    CHECK(views[i].vertices);
    base[i] = malloc((size_t)sub->vertex_count * sizeof(BkModelVertex));
    first[i] = malloc((size_t)sub->vertex_count * sizeof(BkModelVertex));
    native[i] = malloc((size_t)sub->vertex_count * sizeof(BkModelVertex));
    CHECK(base[i] && first[i] && native[i]);
    memcpy(views[i].vertices, sub->vertices,
           (size_t)sub->vertex_count * sizeof(BkModelVertex));
    views[i].world = bk_actor_pose_frame(poses[mesh->actor], mesh->frame);
    if (sub->vertex_count > max)
      max = sub->vertex_count;
    for (uint32_t j = 0; j < bk_model_skin_count(skin[mesh->actor]); j++)
      if (bk_model_skin_entry(skin[mesh->actor], j)->submesh == mesh->submesh) {
        skin_mesh[i] = bk_skin_mesh_create(skin[mesh->actor], j, sub, e);
        CHECK(skin_mesh[i]);
      }
  }
  unsigned count = bk_bom_assets_count(bom);
  for (unsigned i = 0; i < count; i++)
    CHECK(bk_bom_assets_plan(bom, i, &plans[i]));
  reference = bk_bom_deform_create(views, mesh_count, plans, count, e);
  CHECK(reference);
  material = bk_material_pose_create(models[0], e);
  CHECK(material);
  readback = malloc((size_t)max * sizeof(*readback));
  CHECK(readback);
  BkRenderStats before_create = bk_renderer_stats(renderer);
  render =
      bk_ending_normal_render_create(renderer, missing_movie, assets, 1000, e);
  CHECK(!render);
  BkRenderStats failed_create = bk_renderer_stats(renderer);
  CHECK(failed_create.live_allocations == before_create.live_allocations &&
        failed_create.live_bytes == before_create.live_bytes);
  render = (special ? bk_ending_normal_render_create_special
                    : bk_ending_normal_render_create)(renderer, store, assets,
                                                      1000, e);
  CHECK(render);
  CHECK(bk_ending_normal_render_movie_frame(render) == UINT32_MAX);
  const BkImage *movie = bk_ending_normal_render_movie_image(render);
  CHECK(movie && movie->width == 256 && movie->height == 256);
  for (unsigned i = 0; i < 256 * 256; i++)
    CHECK(movie->rgba[i * 4] == 0 && movie->rgba[i * 4 + 3] == 255);
  CHECK(bk_ending_normal_render_movie_step(render, 1040, 1041, e));
  uint32_t previous_movie = bk_ending_normal_render_movie_frame(render);
  CHECK(previous_movie != UINT32_MAX);
  BkRenderStats baseline = {0};
  float special_cameras[BK_ENDING_SPECIAL_CAMERAS][4];
  CHECK(bk_ending_special_cameras(special_cameras, group));
  BkEndingFrameState ending = {.group = group};
  int32_t action = variant, index = 0, offset = 0;
  uint8_t camera_variant = 0, restore_hidden = 0;
  uint32_t primary = bk_actor_forest_node(forest, 0, roots[0]),
           aux = bk_actor_forest_node(forest, 1, roots[1]);
  uint32_t head =
      bk_actor_forest_node(forest, 0, bk_ending_normal_assets_node(assets, 0));
  BkEndingSpecialBindings bindings = {
      &ending,         &action,
      &camera_variant, &restore_hidden,
      &index,          &offset,
      special_cameras, bk_actor_forest_world(forest, head) + 12,
      &primary,        &aux};
  for (unsigned step = 0; step < 32; step++) {
    float dt = step % 5 ? .016f : 0;
    CHECK(bk_actor_pose_advance(poses[0], -1, dt, e));
    CHECK(bk_actor_pose_advance(poses[2], -1, dt, e));
    CHECK(bk_bom_assets_advance(bom, dt, e));
    CHECK(bk_bom_assets_follow_references(bom, e));
    BkActorVisibilityEdit edits[] = {{roots[0], step % 8 == 3},
                                     {roots[1], step % 8 == 5}};
    for (unsigned i = 0; i < 2; i++)
      CHECK(bk_actor_pose_visibility(poses[i], &edits[i], 1, e));
    BkMaterialAlphaEdit alpha = {.frame = roots[0],
                                 .alpha = step % 8 == 4 ? 0 : 1};
    CHECK(bk_material_pose_alpha(material, &alpha, 1, e));
    CHECK(bk_face_assets_step(bk_ending_normal_assets_face(assets),
                              bk_ending_normal_assets_face_state(assets), 9,
                              step % 100, 1000 + step * 17, 1001 + step * 17,
                              1002 + step * 17, 1003 + step * 17, &rng, e));
    CHECK(bk_ending_normal_render_movie_step(render, 1080 + (int32_t)step * 40,
                                             1081 + (int32_t)step * 40, e));
    uint32_t movie_frame = bk_ending_normal_render_movie_frame(render);
    movie_changes += movie_frame != previous_movie;
    previous_movie = movie_frame;
    BkDrawDispatchInput in = {
        .flow = 16,
        .group = (int32_t)group,
        .event_state = step % 8 == 7 ? 7
                       : step % 2    ? 1
                                     : 2,
        .root_721b28 = bk_actor_forest_node(forest, 0, roots[0]),
        .root_721b2c = bk_actor_forest_node(forest, 1, roots[1]),
        .root_721b34 = bk_actor_forest_node(forest, 4, roots[2])};
    BkDrawDispatch dispatch;
    if (special)
      in.event_state = (int[]){1, 3, 4, 5, 6, 8, 2, 7}[step % 8];
    CHECK(bk_draw_dispatch_select(&in, &dispatch));
    int32_t disabled[4];
    for (unsigned i = 0; i < count; i++)
      disabled[i] = (int32_t[]){0, 1, 2, -1}[(step + i) % 4];
    BkFog fog = {0};
    if (step == 0) {
      BkDrawDispatch bad = dispatch;
      bad.objects[20] = 42;
      CHECK(!bk_ending_normal_render_prepare_regular(
          render, &bad, &camera, &fog, material, disabled, count, e));
      CHECK(!bk_ending_normal_render_pass_count(render));
    }
    int dual = special && in.event_state != 7;
    BkViewport viewport = {8, 6, 48, 36};
    if (staged) {
      size_t floats;
      const float *world = bk_actor_pose_world(poses[1], &floats);
      CHECK(world && floats == (size_t)models[1]->frame_count * 16);
      memcpy(old_source_world, world, floats * sizeof(float));
    }
    if (dual) {
      ending.phase = in.event_state;
      ending.state_721ee0 = (int[]){4, 6, 7, 8}[step % 4];
      camera_variant = step % 10;
      restore_hidden = (uint8_t[]){0, 1, 255}[step % 3];
      index = (step * 7) % 108;
      offset = step % 3;
      if (step % 5 == 0) {
        index = group == 4 ? 47 : 104;
        offset = group == 4 ? 1 : 2;
      }
      /* Normal assets contain none of the group4 special-material name;
       * its CPU named setter is covered by the explicit native fixture. */
      BkEndingSpecialRenderInput input = {.bindings = &bindings,
                                          .dispatch = &dispatch,
                                          .camera = &camera,
                                          .fog = &fog,
                                          .primary_materials = material,
                                          .bom_disabled = disabled,
                                          .bom_count = count,
                                          .main_viewport = viewport,
                                          .special_viewport = {24, 6, 32, 24}};
      BkMenuCamera saved = camera;
      if (!step) {
        BkDrawDispatch held = dispatch;
        input.special_viewport.width = 0;
        CHECK(!bk_ending_normal_render_prepare_special(render, &input, e));
        CHECK(!bk_ending_normal_render_pass_count(render));
        CHECK(!memcmp(&camera, &saved, sizeof(camera)) &&
              !memcmp(&held, &dispatch, sizeof(dispatch)));
        input.special_viewport.width = 32;
        input.bom_count = count + 1;
        CHECK(!bk_ending_normal_render_prepare_special(render, &input, e));
        CHECK(!bk_ending_normal_render_pass_count(render));
        input.bom_count = count;
      }
      CHECK(bk_ending_normal_render_prepare_special(render, &input, e));
      CHECK(!memcmp((char *)&camera + 64, (char *)&saved + 64,
                    sizeof(camera) - 64));
    } else
      CHECK(bk_ending_normal_render_prepare_regular(
          render, &dispatch, &camera, &fog, material, disabled, count, e));
    CHECK(bk_ending_normal_render_movie_frame(render) == movie_frame);
    unsigned passes = bk_ending_normal_render_pass_count(render);
    unsigned regular = in.event_state == 7                            ? 0
                       : (in.event_state == 1 || in.event_state == 8) ? 3
                                                                      : 2;
    CHECK(passes == regular + (dual ? regular - 1 : 0));
    for (unsigned p = 0; p < passes; p++)
      CHECK(bk_ending_normal_render_pass_view(render, p) == (p >= regular));
    if (!passes)
      empty++;
    for (unsigned i = 0; passes && i < mesh_count; i++) {
      const BkBomAssetMesh *mesh = bk_bom_assets_mesh(bom, i);
      const BkModelSubmesh *sub =
          &models[mesh->actor]->submeshes[mesh->submesh];
      const BkMorphMesh *morph =
          mesh->actor
              ? bk_morph_group_mesh(bk_bom_assets_morph(bom), mesh->submesh)
              : bk_face_assets_mesh(bk_ending_normal_assets_face(assets),
                                    mesh->submesh);
      const BkModelVertex *v =
          morph ? bk_morph_mesh_vertices(morph) : sub->vertices;
      size_t floats;
      const float *world = bk_actor_pose_world(poses[mesh->actor], &floats);
      const BkModelVertex *unskinned = v;
      if (skin_mesh[i]) {
        CHECK(bk_skin_mesh_apply(skin_mesh[i], world, floats, v,
                                 sub->vertex_count, e));
        v = bk_skin_mesh_vertices(skin_mesh[i]);
      }
      memcpy(base[i], v, (size_t)sub->vertex_count * sizeof(*v));
      /*An existing outer background precedes primary and auxiliary roots.
       * The primary callback reads auxiliary's previous published cache;
       * its later visible draw uses its new cache. No world data is read
       * from the renderer under test to derive this reference.*/
      if (staged && mesh->actor == 1) {
        if (skin_mesh[i]) {
          CHECK(bk_skin_mesh_apply(skin_mesh[i], old_source_world, floats,
                                   unskinned, sub->vertex_count, e));
          v = bk_skin_mesh_vertices(skin_mesh[i]);
        }
        views[i].world = old_source_world + mesh->frame * 16;
      } else
        views[i].world = bk_actor_pose_frame(poses[mesh->actor], mesh->frame);
      memcpy(views[i].vertices, v, (size_t)sub->vertex_count * sizeof(*v));
    }
    int main_seen[8] = {0}, second_seen[8] = {0};
    CHECK(apply_queues(render, 0, regular, bom, reference, views, mesh_count,
                       plans, count, disabled, 1, main_seen, e));
    for (unsigned i = 0; i < mesh_count; i++) {
      const BkBomAssetMesh *mesh = bk_bom_assets_mesh(bom, i);
      if (staged && mesh->actor == 1) {
        memcpy(views[i].vertices, base[i],
               (size_t)views[i].count * sizeof(BkModelVertex));
        views[i].world = bk_actor_pose_frame(poses[1], mesh->frame);
      }
      memcpy(first[i], views[i].vertices,
             (size_t)views[i].count * sizeof(BkModelVertex));
    }
    if (dual) {
      CHECK(apply_queues(render, regular, passes, bom, reference, views,
                         mesh_count, plans, count, disabled, 1, second_seen, e));
      for (unsigned i = 0; i < mesh_count; i++) {
        memcpy(native[i], views[i].vertices,
               (size_t)views[i].count * sizeof(BkModelVertex));
        memcpy(views[i].vertices, base[i],
               (size_t)views[i].count * sizeof(BkModelVertex));
      }
      CHECK(apply_queues(render, regular, passes, bom, reference, views,
                         mesh_count, plans, count, disabled, 0, NULL, e));
      for (unsigned i = 0; i < mesh_count; i++)
        if (second_seen[i]) {
          CHECK(!memcmp(native[i], views[i].vertices,
                        (size_t)views[i].count * sizeof(BkModelVertex)));
          equivalent_meshes++;
          for (unsigned k = 0; k < count; k++)
            if (plans[k].target == i) {
              equivalent_targets++;
              break;
            }
        }
    }
    CHECK(bk_renderer_begin(renderer, e));
    CHECK(bk_renderer_viewport(renderer, &viewport, e));
    CHECK(bk_ending_normal_render_draw(render, e));
    CHECK(bk_renderer_end(renderer, e));
    for (unsigned i = 0; passes && i < mesh_count; i++) {
      const BkBomAssetMesh *mesh = bk_bom_assets_mesh(bom, i);
      if (!staged || main_seen[i]) {
        CHECK(bk_lit_mesh_readback(
          renderer,
          bk_ending_normal_render_mesh(render, mesh->actor, mesh->submesh),
          readback, views[i].count, e));
        CHECK(compare(readback, first[i], views[i].count));
      }
      if (dual && (!staged || second_seen[i])) {
        CHECK(bk_lit_mesh_readback(renderer,
                                   bk_ending_normal_render_view_mesh(
                                       render, 1, mesh->actor, mesh->submesh),
                                   readback, views[i].count, e));
        CHECK(compare(readback, views[i].vertices, views[i].count));
      }
    }
    if (step % 8 == 0) {
      /* No animation, world update or video clock advance for a pure redraw. */
      CHECK(bk_renderer_begin(renderer, e));
      CHECK(bk_renderer_viewport(renderer, &viewport, e));
      CHECK(bk_ending_normal_render_draw(render, e));
      CHECK(bk_renderer_end(renderer, e));
      CHECK(bk_ending_normal_render_movie_frame(render) == movie_frame);
      for (unsigned i = 0; i < mesh_count; i++) {
        const BkBomAssetMesh *mesh = bk_bom_assets_mesh(bom, i);
        if (!staged || main_seen[i]) {
          CHECK(bk_lit_mesh_readback(
            renderer,
            bk_ending_normal_render_mesh(render, mesh->actor, mesh->submesh),
            readback, views[i].count, e));
          CHECK(compare(readback, first[i], views[i].count));
        }
        if (dual && (!staged || second_seen[i])) {
          CHECK(bk_lit_mesh_readback(renderer,
                                     bk_ending_normal_render_view_mesh(
                                         render, 1, mesh->actor, mesh->submesh),
                                     readback, views[i].count, e));
          CHECK(compare(readback, views[i].vertices, views[i].count));
        }
      }
      repeats++;
    }
    BkRenderStats now = bk_renderer_stats(renderer);
    if (!step)
      baseline = now;
    else
      CHECK(now.live_allocations == baseline.live_allocations &&
            now.live_bytes == baseline.live_bytes);
    frames++;
  }
  printf("normal GPU group%u variant%u:32 frames\n", group, variant);
  ok = 1;
done:
  bk_ending_normal_render_destroy(render);
  bk_bom_deform_destroy(reference);
  bk_material_pose_destroy(material);
  for (unsigned i = 0; i < mesh_count; i++) {
    free(views[i].vertices);
    free(base[i]);
    free(first[i]);
    free(native[i]);
    bk_skin_mesh_destroy(skin_mesh[i]);
  }
  for (unsigned i = 0; i < 2; i++)
    bk_model_skin_destroy(skin[i]);
  free(readback);
  free(old_source_world);
  bk_ending_normal_assets_destroy(assets);
  bk_ending_secondary_assets_destroy(previous);
  return ok;
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 4) return 2;
  for (int i = 2; i < argc; ++i) {
    if (!strcmp(argv[i], "--special") && !special) special = 1;
    else if (!strcmp(argv[i], "--retained") && !retained) retained = 1;
    else return 2;
  }
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkRenderer *renderer = bk_renderer_create(64, 48, stdout, e);
  BkResourceStore *store = NULL, *missing_movie = NULL;
  CHECK(renderer);
  store = bk_resources_create(e);
  CHECK(store);
  missing_movie = bk_resources_create(e);
  CHECK(missing_movie);
  const char *packs[] = {"bk3_08", "bk3_03", "bk3_04", "fambom", "bk3_18"};
  for (unsigned i = 0; i < 5; i++) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
    if (i < 4)
      CHECK(bk_resources_mount(missing_movie, packs[i], path, e));
  }
  if (retained) {
    CHECK(snprintf(path, sizeof(path), "%s/bk3_09.pp", argv[1]) < (int)sizeof(path));
    CHECK(bk_resources_mount(store, "bk3_09", path, e));
    CHECK(bk_resources_mount(missing_movie, "bk3_09", path, e));
  }
  BkRenderStats baseline = bk_renderer_stats(renderer);
  for (unsigned g = 0; g < 5; g++)
    for (unsigned v = 0; v < 2; v++) {
      CHECK(profile(renderer, store, missing_movie, g, v, e));
      BkRenderStats now = bk_renderer_stats(renderer);
      CHECK(now.live_allocations == baseline.live_allocations &&
            now.live_bytes == baseline.live_bytes);
    }
  CHECK(bk_renderer_stats(renderer).skin_dispatches > 0 && movie_changes > 10);
  printf("PASS %s ending GPU retained=%d:10 profiles %u frames %u queue items %llu "
         "vertices max_relative=%g redraws=%u empty=%u movie_changes=%u; "
         "equivalent_second_view_meshes=%u targets=%u; stable allocations\n",
         special ? "special" : "normal", retained, frames, queues,
         (unsigned long long)vertices, worst, repeats, empty, movie_changes,
         equivalent_meshes, equivalent_targets);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", e);
  bk_resources_destroy(store);
  bk_resources_destroy(missing_movie);
  bk_renderer_destroy(renderer);
  return rc;
}
