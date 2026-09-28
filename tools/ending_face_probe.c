#include "model/clip.h"
#include "resource/bom.h"
#include "scene/face_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
static uint64_t digest(uint64_t h, const void *data, size_t n) {
  const uint8_t *p = data;
  for (size_t i = 0; i < n; i++)
    h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024], pack[32];
  BkArchive archive = {0};
  BkResourceStore *store = bk_resources_create(e);
  BkBlob raw = {0};
  BkModel *model = NULL;
  BkClipSet *clips = NULL;
  BkFaceAssets *face = NULL;
  unsigned profiles = 0, frames = 0, boms = 0, four = 0;
  uint64_t vertices = 0, hash = UINT64_C(14695981039346656037);
  int rc = 1;
  CHECK(store);
  snprintf(path, sizeof(path), "%s/fambom.pp", argv[1]);
  CHECK(bk_archive_open(&archive, path, e));
  CHECK(bk_resources_mount(store, "ending-faces", path, e));
  for (unsigned i = 8; i < 15; i++) {
    snprintf(pack, sizeof(pack), "bk3_%02u", i);
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], pack);
    CHECK(bk_resources_mount(store, pack, path, e));
  }
  for (unsigned i = 0; i < archive.count; i++) {
    const char *name = archive.entries[i].name;
    size_t length = strlen(name);
    if (length < 4)
      continue;
    if (!strcmp(name + length - 4, ".bom")) {
      CHECK(bk_resources_read(store, "ending-faces", name, &raw, e) ==
            BK_RESOURCE_OK);
      BkBomConfig config;
      CHECK(bk_bom_decode(raw.data, raw.size, &config, e));
      bk_blob_free(&raw);
      hash = digest(hash, &config, sizeof(config));
      boms++;
      continue;
    }
    if (strcmp(name + length - 4, ".fam"))
      continue;
    CHECK(bk_resources_read(store, "ending-faces", name, &raw, e) ==
          BK_RESOURCE_OK);
    BkFaceConfig config;
    CHECK(bk_face_config_decode(raw.data, raw.size, &config, e));
    bk_blob_free(&raw);
    /* Asset inventory search, not production scene selection policy. */
    unsigned matches = 0;
    for (unsigned j = 8; j < 15; j++) {
      char candidate[32];
      snprintf(candidate, sizeof(candidate), "bk3_%02u", j);
      BkResourceResult r =
          bk_resources_read(store, candidate, config.actor_clip, &raw, e);
      CHECK(r != BK_RESOURCE_ERROR);
      if (r == BK_RESOURCE_OK) {
        matches++;
        CHECK(matches == 1);
        strcpy(pack, candidate);
        clips = bk_clip_set_decode(raw.data, raw.size, e);
        bk_blob_free(&raw);
        CHECK(clips);
      }
    }
    CHECK(matches == 1);
    CHECK(bk_resources_read(store, pack, bk_clip_model_name(clips), &raw, e) ==
          BK_RESOURCE_OK);
    CHECK(bk_model_decode(raw.data, raw.size, &model, e) == BK_MODEL_OK);
    bk_blob_free(&raw);
    if (config.counts[0] == 4) {
      BkFaceAssets *ordinary =
          bk_face_assets_create(store, "ending-faces", name, pack, model,
                                bk_clip_model_name(clips), e);
      assert(!ordinary);
      four++;
    }
    face = bk_face_assets_create_ending(store, "ending-faces", name, pack,
                                        model, bk_clip_model_name(clips), e);
    CHECK(face);
    unsigned meshes = model->submesh_count;
    bk_model_destroy(model);
    model = NULL;
    bk_clip_set_destroy(clips);
    clips = NULL;
    BkFaceState state;
    uint32_t random = 123 + profiles, clocks[4] = {15000, 15001, 15002, 15003};
    CHECK(bk_face_assets_initialize(face, &state, clocks, &random, e));
    assert(state.eye_count == config.counts[0] &&
           state.mouth_count == config.counts[1]);
    for (unsigned step = 0; step < 120; step++) {
      uint32_t now = 15100 + step * 137;
      CHECK(bk_face_assets_step(face, &state, (int32_t)(step / 10 % 12),
                                (float)(step % 10), now, now + 1, now + 2,
                                now + 3, &random, e));
      hash = digest(hash, &state, sizeof(state));
      hash = digest(hash, &random, sizeof(random));
      for (unsigned m = 0; m < meshes; m++) {
        const BkMorphMesh *mesh = bk_face_assets_mesh(face, m);
        if (!mesh)
          continue;
        uint32_t count = bk_morph_mesh_count(mesh);
        vertices += count;
        hash = digest(hash, bk_morph_mesh_vertices(mesh),
                      count * sizeof(BkModelVertex));
      }
      frames++;
    }
    bk_face_assets_destroy(face);
    face = NULL;
    profiles++;
  }
  assert(profiles == 50 && boms == 7 && four > 20 && frames == 6000);
  printf("PASS ending face: %u configs (%u fourth-eye), %u BOM, %u frames, "
         "%llu vertices, FNV%016llx; ordinary limits retained and owned source "
         "lifetimes\n",
         profiles, four, boms, frames, (unsigned long long)vertices,
         (unsigned long long)hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "profile%u: %s\n", profiles, e);
  bk_face_assets_destroy(face);
  bk_model_destroy(model);
  bk_clip_set_destroy(clips);
  bk_blob_free(&raw);
  bk_archive_close(&archive);
  bk_resources_destroy(store);
  return rc;
}
