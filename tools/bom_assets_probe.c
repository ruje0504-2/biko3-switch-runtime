#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "bom_motion_fixture.h"
#include "scene/bom_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
static uint64_t hash_bytes(uint64_t h, const void *data, size_t size) {
  const unsigned char *p = data;
  while (size--)
    h = (h ^ *p++) * UINT64_C(1099511628211);
  return h;
}
static uint64_t pose_hash(BkActorPose *pose) {
  uint64_t h = UINT64_C(14695981039346656037);
  const BkModel *m = bk_actor_pose_model(pose);
  for (uint32_t i = 0; i < m->frame_count; i++) {
    h = hash_bytes(h, bk_actor_pose_local(pose, i), 64);
    h = hash_bytes(h, bk_actor_pose_frame(pose, i), 64);
    h = hash_bytes(h, bk_actor_pose_parent_world(pose, i), 64);
  }
  BkClipState state;
  if (!bk_actor_pose_state(pose, &state))
    abort();
  return hash_bytes(h, &state, sizeof(state));
}
int main(int argc, char **argv) {
  if (argc != 2 && !(argc == 3 && (!strcmp(argv[2], "--motion") ||
                                   !strcmp(argv[2], "--seconds"))))
    return 2;
  int motion = argc == 3;
  int seconds = motion && !strcmp(argv[2], "--seconds");
  BkBomMotionFixture motion_state;
  motion_fixture_init(&motion_state);
  char e[256] = {0}, path[1024], name[32];
  BkResourceStore *store = bk_resources_create(e), *overlay = NULL;
  BkBlob raw = {0};
  BkBomConfig config;
  BkModel *models[2] = {0};
  BkClipSet *clips[2] = {0};
  BkActorPose *poses[2] = {0};
  BkActorForest *forest = NULL;
  BkBomAssets *assets = NULL;
  uint32_t roots[2];
  BkBomActor actors[2];
  unsigned frames = 0, missing = 0, rejected = 0;
  uint64_t vertices = 0, maps = 0;
  uint64_t hash = UINT64_C(14695981039346656037);
  char temporary[] = "/tmp/biko3-bom-XXXXXX", bad_path[256] = {0};
  int have_temp = 0, rc = 1;
  CHECK(store);
  for (unsigned i = 0; i < 3; i++) {
    snprintf(name, sizeof(name), "%s",
             i == 0   ? "bk3_08"
             : i == 1 ? "bk3_11"
                      : "fambom");
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], name);
    CHECK(bk_resources_mount(store, name, path, e));
  }
  for (unsigned profile = 0; profile < 7; profile++) {
    const char *pack = profile < 5 ? "bk3_08" : "bk3_11";
    if (profile < 5)
      snprintf(name, sizeof(name), "h%02u_00.bom", profile + 1);
    else
      snprintf(name, sizeof(name), "%s",
               profile == 5 ? "jouhansin.bom" : "kahansin.bom");
    CHECK(bk_resources_read(store, "fambom", name, &raw, e) == BK_RESOURCE_OK);
    CHECK(bk_bom_decode(raw.data, raw.size, &config, e));
    bk_blob_free(&raw);
    /* Metadata is an explicit test inventory only, never production dispatch.
     */
    const char *xans[] = {config.primary_clip, config.secondary_clip};
    for (unsigned i = 0; i < 2; i++) {
      CHECK(bk_resources_read(store, pack, xans[i], &raw, e) == BK_RESOURCE_OK);
      clips[i] = bk_clip_set_decode(raw.data, raw.size, e);
      bk_blob_free(&raw);
      CHECK(clips[i]);
      CHECK(bk_resources_read(store, pack, bk_clip_model_name(clips[i]), &raw,
                              e) == BK_RESOURCE_OK);
      CHECK(bk_model_decode(raw.data, raw.size, &models[i], e) == BK_MODEL_OK);
      bk_blob_free(&raw);
      roots[i] = BK_MODEL_NONE;
      for (uint32_t f = 0; f < models[i]->frame_count; f++)
        if (models[i]->frames[f].parent_index == BK_MODEL_NONE) {
          CHECK(roots[i] == BK_MODEL_NONE);
          roots[i] = f;
        }
      CHECK(roots[i] != BK_MODEL_NONE);
      poses[i] = bk_actor_pose_create_loaded(models[i], clips[i], roots[i],
                                             (float[3]){0}, 0, e);
      CHECK(poses[i]);
      float local[16];
      memcpy(local, models[i]->frames[roots[i]].local, 64);
      local[12] += 2.25f * i;
      local[13] -= 1.5f * i;
      CHECK(bk_actor_pose_root_local(poses[i], local, e));
      actors[i] =
          (BkBomActor){poses[i], bk_clip_model_name(clips[i]), i, roots[i]};
    }
    forest = bk_actor_forest_create(poses, 2, e);
    CHECK(forest);
    for (unsigned i = 0; i < 2; i++)
      CHECK(bk_actor_forest_attach(
          forest, 0, bk_actor_forest_node(forest, i, roots[i]), e));
    uint64_t held[] = {pose_hash(poses[0]), pose_hash(poses[1])};
    BkBomConfig invalid = config;
    invalid.count = 5;
    CHECK(!bk_bom_assets_create(store, pack, &invalid, actors, forest, e));
    rejected++;
    invalid = config;
    memset(invalid.bindings[0].parent, 'x', 260);
    CHECK(!bk_bom_assets_create(store, pack, &invalid, actors, forest, e));
    rejected++;
    invalid = config;
    snprintf(invalid.bindings[0].child, 260, "absent-required-child");
    CHECK(!bk_bom_assets_create(store, pack, &invalid, actors, forest, e));
    rejected++;
    CHECK(held[0] == pose_hash(poses[0]) && held[1] == pose_hash(poses[1]));
    /* Resource corruption/read failure must abort; unlike a missing VIX it
     * cannot silently become an empty selection. */
    if (profile == 0) {
      CHECK(mkdtemp(temporary));
      have_temp = 1;
      snprintf(bad_path, sizeof(bad_path), "%s/%s", temporary,
               config.bindings[0].selection);
      FILE *file = fopen(bad_path, "wb");
      CHECK(file);
      int wrote = fwrite("bad", 1, 3, file) == 3;
      int closed = fclose(file) == 0;
      CHECK(wrote && closed);
      overlay = bk_resources_create(e);
      CHECK(overlay);
      CHECK(bk_resources_mount_directory(overlay, pack, temporary, 1, e));
      CHECK(!bk_bom_assets_create(overlay, pack, &config, actors, forest, e));
      rejected++;
      CHECK(held[0] == pose_hash(poses[0]) && held[1] == pose_hash(poses[1]));
      bk_resources_destroy(overlay);
      overlay = NULL;
      CHECK(unlink(bad_path) == 0);
      bad_path[0] = 0;
      CHECK(rmdir(temporary) == 0);
      have_temp = 0;
    }
    assets = bk_bom_assets_create(store, pack, &config, actors, forest, e);
    CHECK(assets);
    CHECK(bk_bom_assets_count(assets) == config.count);
    for (uint32_t i = 0; i < config.count; i++) {
      const BkBomAssetBinding *b = bk_bom_assets_binding(assets, i);
      CHECK(b);
      missing += b->missing_selection;
      int32_t group;
      const uint32_t *sources;
      size_t count;
      CHECK(bk_bom_assets_mapping(assets, i, &group, &sources, &count));
      maps += count;
      BkBomDeformBinding plan;
      CHECK(bk_bom_assets_plan(assets, i, &plan));
      CHECK(plan.count == count && plan.source == b->source &&
            plan.target == b->target);
      BkResourceResult read =
          bk_resources_read(store, pack, config.bindings[i].selection, &raw, e);
      CHECK(read == BK_RESOURCE_OK ||
            (read == BK_RESOURCE_MISSING && b->missing_selection));
      CHECK(raw.size / 2 == count);
      for (size_t j = 0; j < count; j++)
        CHECK(plan.indices[j] ==
              (uint16_t)(raw.data[j * 2] | (uint16_t)raw.data[j * 2 + 1] << 8));
      bk_blob_free(&raw);
      hash = hash_bytes(hash, b, sizeof(*b));
      hash = hash_bytes(hash, &group, 4);
      hash = hash_bytes(hash, sources, count * 4);
      if (b->missing_selection)
        CHECK(!count && !b->selection_count);
    }
    for (unsigned step = 0; step < 240; step++) {
      if (step % 10 == 0) {
        BkActorVisibilityEdit edit = {roots[1], step % 20 == 0};
        CHECK(bk_actor_pose_visibility(poses[1], &edit, 1, e));
      }
      uint64_t before = pose_hash(poses[1]);
      uint32_t hidden;
      CHECK(bk_actor_pose_hidden(poses[1], roots[1], &hidden));
      if (seconds) {
        if (step % 15 == 0) {
          unsigned active[128], n = 0;
          for (unsigned slot = 0; slot < 128; slot++)
            if (bk_clip_definition(clips[1], slot)->active)
              active[n++] = slot;
          CHECK(n && bk_actor_pose_request(poses[1], active[step / 15 % n], e));
        }
        CHECK(bk_bom_assets_advance(
            assets, (float[]){0, .016f, .05f, .1f, .25f}[step % 5], e));
      } else
        CHECK(bk_bom_assets_advance_frame(assets, e));
      if (hidden && !(seconds && step % 15 == 0))
        CHECK(before == pose_hash(poses[1]));
      if (motion)
        CHECK(motion_fixture_step(&motion_state, assets, step, e));
      if (step % 7 == 0)
        CHECK(bk_actor_forest_refresh(forest, e));
      for (unsigned i = 0; i < 2; i++) {
        uint64_t h = pose_hash(poses[i]);
        hash = hash_bytes(hash, &h, sizeof(h));
      }
      const BkMorphGroup *g = bk_bom_assets_morph(assets);
      for (uint32_t i = 0; i < models[1]->submesh_count; i++) {
        const BkMorphMesh *m = bk_morph_group_mesh(g, i);
        if (m) {
          uint32_t count = bk_morph_mesh_count(m);
          vertices += count;
          hash = hash_bytes(hash, bk_morph_mesh_vertices(m),
                            (size_t)count * sizeof(BkModelVertex));
        }
      }
      for (uint32_t i = 0; i < models[1]->material_count; i++) {
        const BkModelMaterial *m =
            bk_material_pose_material(bk_bom_assets_materials(assets), i);
        CHECK(m);
        hash = hash_bytes(hash, m->diffuse, 68);
      }
      frames++;
    }
    bk_bom_assets_destroy(assets);
    assets = NULL;
    bk_actor_forest_destroy(forest);
    forest = NULL;
    for (unsigned i = 0; i < 2; i++) {
      bk_actor_pose_destroy(poses[i]);
      poses[i] = NULL;
      bk_clip_set_destroy(clips[i]);
      clips[i] = NULL;
      bk_model_destroy(models[i]);
      models[i] = NULL;
    }
  }
  CHECK(missing == 1);
  printf("PASS BOM owner:7 profiles %u frames %llu vertices %llu mappings %u "
         "missing VIX %u preflight failures FNV%016llx\n",
         frames, (unsigned long long)vertices, (unsigned long long)maps,
         missing, rejected, (unsigned long long)hash);
  rc = 0;
  if (seconds)
    puts("PASS BOM seconds: actual ANIM/MATA/MORP with clip requests and "
         "hidden frames");
  if (motion)
    puts("PASS actual BOM motion: retained controllers across7 resource "
         "reloads");
done:
  bk_blob_free(&raw);
  bk_bom_assets_destroy(assets);
  bk_actor_forest_destroy(forest);
  for (unsigned i = 0; i < 2; i++) {
    bk_actor_pose_destroy(poses[i]);
    bk_clip_set_destroy(clips[i]);
    bk_model_destroy(models[i]);
  }
  bk_resources_destroy(overlay);
  bk_resources_destroy(store);
  if (*bad_path)
    unlink(bad_path);
  if (have_temp)
    rmdir(temporary);
  if (rc)
    fprintf(stderr, "BOM owner failed:%s\n", e);
  return rc;
}
