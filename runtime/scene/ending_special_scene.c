#include "scene/ending_special_scene.h"
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending special scene: %s", why);
  return 0;
}
static int draw(void *p, const BkDrawDispatch *d, char e[256]) {
  const BkEndingSpecialScene *s = p;
  memcpy(s->camera->pose.world, bk_actor_forest_world(s->forest, 1), 64);
  return s->draw(s->render_context, d, s->camera, e);
}
static int camera_read(void *p, float position[3], float forward[3],
                       float up[3], char e[256]) {
  const BkEndingSpecialScene *s = p;
  const float *world = bk_actor_forest_world(s->forest, 1);
  if (!bk_node_reference_offset(position, world,
                                bk_actor_forest_world(s->forest, 0), e))
    return 0;
  /*423564's global-reference branch reads published world columns directly. */
  for (unsigned i = 0; i < 3; i++) {
    forward[i] = world[i * 4 + 2];
    up[i] = world[i * 4 + 1];
  }
  return 1;
}
static int commit(const BkEndingSpecialScene *s, const BkNodeReference *n,
                  char e[256]) {
  if (!bk_actor_forest_commit_anchor_reference(s->forest, 1, n, e))
    return 0;
  memcpy(s->camera->pose.world, n->world, 64);
  return 1;
}
static int camera_position(void *p, const float position[3], char e[256]) {
  const BkEndingSpecialScene *s = p;
  BkNodeReference n;
  return bk_actor_forest_anchor_reference(s->forest, 1, &n, e) &&
         bk_node_reference_position(&n, bk_actor_forest_world(s->forest, 0),
                                    position, e) &&
         commit(s, &n, e);
}
static int camera_orientation(void *p, const float forward[3],
                              const float up[3], char e[256]) {
  const BkEndingSpecialScene *s = p;
  BkNodeReference n;
  return bk_actor_forest_anchor_reference(s->forest, 1, &n, e) &&
         bk_node_reference_orientation(&n, bk_actor_forest_world(s->forest, 0),
                                       forward, up, e) &&
         commit(s, &n, e);
}
static int camera_aim(void *p, const float target[3], char e[256]) {
  const BkEndingSpecialScene *s = p;
  BkNodeReference n;
  return bk_actor_forest_anchor_reference(s->forest, 1, &n, e) &&
         bk_node_reference_aim(&n, target, e) && commit(s, &n, e);
}
static int camera_publish(void *p, char e[256]) {
  return bk_actor_forest_camera_publish(
      ((const BkEndingSpecialScene *)p)->forest, e);
}
static int render_event(void *p, BkEndingSpecialRenderEvent event, unsigned arg,
                        char e[256]) {
  const BkEndingSpecialScene *s = p;
  return s->render_event(s->render_context, event, arg, e);
}
static int find_node(void *p, uint32_t root, const char *name, uint32_t *node,
                     char e[256]) {
  uint32_t found;
  if (!bk_actor_forest_find(((const BkEndingSpecialScene *)p)->forest, root,
                            name, &found, e))
    return 0;
  *node = found == BK_FRAME_NONE ? 0 : found;
  return 1;
}
static int hide_node(void *p, uint32_t node, uint32_t hidden, char e[256]) {
  return bk_actor_forest_visibility(((const BkEndingSpecialScene *)p)->forest,
                                    node, hidden, e);
}
static int material(void *p, const char *name, int mode, float alpha,
                    char e[256]) {
  const BkEndingSpecialScene *s = p;
  for (size_t i = 0; i < s->material_count; i++) {
    const BkEndingSpecialMaterial *binding = s->materials + i;
    const BkModelMaterial *m =
        bk_material_pose_material(binding->pose, binding->index);
    if (strcmp(m->name, name))
      continue;
    BkMaterialValuesEdit edit = {.index = binding->index, .id = m->id};
    memcpy(edit.values.diffuse, m->diffuse, 16);
    memcpy(edit.values.ambient, m->ambient, 16);
    memcpy(edit.values.specular, m->specular, 16);
    memcpy(edit.values.emissive, m->emissive, 16);
    edit.values.power = m->power;
    edit.values.diffuse[3] = mode ? 0 : alpha;
    return bk_material_pose_values(binding->pose, &edit, 1, e);
  }
  return 1;
}
int bk_ending_special_scene_draw(const BkEndingSpecialScene *s,
                                 const BkEndingSpecialBindings *bindings,
                                 BkDrawDispatch *dispatch, char e[256]) {
  if (!s || !s->forest || !s->camera || !s->draw || !s->render_event ||
      (s->material_count && !s->materials))
    return fail(e, "missing scene/render bindings");
  for (size_t i = 0; i < s->material_count; i++)
    if (!bk_material_pose_material(s->materials[i].pose, s->materials[i].index))
      return fail(e, "invalid material registry");
  BkEndingSpecialOps ops = {
      (void *)s,          draw,       camera_read,    camera_position,
      camera_orientation, camera_aim, camera_publish, render_event,
      find_node,          hide_node,  material};
  return bk_ending_special_draw(bindings, dispatch, &ops, e);
}
