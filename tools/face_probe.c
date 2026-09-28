#include "model/clip.h"
#include "scene/face_assets.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: face-probe Data-directory\n");
    return 2;
  }
  char error[256] = {0}, path[4096];
  BkResourceStore *store = bk_resources_create(error);
  BkBlob data = {0};
  BkClipSet *clips = NULL;
  BkModel *model = NULL;
  BkFaceAssets *face = NULL;
  int rc = 1;
  int n = snprintf(path, sizeof(path), "%s/bk3_01.pp", argv[1]);
  if (n < 0 || (size_t)n >= sizeof(path) || !store ||
      !bk_resources_mount(store, "bk3_01", path, error) ||
      !bk_resources_mount_directory(store, "faces", argv[1], 20480, error))
    goto done;
  const unsigned variants[] = {60, 61, 70, 80};
  for (unsigned group = 1; group <= 5; group++)
    for (unsigned variant = 0; variant < 4; variant++) {
      char name[32], config[32];
      snprintf(name, sizeof(name), "h%02u_%02u.xan", group, variants[variant]);
      snprintf(config, sizeof(config), "h%02u_%02u.fam", group,
               variants[variant]);
      if (bk_resources_read(store, "bk3_01", name, &data, error) !=
          BK_RESOURCE_OK)
        goto done;
      clips = bk_clip_set_decode(data.data, data.size, error);
      bk_blob_free(&data);
      if (!clips ||
          bk_resources_read(store, "bk3_01", bk_clip_model_name(clips), &data,
                            error) != BK_RESOURCE_OK ||
          bk_model_decode(data.data, data.size, &model, error) != BK_MODEL_OK)
        goto done;
      bk_blob_free(&data);
      face = bk_face_assets_create(store, "faces", config, "bk3_01", model,
                                   bk_clip_model_name(clips), error);
      if (!face)
        goto done;
      /* Adapter owns decoded keys and targets, not these source lifetimes. */
      bk_clip_set_destroy(clips);
      clips = NULL;
      bk_model_destroy(model);
      model = NULL;
      BkFaceState state;
      uint32_t random = 1, clocks[4] = {0, 1, 2, 3};
      if (!bk_face_assets_initialize(face, &state, clocks, &random, error))
        goto done;
      for (unsigned frame = 0; frame < 100; frame++) {
        uint32_t now = 100 + frame * 137;
        if (!bk_face_assets_step(face, &state, (int32_t)(frame / 11 % 4),
                                 (float)(frame % 10), now, now + 1, now + 2,
                                 now + 3, &random, error))
          goto done;
      }
      bk_face_assets_destroy(face);
      face = NULL;
    }
  puts("PASS: 20 face assets, owned resource lifetimes, 2000 CPU "
       "controller/vertex steps");
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "face-probe: %s\n", error);
  bk_blob_free(&data);
  bk_face_assets_destroy(face);
  bk_model_destroy(model);
  bk_clip_set_destroy(clips);
  bk_resources_destroy(store);
  return rc;
}
