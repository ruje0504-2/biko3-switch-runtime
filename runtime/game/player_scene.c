#include "game/player_scene.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int bk_player_scene_in_view(int *visible, const BkPlayerSceneInput *in) {
  if (!visible || !in || !isfinite(in->head_distance) ||
      !isfinite(in->projected_depth) || !isfinite(in->screen_scale[0]) ||
      !isfinite(in->screen_scale[1]) || in->screen_scale[0] <= 0 ||
      in->screen_scale[1] <= 0)
    return 0;
  *visible = !in->npc_hidden && in->head_distance <= 100 &&
             in->projected_depth < 1 && in->screen_position[0] >= 0 &&
             in->screen_position[0] <= 1024.0 * in->screen_scale[0] &&
             in->screen_position[1] >= 0 &&
             in->screen_position[1] <= 768.0 * in->screen_scale[1];
  return 1;
}
static int wall_heading(float *out, const float normal[3]) {
  const double degrees = 57.29577791868205;
  float x = (float)(asin(normal[0]) * degrees);
  float z = (float)(acos(normal[2]) * degrees);
  if (!isfinite(x) || !isfinite(z))
    return 0;
  *out = x >= 0 ? z : (float)(360.0 - z);
  return 1;
}
int bk_player_scene_step(BkPlayerScene *state, const BkCollision *collision,
                         const BkPlayerSceneInput *in, char error[256]) {
  int visible;
  if (!state || !collision || !in || !in->excluded_surface ||
      !memchr(in->excluded_surface, 0, 260) ||
      !bk_player_scene_in_view(&visible, in))
    goto invalid;
  BkPlayerScene next = *state;
  next.npc_in_view = visible;
  next.any_wall = 0;
  next.wall.near_wall = 0;
  next.wall_heading = 0;
  memset(next.wall_name, 0, sizeof(next.wall_name));
  memset(next.surface_name, 0, sizeof(next.surface_name));
  const float threshold = (float)cos(1.047);
  if (!bk_player_wall_validate(&next.wall, &in->wall))
    goto invalid;
  uint32_t count = bk_collision_count(collision);
  for (unsigned pass = 0; pass < 2; ++pass)
    for (uint32_t i = 0; i < count; ++i) {
      const BkCollisionMesh *m = bk_collision_mesh(collision, i);
      if (!m || !m->vertices || !m->indices || !m->normals ||
          m->index_count % 3 || !memchr(m->name, 0, sizeof(m->name)))
        goto invalid;
      for (uint32_t j = 0; j < m->index_count; j += 3) {
        const float *normal = m->normals[j / 3];
        if (!isfinite(normal[0]) || !isfinite(normal[1]) ||
            !isfinite(normal[2]))
          goto invalid;
        if (normal[1] >= threshold)
          continue;
        float t[3][3];
        for (unsigned k = 0; k < 3; ++k) {
          if (m->indices[j + k] >= m->vertex_count)
            goto invalid;
          memcpy(t[k], m->vertices[m->indices[j + k]], sizeof(t[k]));
        }
        if (pass) {
          if (!bk_player_wall_restore(next.wall.position, in->wall.previous,
                                      in->wall.height, t, error))
            return 0;
        } else {
          int near;
          if (!bk_player_wall_triangle(&next.wall, &near, &in->wall, t, error))
            return 0;
          if (!near)
            continue;
          next.any_wall = 1;
          if (m->name[0]) {
            if (strcmp(m->name, in->excluded_surface))
              memcpy(next.wall_name, m->name, sizeof(next.wall_name));
            if (!wall_heading(&next.wall_heading, normal))
              goto invalid;
          }
        }
      }
    }
  if (next.any_wall && !next.wall_name[0])
    snprintf(next.wall_name, sizeof(next.wall_name), "%s",
             in->excluded_surface);
  float original_y = next.wall.position[1], best = -9999;
  for (uint32_t i = 0; i < count; ++i) {
    const BkCollisionMesh *m = bk_collision_mesh(collision, i);
    float height = next.wall.position[1];
    int hit;
    if (!bk_collision_ground(m, next.wall.position, &hit, &height, error))
      return 0;
    next.wall.position[1] = height;
    if (hit && (double)original_y + 15 >= height &&
        (double)original_y - 15 <= height && height >= best) {
      best = height;
      if (!strcmp(m->name, in->excluded_surface))
        memset(next.surface_name, 0, sizeof(next.surface_name));
      else if (!next.surface_name[0])
        memcpy(next.surface_name, m->name, sizeof(next.surface_name));
    }
  }
  /* FST stores a float delta but the following FMUL still uses its unrounded
   * x87 value; round only the .4 product and final addition. */
  float delta = (float)(((double)best - original_y) * 0.4);
  next.wall.position[1] = (float)((double)original_y + delta);
  if (!isfinite(next.wall.position[1]))
    goto invalid;
  *state = next;
  return 1;
invalid:
  if (error)
    snprintf(error, 256, "Invalid player scene query");
  return 0;
}
