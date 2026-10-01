#include "game/npc_scene.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int bk_npc_scene_step(BkNpcSceneState *state, const BkCollision *collision,
                      const BkNpcSceneInput *input, float seconds,
                      char error[256]) {
  if (!state || !collision || !input || !input->excluded_surface ||
      !isfinite(seconds) || seconds < 0)
    goto invalid;
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(state->position[i]))
      goto invalid;
  if (state->hidden == 1)
    return 1;
  BkNpcSceneState next = *state;
  int visible;
  if (!bk_sight_cone(&visible, &input->cone))
    goto invalid;
  next.visible = (uint8_t)visible;
  memset(next.surface_name, 0, sizeof(next.surface_name));
  float original_y = state->position[1], best = -9999;
  uint32_t count = bk_collision_count(collision);
  if (count)
    for (unsigned i = 0; i < 3; i++)
      if (!isfinite(input->sight.start[i]) || !isfinite(input->sight.end[i]))
        goto invalid;
  for (uint32_t i = 0; i < count; i++) {
    const BkCollisionMesh *mesh = bk_collision_mesh(collision, i);
    /* Occlusion can only clear visibility. Ground queries below must still
     * visit every mesh in order, including after the first blocker. */
    for (uint32_t j = 0; next.visible && j < mesh->index_count; j += 3) {
      float t[3][3];
      for (unsigned k = 0; k < 3; k++)
        memcpy(t[k], mesh->vertices[mesh->indices[j + k]], sizeof(t[k]));
      int blocked;
      if (!bk_sight_triangle(&blocked, t, &input->sight))
        goto invalid;
      if (blocked)
        next.visible = 0;
    }
    int hit;
    float height = next.position[1];
    if (!bk_collision_ground(mesh, next.position, &hit, &height, error))
      return 0;
    next.position[1] = height;
    if (hit && (double)original_y + 15 >= height &&
        (double)original_y - 15 <= height && height >= best) {
      best = height;
      if (!strcmp(mesh->name, input->excluded_surface))
        memset(next.surface_name, 0, sizeof(next.surface_name));
      else if (!next.surface_name[0])
        memcpy(next.surface_name, mesh->name, sizeof(next.surface_name));
    }
  }
  int suppressed = 0;
  for (unsigned i = 0; i < 6; i++)
    if (input->cone.player_action == input->suppressed_actions[i])
      suppressed = 1;
  if (next.visible == 1 && !suppressed && next.behavior != 4)
    next.behavior = 4;
  float delta = (float)((double)best - original_y);
  delta = (float)((double)seconds * delta);
  next.position[1] = (float)((double)original_y + delta);
  if (!isfinite(delta) || !isfinite(next.position[1]))
    goto invalid;
  *state = next;
  return 1;
invalid:
  snprintf(error, 256, "NPC scene: invalid visibility/ground input");
  return 0;
}
