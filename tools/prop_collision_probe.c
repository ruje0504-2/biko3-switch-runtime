/* Explicit actual-prop geometry fixture; no guessed prop spawn/animation. */
#include "resource/store.h"
#include "world/collision.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: prop-collision-probe DATA\n");
    return 2;
  }
  static const char *const names[] = {
      "h93_00.x", "train_4ryou.x", "h98_00.x", "h93_10.x", "h93_11.x",
      "h93_12.x", "h93_13.x",      "h93_14.x", "h98_20.x", "h98_30.x"};
  static const int32_t kinds[] = {0, 1, 10, 13, 14, 15, 16, 17, 18, 19};
  char error[256] = {0}, path[1024], filenames[10][80];
  BkResourceStore *store = bk_resources_create(error);
  BkModel *models[10] = {0};
  float *world[10] = {0};
  BkCollision *collision = NULL;
  BkBlob blob = {0};
  uint8_t *atr = calloc(1, BK_COLLISION_ATR_SIZE);
  int status = 1;
  unsigned queries = 0, meshes = 0;
  if (!store || !atr ||
      snprintf(path, sizeof(path), "%s/bk3_07.pp", argv[1]) >=
          (int)sizeof(path) ||
      !bk_resources_mount(store, "bk3_07", path, error))
    goto done;
  for (unsigned i = 0; i < 10; ++i) {
    if (bk_resources_read(store, "bk3_07", names[i], &blob, error) !=
            BK_RESOURCE_OK ||
        bk_model_decode(blob.data, blob.size, &models[i], error) != BK_MODEL_OK)
      goto done;
    bk_blob_free(&blob);
    size_t count = models[i]->frame_count * 16;
    world[i] = calloc(count, sizeof(float));
    if (!world[i] ||
        !bk_model_world_matrices(models[i], world[i], count, error))
      goto done;
    snprintf(filenames[i], sizeof(filenames[i]), "%s", names[i]);
    for (char *p = filenames[i]; *p; ++p)
      if (*p >= 'a' && *p <= 'z')
        *p += 'A' - 'a';
  }
  collision =
      bk_collision_create(models[0], world[0], models[0]->frame_count * 16,
                          "BASE.X", atr, BK_COLLISION_ATR_SIZE, error);
  if (!collision)
    goto done;
  for (unsigned frame = 0; frame < 120; ++frame) {
    BkCollisionProp props[16] = {0};
    for (unsigned i = 0; i < 16; ++i) {
      unsigned index = (frame + i) % 10;
      props[i] =
          (BkCollisionProp){.active = (frame + i) % 7 != 0,
                            .kind = kinds[index],
                            .model = models[index],
                            .world = world[index],
                            .world_floats = models[index]->frame_count * 16,
                            .model_name = filenames[index],
                            .position = {(float)i * 30, NAN, (float)frame}};
    }
    if (!bk_collision_begin_props(collision, props, 16, error))
      goto done;
    uint32_t count = bk_collision_count(collision);
    if (!count)
      goto done;
    for (uint32_t i = 0; i < count; ++i) {
      const BkCollisionMesh *m = bk_collision_mesh(collision, i);
      int8_t kind;
      if (!bk_collision_kind(collision, i, &kind) || kind < 0 ||
          !m->index_count)
        goto done;
      float point[3] = {0};
      for (unsigned axis = 0; axis < 3; ++axis)
        point[axis] = (float)(((double)m->vertices[m->indices[0]][axis] +
                               m->vertices[m->indices[1]][axis] +
                               m->vertices[m->indices[2]][axis]) /
                              3);
      int hit;
      float y = point[1];
      if (!bk_collision_ground(m, point, &hit, &y, error))
        goto done;
      ++queries;
      ++meshes;
    }
    /* Replacing without clear is supported; also check idempotent end. */
    if (frame % 2 && frame + 1 < 120) {
      bk_collision_end_props(collision);
      bk_collision_end_props(collision);
    }
  }
  /* Inputs can be freed before the owned geometry is queried/cleared. */
  for (unsigned i = 0; i < 10; ++i) {
    bk_model_destroy(models[i]);
    models[i] = NULL;
    free(world[i]);
    world[i] = NULL;
  }
  const BkCollisionMesh *retained = bk_collision_mesh(collision, 0);
  if (!retained || !isfinite(retained->vertices[0][0]))
    goto done;
  bk_collision_end_props(collision);
  if (bk_collision_count(collision))
    goto done;
  printf("PASS 10 actual prop models, 120 replacements, %u meshes and %u "
         "queries, cleanup\n",
         meshes, queries);
  status = 0;
done:
  if (status)
    fprintf(stderr, "FAIL props: %s\n", error);
  bk_collision_destroy(collision);
  bk_blob_free(&blob);
  free(atr);
  for (unsigned i = 0; i < 10; ++i) {
    free(world[i]);
    bk_model_destroy(models[i]);
  }
  bk_resources_destroy(store);
  return status;
}
