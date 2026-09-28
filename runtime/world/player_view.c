#include "world/player_view.h"
#include "core/matrix.h"
#include "world/placement.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static float add(float a, float b) { return (float)((double)a + b); }
static float sub(float a, float b) { return (float)((double)a - b); }
static float mul(float a, float b) { return (float)((double)a * b); }
static int finite3(const float *p) {
  return isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]);
}
int bk_player_view_collision_query(const BkPlayerView *view,
                                   BkPlayerWallInput *out) {
  if (!view || !out || !finite3(view->probe))
    return 0;
  for (unsigned i = 0; i < 7; i++)
    if (!finite3(view->rays[i]))
      return 0;
  memcpy(out->camera, view->probe, sizeof(out->camera));
  memcpy(out->rays, view->rays, sizeof(out->rays));
  return 1;
}
static int rotate(BkPlayerView *s, float pitch) {
  float x = (float)((double)pitch * .01745329238474369f);
  float y = (float)((double)s->yaw * .01745329238474369f);
  float sx = (float)sin((double)x), cx = (float)cos((double)x);
  float sy = (float)sin((double)y), cy = (float)cos((double)y);
  const float a[16] = {1, 0, 0, 0, 0, cx, sx, 0, 0, -sx, cx, 0, 0, 0, 0, 1};
  const float b[16] = {cy, 0, -sy, 0, 0, 1, 0, 0, sy, 0, cy, 0, 0, 0, 0, 1};
  bk_matrix_multiply(s->matrix, a, b);
  memcpy(s->matrix + 12, s->pose.position, 12);
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(s->matrix[i]))
      return 0;
  memcpy(s->pose.world, s->matrix, 64);
  return 1;
}
static int yaw(BkPlayerView *s, float value) {
  return bk_angle_blend_degrees(&s->yaw, value, s->yaw, 1);
}
static int yaw_pitch(BkPlayerView *s, const BkPlayerViewInput *in) {
  return yaw(s, in->yaw) &&
         bk_angle_blend_degrees(&s->pitch, in->pitch, s->pitch, 1);
}
static void offset(float *x, float *z, float degrees, double radius) {
  double radians = (double)degrees * .01745;
  *x = (float)(sin(radians) * radius);
  *z = (float)(cos(radians) * radius);
}
static int orbit(BkPlayerView *s, const BkPlayerViewInput *in) {
  float dx, dz;
  offset(&dx, &dz, in->yaw, -40);
  float dy = (float)(sin((double)in->pitch * .01745) * -40);
  s->probe[0] = add(in->position[0], dx);
  s->probe[1] = add(in->vertical, dy);
  s->probe[2] = add(in->position[2], dz);
  float weight = mul(in->seconds, 3);
  if (weight >= 1)
    weight = 1;
  /* FST without pop: retain the unrounded distance subtraction for multiply. */
  float change = (float)(((double)s->target_distance - s->distance) * weight);
  s->distance = add(s->distance, change);
  offset(&dx, &dz, in->yaw, -(double)s->distance);
  dy = (float)(sin((double)in->pitch * .01745) * -(double)s->distance);
  float cp = (float)cos((double)in->pitch * .01745);
  dx = mul(dx, cp);
  dz = mul(dz, cp);
  float tx = add(dx, in->position[0]), tz = add(dz, in->position[2]);
  s->pose.position[0] = add(s->pose.position[0], sub(tx, s->pose.position[0]));
  s->pose.position[1] = sub(in->vertical, dy);
  s->pose.position[2] = add(s->pose.position[2], sub(tz, s->pose.position[2]));
  if (!yaw_pitch(s, in) || !rotate(s, s->pitch))
    return 0;
  double nx = (double)in->npc_position[0] - in->position[0],
         nz = (double)in->npc_position[2] - in->position[2];
  s->npc_distance = (float)sqrt(nx * nx + nz * nz);
  if (!bk_route_heading(&s->npc_heading, in->npc_position[0],
                        in->npc_position[2], in->position[0], in->position[2]))
    return 0;
  double radians = (double)s->npc_heading * .01745329238474369f;
  s->focus[0] = (float)(sin(radians) * ((double)s->npc_distance / 2) +
                        in->npc_position[0]);
  s->focus[1] = add(in->npc_position[1], 15);
  s->focus[2] = (float)(cos(radians) * ((double)s->npc_distance / 2) +
                        in->npc_position[2]);
  static const float angles[6] = {45, 90, 135, -45, -90, -135};
  for (unsigned i = 0; i < 6; ++i) {
    radians = ((double)s->npc_heading + angles[i]) * .01745329238474369f;
    s->rays[i][0] = (float)(sin(radians) * s->npc_distance + s->focus[0]);
    s->rays[i][1] = add(s->focus[1], 20);
    s->rays[i][2] = (float)(cos(radians) * s->npc_distance + s->focus[2]);
  }
  radians = (double)in->npc_yaw * .01745329238474369f;
  s->rays[6][0] = (float)(sin(radians) * 30 + in->npc_position[0]);
  s->rays[6][1] = add(in->npc_position[1], in->npc_height);
  s->rays[6][2] = (float)(cos(radians) * 30 + in->npc_position[2]);
  s->target_distance = 40;
  memset(s->blocked, 0, sizeof(s->blocked));
  return 1;
}
int bk_player_view_step(BkPlayerView *state, BkPlayerViewKind kind,
                        const BkPlayerViewInput *in,
                        BkPlayerViewEffects *effects, char error[256]) {
  if (!state || !in || !effects || kind < BK_PLAYER_VIEW_HOLD ||
      kind > BK_PLAYER_VIEW_HEAD || !isfinite(in->seconds) || in->seconds < 0 ||
      (in->buttons & ~3u))
    goto invalid;
  BkPlayerView s = *state;
  BkPlayerViewEffects out = {.root_hidden = -1, .player_hidden = -1};
  switch (kind) {
  case BK_PLAYER_VIEW_HOLD:
    break;
  case BK_PLAYER_VIEW_ORBIT:
    if (!orbit(&s, in))
      goto invalid;
    out.root_hidden = out.player_hidden = 0;
    break;
  case BK_PLAYER_VIEW_RAY: {
    s.selected_ray = 6;
    for (unsigned i = 0; i < 6; ++i)
      if (!s.blocked[i]) {
        s.selected_ray = (int32_t)i;
        break;
      }
    /* Aim from PREVIOUS rendered position, then install selected candidate.
     * Smoothing XYZ and +4a4 are intentionally held in this controller. */
    if (!bk_camera_aim(s.pose.world, s.pose.world, s.focus) ||
        !finite3(s.rays[s.selected_ray]))
      goto invalid;
    memcpy(s.pose.world + 12, s.rays[s.selected_ray], 12);
    memset(s.blocked, 0, sizeof(s.blocked));
    break;
  }
  case BK_PLAYER_VIEW_PROP_HEAD:
  case BK_PLAYER_VIEW_PROP_LOW:
  case BK_PLAYER_VIEW_WALL:
    memcpy(s.pose.position, in->position, 12);
    s.pose.position[1] =
        kind == BK_PLAYER_VIEW_PROP_HEAD
            ? in->head[1]
            : add(in->position[1], kind == BK_PLAYER_VIEW_PROP_LOW ? 1 : 20);
    if (!yaw(&s, in->yaw) || !rotate(&s, 0))
      goto invalid;
    out.root_hidden = 1;
    if (kind != BK_PLAYER_VIEW_WALL) {
      out.write_return_yaw = 1;
      out.return_yaw = s.yaw;
    }
    break;
  case BK_PLAYER_VIEW_TRACK: {
    if (!isfinite(in->track_source) || !isfinite(in->track_end))
      goto invalid;
    for (unsigned i = 0; i < 3; ++i)
      s.pose.position[i] =
          add(s.pose.position[i],
              mul(sub(in->track[i], s.pose.position[i]), in->seconds));
    const float target[3] = {in->position[0], add(in->position[1], 20),
                             in->position[2]};
    if (!bk_camera_aim(s.pose.world, s.pose.world, target) ||
        !finite3(s.pose.position))
      goto invalid;
    memcpy(s.pose.world + 12, s.pose.position, 12);
    out.reset_mode = in->track_source >= in->track_end;
    break;
  }
  case BK_PLAYER_VIEW_COVER: {
    float dx, dz;
    offset(&dx, &dz, in->yaw, -30);
    float tx = add(in->position[0], dx), tz = add(in->position[2], dz);
    tx = add(s.pose.position[0], sub(tx, s.pose.position[0]));
    tz = add(s.pose.position[2], sub(tz, s.pose.position[2]));
    if (in->buttons & 1)
      s.lean = -10;
    else if (in->buttons & 2)
      s.lean = 10;
    double angle = ((double)in->yaw + 90) * .01745;
    dx = (float)(sin(angle) * s.lean);
    dz = (float)(cos(angle) * s.lean);
    tx = add(tx, dx);
    tz = add(tz, dz);
    s.pose.position[0] =
        add(s.pose.position[0], mul(sub(tx, s.pose.position[0]), .5f));
    s.pose.position[1] = in->vertical;
    s.pose.position[2] =
        add(s.pose.position[2], mul(sub(tz, s.pose.position[2]), .5f));
    s.pitch = 0;
    if (!yaw(&s, in->yaw) || !rotate(&s, 0))
      goto invalid;
    break;
  }
  case BK_PLAYER_VIEW_NPC_FRONT: {
    float dx, dz;
    offset(&dx, &dz, in->npc_yaw, 30);
    float tx = add(in->npc_position[0], dx), tz = add(in->npc_position[2], dz);
    s.pose.position[0] = add(s.pose.position[0], sub(tx, s.pose.position[0]));
    /* 4ba87c receives NPC as its actor. */
    s.pose.position[1] = in->npc_vertical;
    s.pose.position[2] = add(s.pose.position[2], sub(tz, s.pose.position[2]));
    s.pitch = 0;
    if (!yaw(&s, add(in->npc_yaw, 180)) || !rotate(&s, 0))
      goto invalid;
    break;
  }
  case BK_PLAYER_VIEW_HEAD:
    memcpy(s.pose.position, in->head, 12);
    if (!yaw_pitch(&s, in) || !rotate(&s, s.pitch))
      goto invalid;
    break;
  }
  /* State includes held slots which may be arbitrary; validate only all new
   * outputs for the controller, retaining untouched data verbatim. */
  if (kind == BK_PLAYER_VIEW_ORBIT) {
    if (!finite3(s.probe) || !finite3(s.focus) || !isfinite(s.npc_distance) ||
        !isfinite(s.distance))
      goto invalid;
    for (unsigned i = 0; i < 7; ++i)
      if (!finite3(s.rays[i]))
        goto invalid;
  }
  *state = s;
  *effects = out;
  return 1;
invalid:
  if (error)
    snprintf(error, 256, "Invalid player view controller");
  return 0;
}
