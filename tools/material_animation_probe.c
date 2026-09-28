#include "model/material_animation.h"
#include "resource/store.h"
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
static uint32_t word(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static int has_mata(const BkBlob *b) {
  if (b->size < 12 || memcmp(b->data, "OBJM", 4))
    return 0;
  size_t p = 12;
  while (b->size - p >= 8) {
    uint32_t n = word(b->data + p + 4);
    if (n > b->size - p - 8)
      return 0;
    if (!memcmp(b->data + p, "MATA", 4))
      return 1;
    p += 8 + n;
  }
  return 0;
}
static uint64_t digest(uint64_t h, const void *data, size_t size) {
  const uint8_t *p = data;
  for (size_t i = 0; i < size; i++)
    h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024];
  BkArchive archive = {0};
  BkBlob raw = {0};
  BkModel *model = NULL;
  BkMaterialAnimation *a = NULL;
  BkMaterialPose *pose = NULL;
  unsigned profiles = 0, frames = 0, tracks = 0;
  uint64_t materials = 0, hash = UINT64_C(14695981039346656037);
  int rc = 1;
  const unsigned packs[] = {1, 3, 8, 9, 10, 11, 12, 13, 14, 17};
  for (unsigned p = 0; p < sizeof(packs) / sizeof(*packs); p++) {
    snprintf(path, sizeof(path), "%s/bk3_%02u.pp", argv[1], packs[p]);
    CHECK(bk_archive_open(&archive, path, e));
    for (uint32_t i = 0; i < archive.count; i++) {
      const char *name = archive.entries[i].name;
      size_t n = strlen(name);
      if (n < 2 || strcmp(name + n - 2, ".x"))
        continue;
      CHECK(bk_archive_read(&archive, &archive.entries[i], &raw.data, e));
      raw.size = archive.entries[i].size;
      if (!has_mata(&raw)) {
        bk_blob_free(&raw);
        continue;
      }
      CHECK(bk_model_decode(raw.data, raw.size, &model, e) == BK_MODEL_OK);
      bk_blob_free(&raw);
      a = bk_material_animation_create(model, e);
      pose = bk_material_pose_create(model, e);
      CHECK(a && pose);
      uint32_t count = bk_material_animation_tracks(a);
      CHECK(count);
      tracks += count;
      for (unsigned step = 0; step < 120; step++) {
        uint32_t ti = (step / 8) % count;
        const BkMaterialTrack *t = bk_material_animation_track(a, ti);
        const BkMaterialKey *key =
            bk_material_animation_key(a, ti, (step / 2) % t->key_count);
        float time = key->time + (step % 2 ? .25f : 0);
        if (step % 7 == 0)
          time = bk_material_animation_time(a);
        CHECK(bk_material_animation_sample(a, time, pose, e));
        for (uint32_t j = 0; j < model->material_count; j++) {
          const BkModelMaterial *m = bk_material_pose_material(pose, j);
          hash = digest(hash, m, sizeof(*m));
          materials++;
        }
        frames++;
      }
      CHECK(bk_material_animation_restore(a, pose, e));
      bk_material_animation_destroy(a);
      a = NULL;
      bk_material_pose_destroy(pose);
      pose = NULL;
      bk_model_destroy(model);
      model = NULL;
      profiles++;
    }
    bk_archive_close(&archive);
  }
  assert(profiles == 78 && tracks == 194 && frames == 9360);
  printf("PASS MATA: %u models %u tracks %u frames %llu materials FNV%016llx\n",
         profiles, tracks, frames, (unsigned long long)materials,
         (unsigned long long)hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "profile%u: %s\n", profiles, e);
  bk_material_pose_destroy(pose);
  bk_material_animation_destroy(a);
  bk_model_destroy(model);
  bk_blob_free(&raw);
  bk_archive_close(&archive);
  return rc;
}
