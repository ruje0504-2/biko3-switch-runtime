#include "scene/dialogue_render.h"
#include <stdlib.h>
struct BkDialogueRender {
  BkRenderer *renderer;
  BkDialogueWorld *world;
  BkActorRender *actor;
  BkActorRenderBatch *batch;
  BkLightSet *lights;
  BkActorRenderVisit *visits;
  uint32_t capacity;
  int ready;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "dialogue render: %s", why);
  return 0;
}
void bk_dialogue_render_destroy(BkDialogueRender *r) {
  if (!r)
    return;
  bk_actor_render_batch_destroy(r->batch);
  bk_actor_render_destroy(r->actor);
  bk_light_set_destroy(r->renderer, r->lights);
  free(r->visits);
  free(r);
}
BkDialogueRender *bk_dialogue_render_create(BkRenderer *renderer,
                                            BkResourceStore *store,
                                            BkDialogueWorld *world,
                                            char e[256]) {
  if (!renderer || !store || !world) {
    fail(e, "missing services");
    return NULL;
  }
  BkDialogueRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(e, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  r->world = world;
  BkDialogueActorAssets *body = bk_dialogue_world_body(world);
  const BkModel *m = bk_actor_pose_model(bk_dialogue_actor_assets_pose(body));
  r->capacity = bk_frame_tree_count(
      bk_actor_forest_tree(bk_dialogue_world_forest(world)));
  r->visits = calloc(r->capacity, sizeof(*r->visits));
  uint64_t parts = 0;
  for (uint32_t i = 0; i < m->frame_count; i++)
    if (m->frames[i].mesh_index != BK_MODEL_NONE)
      parts += m->meshes[m->frames[i].mesh_index].submesh_count;
  if (!parts || parts > UINT32_MAX || !r->visits) {
    fail(e, "invalid geometry capacity");
    goto bad;
  }
  r->actor = bk_actor_render_create(renderer, store, "bk3_01", m,
                                    bk_dialogue_actor_assets_eyes(body), e);
  r->batch = bk_actor_render_batch_create(renderer, (uint32_t)parts, e);
  r->lights = bk_light_set_create(renderer, &(BkLighting){0}, e);
  if (!r->actor || !r->batch || !r->lights)
    goto bad;
  return r;
bad:
  bk_dialogue_render_destroy(r);
  return NULL;
}
int bk_dialogue_render_prepare(BkDialogueRender *r, const BkMenuCamera *camera,
                               const BkFog *fog, char e[256]) {
  if (!r)
    return fail(e, "missing owner");
  r->ready = 0;
  if (!camera || !fog || !bk_fog_validate(fog, e))
    return fail(e, "invalid camera/fog");
  BkActorForest *forest = bk_dialogue_world_forest(r->world);
  const float *anchor = bk_actor_forest_world(forest, 1);
  for (unsigned i = 0; i < 16; i++)
    if (anchor[i] != camera->pose.world[i])
      return fail(e, "camera anchor mismatch");
  float projection[16];
  if (!bk_camera_projection(projection,
                            &(BkCameraLens){camera->fov, .75f, .5f, 126384}))
    return fail(e, "invalid lens");
  BkLightingPass pass;
  if (!bk_dialogue_world_pass(r->world, &pass, e))
    return 0;
  BkSceneLighting *lights = bk_dialogue_world_lighting(r->world);
  BkDialogueActorAssets *body = bk_dialogue_world_body(r->world);
  BkActorPose *pose = bk_dialogue_actor_assets_pose(body);
  unsigned objects = 0, flushes = 0;
  for (unsigned c = 0; c < pass.count; c++) {
    const BkLightingCommand *cmd = &pass.commands[c];
    if (cmd->kind == BK_PASS_AMBIENT || cmd->kind == BK_PASS_LIGHT_ENABLE) {
      if (!bk_scene_lighting_command(lights, cmd))
        return fail(e, "invalid light command");
    } else if (cmd->kind == BK_PASS_OBJECT) {
      if (objects || cmd->target != bk_dialogue_world_root(r->world))
        return fail(e, "unexpected root dispatch");
      const BkFrameVisit *walk;
      uint32_t count, submitted = 0;
      if (!bk_actor_forest_draw(forest, cmd->target, &walk, &count, e))
        return 0;
      for (uint32_t i = 0; i < count; i++) {
        uint32_t actor, frame;
        if (!walk[i].submit ||
            !bk_actor_forest_binding(forest, walk[i].node, &actor, &frame))
          continue;
        if (actor != 0 || submitted >= r->capacity)
          return fail(e, "unexpected frame binding");
        r->visits[submitted++] = (BkActorRenderVisit){r->actor, frame};
      }
      BkLighting values;
      const float *view = bk_actor_forest_view(forest);
      if (!bk_scene_lighting_values(
              lights, bk_actor_pose_frame(pose, 0),
              (size_t)bk_actor_pose_model(pose)->frame_count * 16, &values,
              e) ||
          !bk_light_set_update(r->renderer, r->lights, &values, e) ||
          !bk_light_set_view(r->renderer, r->lights, camera->pose.world + 12,
                             e) ||
          !bk_light_set_fog(r->renderer, r->lights, fog, view, e) ||
          !bk_actor_render_prepare(r->actor, pose, NULL,
                                   bk_dialogue_actor_assets_face(body), view,
                                   projection, e) ||
          !bk_actor_render_batch_prepare_visits(r->batch, r->visits, submitted,
                                                e))
        return 0;
      objects++;
    } else if (cmd->kind == BK_PASS_FLUSH) {
      if (objects != 1 || flushes)
        return fail(e, "unexpected flush");
      flushes++;
    } else
      return fail(e, "unsupported draw command");
  }
  if (objects != 1 || flushes != 1)
    return fail(e, "incomplete draw plan");
  r->ready = 1;
  return 1;
}
int bk_dialogue_render_draw(BkDialogueRender *r, char e[256]) {
  return r && r->ready ? bk_actor_render_batch_draw(r->batch, r->lights, e)
                       : fail(e, "no prepared snapshot");
}
