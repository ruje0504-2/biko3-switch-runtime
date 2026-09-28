#include "game/npc_scene.h"
#include "resource/store.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  if (argc != 3 || strlen(argv[2]) > 64 || strchr(argv[2], '/') ||
      strchr(argv[2], '\\')) {
    fprintf(stderr, "usage: collision-probe DATA_DIRECTORY SCENE_BASENAME\n");
    return 2;
  }
  char error[256] = {0}, path[1024], model_name[80], atr_name[80],
       native_name[80];
  BkBlob model_blob = {0}, atr_blob = {0};
  BkModel *model = NULL;
  BkCollision *collision = NULL;
  float *world = NULL;
  int status = 1;
  uint32_t queries = 0, meshes = 0;
  BkResourceStore *store = bk_resources_create(error);
  if (!store)
    goto done;
  if (snprintf(path, sizeof(path), "%s/bk3_03.pp", argv[1]) >=
      (int)sizeof(path)) {
    snprintf(error, sizeof(error), "data path too long");
    goto done;
  }
  snprintf(model_name, sizeof(model_name), "%s.x", argv[2]);
  snprintf(atr_name, sizeof(atr_name), "%s.atr", argv[2]);
  snprintf(native_name, sizeof(native_name), "%s", model_name);
  for (char *p = native_name; *p; p++)
    if (*p >= 'a' && *p <= 'z')
      *p = (char)(*p - 'a' + 'A');
  if (!bk_resources_mount(store, "bk3_03", path, error) ||
      !bk_resources_mount_directory(store, "collision", argv[1],
                                    BK_COLLISION_ATR_SIZE, error) ||
      bk_resources_read(store, "bk3_03", model_name, &model_blob, error) !=
          BK_RESOURCE_OK ||
      bk_resources_read(store, "collision", atr_name, &atr_blob, error) !=
          BK_RESOURCE_OK ||
      bk_model_decode(model_blob.data, model_blob.size, &model, error) !=
          BK_MODEL_OK)
    goto done;
  size_t count = (size_t)model->frame_count * 16;
  world = calloc(count, sizeof(*world));
  if (!world) {
    snprintf(error, sizeof(error), "world allocation failed");
    goto done;
  }
  if (!bk_model_world_matrices(model, world, count, error))
    goto done;
  collision = bk_collision_create(model, world, count, native_name,
                                  atr_blob.data, atr_blob.size, error);
  if (!collision)
    goto done;
  /* Deliberately release all sources before querying the owned collision. */
  bk_model_destroy(model);
  model = NULL;
  free(world);
  world = NULL;
  bk_blob_free(&model_blob);
  bk_blob_free(&atr_blob);
  bk_resources_destroy(store);
  store = NULL;
  meshes = bk_collision_count(collision);
  for (uint32_t i = 0; i < meshes; i++) {
    const BkCollisionMesh *mesh = bk_collision_mesh(collision, i);
    for (uint32_t j = 0; j < mesh->index_count; j += 3) {
      BkNpcSceneState actor = {.behavior = 1};
      for (unsigned axis = 0; axis < 3; axis++) {
        double center = 0;
        for (unsigned k = 0; k < 3; k++)
          center += mesh->vertices[mesh->indices[j + k]][axis];
        actor.position[axis] = (float)(center / 3);
      }
      int hit;
      float height = actor.position[1];
      if (!bk_collision_ground(mesh, actor.position, &hit, &height, error))
        goto done;
      queries++;
      /* The actual all-mesh stage is exercised for a bounded sample. */
      if (j % 96 == 0) {
        BkNpcSceneInput input = {.suppressed_actions = {18, 19, 20, 21, 23, 27},
                                 .excluded_surface = ""};
        memcpy(input.cone.actor_position, actor.position,
               sizeof(actor.position));
        memcpy(input.cone.player_position, actor.position,
               sizeof(actor.position));
        input.cone.player_position[2] += 30;
        input.cone.short_range_action = 1;
        input.cone.head_distance = 30;
        memcpy(input.sight.start, input.cone.actor_position,
               sizeof(input.sight.start));
        memcpy(input.sight.end, input.cone.player_position,
               sizeof(input.sight.end));
        input.sight.start[1] += 18;
        input.sight.end[1] += 18;
        input.sight.distance = 30;
        if (!bk_npc_scene_step(&actor, collision, &input, 1.0f / 60, error))
          goto done;
      }
    }
  }
  printf("PASS %s: %u collision meshes, %u centroid ground queries and sampled "
         "NPC scene updates\n",
         argv[2], meshes, queries);
  status = 0;
done:
  if (status)
    fprintf(stderr, "%s: %s\n", argv[2], error);
  bk_collision_destroy(collision);
  free(world);
  bk_model_destroy(model);
  bk_blob_free(&model_blob);
  bk_blob_free(&atr_blob);
  bk_resources_destroy(store);
  return status;
}
