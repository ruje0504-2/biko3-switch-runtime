#include "scene/background_assets.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkModel *model;
  BkClipSet *clips;
  BkActorPose *pose;
  BkMaterialPose *materials;
} Object;
struct BkBackgroundAssets {
  uint32_t group, area;
  const BkBackgroundConfig *config;
  Object objects[3];
  BkCollision *collision;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "background assets: %s", message);
  return 0;
}
void bk_background_assets_destroy(BkBackgroundAssets *a) {
  if (!a)
    return;
  bk_collision_destroy(a->collision);
  for (unsigned i = 0; i < 3; ++i) {
    Object *o = &a->objects[i];
    bk_material_pose_destroy(o->materials);
    bk_actor_pose_destroy(o->pose);
    bk_clip_set_destroy(o->clips);
    bk_model_destroy(o->model);
  }
  free(a);
}
static int object_load(Object *o, BkResourceStore *resources, const char *pack,
                       const char *name, int select_zero, char error[256]) {
  BkBlob blob = {0};
  if (bk_resources_read(resources, pack, name, &blob, error) != BK_RESOURCE_OK)
    return 0;
  o->clips = bk_clip_set_decode(blob.data, blob.size, error);
  bk_blob_free(&blob);
  if (!o->clips)
    return 0;
  if (bk_resources_read(resources, pack, bk_clip_model_name(o->clips), &blob,
                        error) != BK_RESOURCE_OK)
    return 0;
  int ok =
      bk_model_decode(blob.data, blob.size, &o->model, error) == BK_MODEL_OK;
  bk_blob_free(&blob);
  if (!ok)
    return 0;
  uint32_t root = BK_MODEL_NONE;
  for (uint32_t f = 0; f < o->model->frame_count; ++f)
    if (o->model->frames[f].parent_index == BK_MODEL_NONE) {
      if (root != BK_MODEL_NONE)
        return fail(error, "multiple object roots");
      root = f;
    }
  const float zero[3] = {0};
  o->pose =
      bk_actor_pose_create_loaded(o->model, o->clips, root, zero, 0, error);
  if (!o->pose || (select_zero && !bk_actor_pose_select(o->pose, 0, 1, error)))
    return 0;
  /* Background models retain their asset root; actor placement at the
   * origin is only the constructor boundary, not an authored transform. */
  BkActorPlacement place = {0};
  memcpy(place.world, o->model->frames[root].local, 64);
  memcpy(place.position, place.world + 12, 12);
  if (!bk_actor_pose_place_exact(o->pose, &place, error))
    return 0;
  o->materials = bk_material_pose_create(o->model, error);
  return o->materials != NULL;
}
BkBackgroundAssets *bk_background_assets_create(BkResourceStore *resources,
                                                uint32_t group, uint32_t area,
                                                unsigned quality, int snow,
                                                char error[256]) {
  const BkBackgroundConfig *config = bk_background_config(group, area);
  if (!resources || !config || quality > 2 || (snow != 0 && snow != 1)) {
    fail(error, "invalid resources/profile/quality");
    return NULL;
  }
  BkBackgroundAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->group = group;
  a->area = area;
  a->config = config;
  const char *pack = quality ? "bk3_03" : "bk3_17";
  if (!object_load(&a->objects[0], resources, pack, config->clip, 1, error))
    goto bad;
  if (group == 2 && area == 8 &&
      !object_load(&a->objects[1], resources, pack, "m03_04_door.xan", 0,
                   error))
    goto bad;
  if (snow && group == 0 && area <= 6) {
    if (!object_load(&a->objects[2], resources, "bk3_20", "yuki.xan", 0, error))
      goto bad;
    BkClipState clock;
    /* Authored yuki current slot0 makes native40168c(0) a no-op. Reject
     * changed seeds until its distinct active-slot selection is supported. */
    if (!bk_actor_pose_state(a->objects[2].pose, &clock) || clock.slot != 0) {
      fail(error, "unsupported weather initial slot");
      goto bad;
    }
  }
  BkBlob atr = {0};
  if (bk_resources_read(resources, "collision", config->atr, &atr, error) !=
      BK_RESOURCE_OK)
    goto bad;
  Object *scene = &a->objects[0];
  char model_name[256];
  snprintf(model_name, sizeof(model_name), "%s",
           bk_clip_model_name(scene->clips));
  for (char *p = model_name; *p; ++p)
    *p = (char)toupper((unsigned char)*p);
  a->collision =
      bk_collision_create(scene->model, bk_actor_pose_frame(scene->pose, 0),
                          (size_t)scene->model->frame_count * 16, model_name,
                          atr.data, atr.size, error);
  bk_blob_free(&atr);
  if (!a->collision)
    goto bad;
  return a;
bad:
  bk_background_assets_destroy(a);
  return NULL;
}
const BkActorPose *bk_background_assets_pose(const BkBackgroundAssets *a,
                                             unsigned i) {
  return a && i < 3 ? a->objects[i].pose : NULL;
}
BkActorPose *bk_background_assets_bind_pose(BkBackgroundAssets *a, unsigned i) {
  return a && i < 3 ? a->objects[i].pose : NULL;
}
const BkMaterialPose *
bk_background_assets_materials(const BkBackgroundAssets *a, unsigned i) {
  return a && i < 3 ? a->objects[i].materials : NULL;
}
const BkBackgroundConfig *
bk_background_assets_config(const BkBackgroundAssets *a) {
  return a ? a->config : NULL;
}
BkCollision *bk_background_assets_collision(BkBackgroundAssets *a) {
  return a ? a->collision : NULL;
}
int bk_background_assets_input(const BkBackgroundAssets *a,
                               BkBackgroundInput *in) {
  if (!a || !in)
    return 0;
  BkClipState clip;
  if (!bk_actor_pose_state(a->objects[0].pose, &clip))
    return 0;
  in->group = (int32_t)a->group;
  in->area = (int32_t)a->area;
  in->background_clip = clip.slot;
  in->background_present = 1;
  in->door_clip = -1;
  if (a->objects[1].pose) {
    if (!bk_actor_pose_state(a->objects[1].pose, &clip))
      return 0;
    in->door_clip = clip.slot;
  }
  in->weather_present = a->objects[2].pose != NULL;
  return 1;
}
int bk_background_assets_step(BkBackgroundAssets *a, BkBackgroundState *state,
                              uint32_t *random, const BkBackgroundInput *input,
                              BkBackgroundCommands *out,
                              BkBackgroundConsumer consume, void *context,
                              char error[256]) {
  if (!a || !state || !random || !input || !out)
    return fail(error, "missing frame inputs");
  BkBackgroundInput in = *input;
  BkBackgroundState next = *state;
  uint32_t rng = *random;
  BkBackgroundCommands commands;
  if (!bk_background_assets_input(a, &in) ||
      !bk_background_step(&next, &rng, &in, &commands))
    return fail(error, "invalid frame policy");
  if (consume && !consume(context, &commands, error))
    return 0;
  *state = next;
  *random = rng;
  *out = commands;
  if (a->objects[1].pose && commands.door_advance &&
      !bk_actor_pose_advance(a->objects[1].pose, commands.door_request,
                             commands.seconds, error))
    return 0;
  if (commands.background_advance &&
      !bk_actor_pose_advance(a->objects[0].pose, -1, commands.seconds, error))
    return 0;
  if (commands.weather_advance) {
    const float local[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 10, 1};
    if (!bk_actor_pose_root_local(a->objects[2].pose, local, error) ||
        !bk_actor_pose_advance(a->objects[2].pose, 0, commands.weather_seconds,
                               error))
      return 0;
  }
  return 1;
}
void bk_background_assets_publish(BkBackgroundAssets *a) {
  if (a)
    for (unsigned i = 0; i < 3; ++i)
      if (a->objects[i].pose)
        bk_actor_pose_publish(a->objects[i].pose);
}
int bk_background_assets_publish_camera(BkBackgroundAssets *a,
                                        const float camera_world[16],
                                        char error[256]) {
  if (!a)
    return fail(error, "missing background instance");
  if (a->objects[2].pose &&
      !bk_actor_pose_publish_under(a->objects[2].pose, camera_world, error))
    return 0;
  for (unsigned i = 0; i < 2; ++i)
    if (a->objects[i].pose)
      bk_actor_pose_publish(a->objects[i].pose);
  return 1;
}
