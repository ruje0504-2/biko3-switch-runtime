/* Numeric mesh readback only. Metadata selects explicit resource fixtures;
 * this is not the ending loader, gameplay, or a visual scene comparison. */
#include "bom_motion_fixture.h"
#include "model/skin.h"
#include "scene/bom_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "line%d: %s\n", __LINE__, #x);                           \
      goto done;                                                               \
    }                                                                          \
  } while (0)
#define MAX_MESH 8
static uint64_t vertices_checked;
static unsigned frames_checked, alpha_frames, hidden_frames, repeated_frames;
static double worst;
static int motion, seconds;
static BkBomMotionFixture motion_state;
static int compare(const BkLitVertex *got, const BkModelVertex *want,
                   unsigned n, char e[256]) {
  for (unsigned i = 0; i < n; i++) {
    float p[] = {got[i].base.x, got[i].base.y, got[i].base.z};
    for (unsigned j = 0; j < 6; j++) {
      float a = j < 3 ? p[j] : got[i].normal[j - 3];
      float b = j < 3 ? want[i].position[j] : want[i].normal[j - 3];
      double d = fabs((double)a - b) / fmax(1, fabs(b));
      if (d > worst)
        worst = d;
      if (!isfinite(d) || d > 3e-5) {
        snprintf(e, 256, "BOM vertex%u component%u got%g want%g relative%g", i,
                 j, a, b, d);
        return 0;
      }
    }
    vertices_checked++;
  }
  return 1;
}
static int profile(BkRenderer *r, BkResourceStore *store, unsigned number,
                   BkLightSet *light, char e[256]) {
  BkModel *models[2] = {0};
  BkClipSet *clips[2] = {0};
  BkActorPose *poses[2] = {0};
  BkActorRender *renders[2] = {0};
  BkModelSkin *skin[2] = {0};
  BkSkinMesh *skin_mesh[MAX_MESH] = {0};
  BkGpuMesh *gpu[MAX_MESH] = {0};
  BkBomMeshView views[MAX_MESH] = {0};
  BkBomDeformBinding plans[4];
  BkActorForest *forest = NULL;
  BkBomAssets *assets = NULL;
  BkBomDeform *reference = NULL;
  BkBomRender *render = NULL;
  BkMaterialPose *material = NULL;
  BkLitVertex *readback = NULL;
  BkActorRenderBatch *batch = NULL;
  float *world[2] = {0};
  float root_local[16], projection[16];
  BkBlob raw = {0};
  BkBomConfig config;
  BkBomActor actors[2];
  uint32_t roots[2], count = 0, max = 0;
  char name[32];
  int ok = 0;
  const char *pack = number < 5 ? "bk3_08" : "bk3_11";
  if (number < 5)
    snprintf(name, sizeof(name), "h%02u_00.bom", number + 1);
  else
    snprintf(name, sizeof(name), "%s",
             number == 5 ? "jouhansin.bom" : "kahansin.bom");
  CHECK(bk_resources_read(store, "fambom", name, &raw, e) == BK_RESOURCE_OK);
  CHECK(bk_bom_decode(raw.data, raw.size, &config, e));
  bk_blob_free(&raw);
  const char *names[] = {config.primary_clip, config.secondary_clip};
  for (unsigned a = 0; a < 2; a++) {
    CHECK(bk_resources_read(store, pack, names[a], &raw, e) == BK_RESOURCE_OK);
    clips[a] = bk_clip_set_decode(raw.data, raw.size, e);
    bk_blob_free(&raw);
    CHECK(clips[a]);
    CHECK(bk_resources_read(store, pack, bk_clip_model_name(clips[a]), &raw,
                            e) == BK_RESOURCE_OK);
    CHECK(bk_model_decode(raw.data, raw.size, &models[a], e) == BK_MODEL_OK);
    bk_blob_free(&raw);
    roots[a] = BK_MODEL_NONE;
    for (uint32_t f = 0; f < models[a]->frame_count; f++)
      if (models[a]->frames[f].parent_index == BK_MODEL_NONE) {
        CHECK(roots[a] == BK_MODEL_NONE);
        roots[a] = f;
      }
    CHECK(roots[a] != BK_MODEL_NONE);
    poses[a] = bk_actor_pose_create_loaded(models[a], clips[a], roots[a],
                                           (float[3]){0}, 0, e);
    CHECK(poses[a]);
    memcpy(root_local, models[a]->frames[roots[a]].local, 64);
    root_local[12] += 2.25f * a;
    root_local[13] -= 1.5f * a;
    CHECK(bk_actor_pose_root_local(poses[a], root_local, e));
    actors[a] =
        (BkBomActor){poses[a], bk_clip_model_name(clips[a]), a, roots[a]};
    world[a] = malloc((size_t)models[a]->frame_count * 64);
    CHECK(world[a]);
    if (bk_model_chunk(models[a], "ENVL")) {
      skin[a] = bk_model_skin_create(models[a], e);
      CHECK(skin[a]);
    }
  }
  forest = bk_actor_forest_create(poses, 2, e);
  CHECK(forest);
  for (unsigned a = 0; a < 2; a++)
    CHECK(bk_actor_forest_attach(forest, 0,
                                 bk_actor_forest_node(forest, a, roots[a]), e));
  assets = bk_bom_assets_create(store, pack, &config, actors, forest, e);
  CHECK(assets);
  count = bk_bom_assets_mesh_count(assets);
  CHECK(count <= MAX_MESH);
  for (uint32_t i = 0; i < count; i++) {
    const BkBomAssetMesh *m = bk_bom_assets_mesh(assets, i);
    const BkModelSubmesh *s = models[m->actor]->submeshes + m->submesh;
    views[i] = (BkBomMeshView){
        malloc((size_t)s->vertex_count * sizeof(BkModelVertex)),
        s->vertex_count, bk_actor_pose_frame(poses[m->actor], m->frame)};
    CHECK(views[i].vertices);
    memcpy(views[i].vertices, s->vertices,
           (size_t)s->vertex_count * sizeof(BkModelVertex));
    if (s->vertex_count > max)
      max = s->vertex_count;
    for (uint32_t j = 0; j < bk_model_skin_count(skin[m->actor]); j++)
      if (bk_model_skin_entry(skin[m->actor], j)->submesh == m->submesh) {
        skin_mesh[i] = bk_skin_mesh_create(skin[m->actor], j, s, e);
        CHECK(skin_mesh[i]);
      }
  }
  for (uint32_t i = 0; i < config.count; i++)
    CHECK(bk_bom_assets_plan(assets, i, plans + i));
  reference = bk_bom_deform_create(views, count, plans, config.count, e);
  CHECK(reference);
  for (uint32_t i = 0; i < config.count; i++) {
    int32_t ga, gb;
    const uint32_t *a, *b;
    size_t na, nb;
    CHECK(bk_bom_deform_mapping(reference, i, &ga, &a, &na));
    CHECK(bk_bom_assets_mapping(assets, i, &gb, &b, &nb));
    CHECK(ga == gb && na == nb && (!na || !memcmp(a, b, na * 4)));
    /* All nonempty targets in these seven real fixtures use ENVL. */
    if (plans[i].count) {
      CHECK(skin_mesh[plans[i].target]);
      const BkBomAssetMesh *target =
          bk_bom_assets_mesh(assets, plans[i].target);
      const BkModelSkin *s = skin[target->actor];
      unsigned entry = 0;
      while (entry < bk_model_skin_count(s) &&
             bk_model_skin_entry(s, entry)->submesh != target->submesh)
        entry++;
      CHECK(entry < bk_model_skin_count(s));
      for (size_t j = 0; j < plans[i].count; j++) {
        int influenced = 0;
        for (uint32_t b = 0; b < bk_model_skin_entry(s, entry)->bone_count;
             b++) {
          const BkSkinBone *bone = bk_model_skin_bone(s, entry, b);
          for (uint32_t k = 0; k < bone->count; k++)
            influenced |= bone->influences[k].index == plans[i].indices[j];
        }
        CHECK(influenced);
      }
    }
  }
  for (unsigned a = 0; a < 2; a++) {
    renders[a] = bk_actor_render_create(r, store, pack, models[a], NULL, e);
    CHECK(renders[a]);
  }
  for (uint32_t i = 0; i < count; i++) {
    const BkBomAssetMesh *m = bk_bom_assets_mesh(assets, i);
    gpu[i] =
        bk_actor_render_mesh(renders[m->actor], models[m->actor], m->submesh);
    CHECK(gpu[i]);
  }
  material = bk_material_pose_create(models[0], e);
  CHECK(material);
  render = bk_bom_render_create(r, assets, renders, e);
  CHECK(render);
  CHECK(!bk_bom_render_create(r, assets, renders,
                              e)); /* callback owner conflict */
  batch = bk_actor_render_batch_create(r, models[0]->frame_count * 4, e);
  CHECK(batch);
  readback = malloc((size_t)max * sizeof(*readback));
  CHECK(readback);
  memcpy(projection, bk_identity, 64);
  projection[0] = projection[5] = .01f;
  projection[10] = .001f;
  projection[12] = 100; /* Clip all geometry, compare buffers. */
  memcpy(root_local, bk_actor_pose_local(poses[0], roots[0]), 64);
  BkRenderStats baseline = bk_renderer_stats(r);
  for (unsigned frame = 0; frame < 72; frame++) {
    int hide = frame % 12 == 7, alpha = frame % 12 == 4;
    int32_t disabled[4];
    for (unsigned i = 0; i < config.count; i++)
      disabled[i] = (int32_t[]){0, 1, 2, -1}[(frame + i) % 4];
    CHECK(!bk_bom_render_prepare(render, disabled, config.count + 1, e));
    BkActorVisibilityEdit v = {roots[0], hide};
    CHECK(bk_actor_pose_visibility(poses[0], &v, 1, e));
    v = (BkActorVisibilityEdit){roots[1], frame % 8 >= 4};
    CHECK(bk_actor_pose_visibility(poses[1], &v, 1, e));
    if (frame % 2 == 0) {
      float local[16];
      memcpy(local, root_local, 64);
      local[12] += .0625f * (frame / 2 % 5);
      CHECK(bk_actor_pose_root_local(poses[0], local, e));
      if (seconds) {
        if (frame % 6 == 0) {
          unsigned active[128], n = 0;
          for (unsigned slot = 0; slot < 128; slot++)
            if (bk_clip_definition(clips[1], slot)->active)
              active[n++] = slot;
          CHECK(n && bk_actor_pose_request(poses[1], active[frame / 6 % n], e));
        }
        CHECK(bk_bom_assets_advance(
            assets, (float[]){0, .016f, .05f, .1f, .25f}[frame % 5], e));
      } else
        CHECK(bk_bom_assets_advance_frame(assets, e));
    }
    if (motion)
      CHECK(motion_fixture_step(&motion_state, assets, frame, e));
    CHECK(bk_actor_forest_refresh(forest, e));
    BkMaterialAlphaEdit edit = {.frame = roots[0], .alpha = alpha ? 0 : 1};
    CHECK(bk_material_pose_alpha(material, &edit, 1, e));
    CHECK(bk_actor_render_prepare(renders[0], poses[0], material, NULL,
                                  bk_identity, projection, e));
    CHECK(bk_actor_render_prepare_morph(
        renders[1], poses[1], bk_bom_assets_materials(assets),
        bk_bom_assets_morph(assets), bk_identity, projection, e));
    CHECK(bk_bom_render_prepare(render, disabled, config.count, e));
    if (frame == 0) {
      /* A later actor prepare invalidates BOM world/flag snapshots. */
      CHECK(bk_actor_render_prepare(renders[0], poses[0], material, NULL,
                                    bk_identity, projection, e));
      CHECK(bk_renderer_begin(r, e));
      CHECK(!bk_actor_render_draw(renders[0], light, e));
      CHECK(bk_renderer_end(r, e));
      CHECK(bk_bom_render_prepare(render, disabled, config.count, e));
    }
    BkActorRender *submitted[] = {renders[0], renders[0]};
    unsigned repeats = frame % 3 == 0 ? 2 : 1;
    CHECK(bk_actor_render_batch_prepare(batch, submitted, repeats, e));
    for (unsigned a = 0; a < 2; a++)
      for (uint32_t f = 0; f < models[a]->frame_count; f++)
        memcpy(world[a] + f * 16, bk_actor_pose_frame(poses[a], f), 64);
    for (uint32_t i = 0; i < count; i++) {
      const BkBomAssetMesh *m = bk_bom_assets_mesh(assets, i);
      const BkModelSubmesh *s = models[m->actor]->submeshes + m->submesh;
      const BkMorphMesh *morph =
          m->actor
              ? bk_morph_group_mesh(bk_bom_assets_morph(assets), m->submesh)
              : NULL;
      const BkModelVertex *vertices =
          morph ? bk_morph_mesh_vertices(morph) : s->vertices;
      views[i].world = bk_actor_pose_frame(poses[m->actor], m->frame);
      if (skin_mesh[i]) {
        CHECK(bk_skin_mesh_apply(skin_mesh[i], world[m->actor],
                                 (size_t)models[m->actor]->frame_count * 16,
                                 vertices, s->vertex_count, e));
        vertices = bk_skin_mesh_vertices(skin_mesh[i]);
      }
      memcpy(views[i].vertices, vertices,
             (size_t)s->vertex_count * sizeof(*vertices));
    }
    /* Consume actual prepared queue identities; duplicates are not collapsed.
     */
    for (uint32_t q = 0; q < bk_actor_render_batch_count(batch); q++) {
      uint32_t actor, frame_id, submesh;
      CHECK(bk_actor_render_batch_item(batch, q, &actor, &frame_id, &submesh));
      (void)actor;
      (void)frame_id;
      for (uint32_t i = 0; i < count; i++) {
        const BkBomAssetMesh *m = bk_bom_assets_mesh(assets, i);
        if (m->actor != 0 || m->submesh != submesh)
          continue;
        int target = 0;
        for (uint32_t j = 0; j < config.count; j++)
          target |= plans[j].target == i;
        if (target)
          CHECK(bk_bom_deform_draw(reference, i, views, count, disabled, e));
      }
    }
    CHECK(bk_renderer_begin(r, e));
    CHECK(bk_actor_render_batch_draw(batch, light, e));
    CHECK(bk_renderer_end(r, e));
    for (uint32_t i = 0; i < count; i++) {
      CHECK(bk_lit_mesh_readback(r, gpu[i], readback, views[i].count, e));
      if (!compare(readback, views[i].vertices, views[i].count, e)) {
        fprintf(stderr, "profile%u frame%u registry%u\n", number, frame, i);
        goto done;
      }
    }
    BkRenderStats now = bk_renderer_stats(r);
    CHECK(now.live_allocations == baseline.live_allocations &&
          now.live_bytes == baseline.live_bytes);
    alpha_frames += alpha;
    hidden_frames += hide;
    repeated_frames += repeats == 2;
    frames_checked++;
  }
  printf("BOM GPU fixture%u: %u meshes, 72 frames\n", number, count);
  ok = 1;
done:
  bk_blob_free(&raw);
  bk_bom_render_destroy(render);
  bk_actor_render_batch_destroy(batch);
  free(readback);
  bk_material_pose_destroy(material);
  bk_bom_deform_destroy(reference);
  for (unsigned i = 0; i < MAX_MESH; i++) {
    free(views[i].vertices);
    bk_skin_mesh_destroy(skin_mesh[i]);
  }
  for (unsigned a = 0; a < 2; a++)
    bk_actor_render_destroy(renders[a]);
  bk_bom_assets_destroy(assets);
  bk_actor_forest_destroy(forest);
  for (unsigned a = 0; a < 2; a++) {
    free(world[a]);
    bk_model_skin_destroy(skin[a]);
    bk_actor_pose_destroy(poses[a]);
    bk_clip_set_destroy(clips[a]);
    bk_model_destroy(models[a]);
  }
  return ok;
}
int main(int argc, char **argv) {
  if (argc != 2 && !(argc == 3 && (!strcmp(argv[2], "--motion") ||
                                   !strcmp(argv[2], "--seconds"))))
    return 2;
  motion = argc == 3;
  seconds = motion && !strcmp(argv[2], "--seconds");
  motion_fixture_init(&motion_state);
  char e[256] = {0}, path[1024];
  int rc = 1;
  BkRenderer *r = bk_renderer_create(32, 32, stdout, e);
  BkResourceStore *store = NULL;
  BkLightSet *light = NULL;
  CHECK(r);
  store = bk_resources_create(e);
  CHECK(store);
  const char *packs[] = {"bk3_08", "bk3_11", "fambom"};
  for (unsigned i = 0; i < 3; i++) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, e));
  }
  light = bk_light_set_create(r, &(BkLighting){.ambient = {1, 1, 1}}, e);
  CHECK(light);
  BkRenderStats baseline = bk_renderer_stats(r);
  for (unsigned i = 0; i < 7; i++) {
    CHECK(profile(r, store, i, light, e));
    BkRenderStats now = bk_renderer_stats(r);
    CHECK(now.live_allocations == baseline.live_allocations &&
          now.live_bytes == baseline.live_bytes);
  }
  printf("PASS BOM GPU:7 profiles %u frames %llu vertices relative max%g; "
         "alpha0:%u hidden:%u repeated:%u; stable allocations\n",
         frames_checked, (unsigned long long)vertices_checked, worst,
         alpha_frames, hidden_frames, repeated_frames);
  rc = 0;
  if (seconds)
    puts("PASS BOM seconds GPU: whole-model MORP blend and material samples");
  if (motion)
    puts("PASS BOM motion GPU integration: input/return/follow before "
         "publication and ENVL");
done:
  bk_light_set_destroy(r, light);
  bk_resources_destroy(store);
  bk_renderer_destroy(r);
  if (rc)
    fprintf(stderr, "BOM GPU failed:%s\n", e);
  return rc;
}
