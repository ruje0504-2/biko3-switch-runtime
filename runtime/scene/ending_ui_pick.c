#include "scene/ending_ui_pick.h"
#include "scene/ending_target.h"
#include "scene/ending_ui_geometry.h"
#include "scene/ending_secondary_ui.h"
#include "core/matrix.h"
#include <math.h>
#include <stdio.h>

static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending UI pick: %s", why);
  return 0;
}
static int finite_matrix(const float *m) {
  if (!m)
    return 0;
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(m[i]))
      return 0;
  return 1;
}
int bk_ending_ui_camera_sector(const float local[16], int32_t cached,
                               int32_t low, int32_t high, int *outside,
                               int *category) {
  if (!local || !outside || !category || !isfinite(local[8]) ||
      !isfinite(local[10]))
    return 0;
  /*42ee68 stores degrees after using its authored float pi; the atan2
   * result is NOT first rounded to float by its non-popping fst. */
  float yaw =
      (float)(atan2((double)local[8], local[10]) * 180.0 / 3.141592025756836);
  if (yaw < -180)
    yaw = (float)((double)yaw + 360);
  else if (yaw > 180)
    yaw = (float)((double)yaw - 360);
  if (yaw < 0)
    yaw = (float)(360.0 - fabs((double)yaw));
  int out = !((double)high > yaw && (double)low < yaw);
  int special = cached == 26 || cached == 27;
  *outside = out;
  *category = out ? (special ? 1 : 2) : (special ? 3 : 4);
  return 1;
}
static float length2(float x, float y) {
  float squared = (float)((double)x * x + (double)y * y);
  return (float)sqrt((double)squared);
}
int bk_ending_ui_segment_hit(const float a[2], const float b[2],
                             const float p[2], float radius, int *hit) {
  if (!a || !b || !p || !hit || isnan(radius) || radius < 0)
    return 0;
  float ab[2], ap[2];
  for (unsigned i = 0; i < 2; i++) {
    if (!isfinite(a[i]) || !isfinite(b[i]) || !isfinite(p[i]))
      return 0;
    ab[i] = (float)((double)b[i] - a[i]);
    ap[i] = (float)((double)p[i] - a[i]);
  }
  float length = length2(ab[0], ab[1]);
  if (!isfinite(length))
    return 0;
  if (length == 0) {
    *hit = 0;
    return 1;
  }
  float t = (float)(((double)ap[0] * ab[0] + (double)ap[1] * ab[1]) /
                    ((double)length * length));
  if (!isfinite(t))
    return 0;
  if (t < 0 || t > 1) {
    *hit = 0;
    return 1;
  }
  float delta[2];
  for (unsigned i = 0; i < 2; i++) {
    float offset = (float)((double)t * ab[i]);
    float point = (float)((double)a[i] + offset);
    delta[i] = (float)((double)p[i] - point);
  }
  float distance = length2(delta[0], delta[1]);
  if (!isfinite(distance))
    return 0;
  *hit = distance < radius;
  return 1;
}
static int project_integer(int32_t xy[2], const float world[16],
                            const float screen[16]) {
  float m[16];
  bk_matrix_multiply(m, world, screen);
  if (!isfinite(m[15]))
    return 0;
  if (m[15] == 0) {
    xy[0] = xy[1] = -10000;
    return 1;
  }
  for (unsigned i = 0; i < 2; i++) {
    double value = (double)m[12 + i] / m[15];
    if (!isfinite(value) || value <= -2147483649.0 || value >= 2147483648.0)
      return 0;
    xy[i] = (int32_t)value;
  }
  return 1;
}
static int project(float xy[2], const float world[16], const float screen[16]) {
  int32_t point[2];
  if (!project_integer(point, world, screen))
    return 0;
  xy[0] = (float)point[0];
  xy[1] = (float)point[1];
  return 1;
}
int bk_ending_ui_project_target(const BkEndingUiPickBindings *b, unsigned node,
                                 int32_t point[2], char e[256]) {
  if (!b || !point || !b->world || !b->present || node >= b->count ||
      !b->present[node] || !finite_matrix(b->world[node]) ||
      !finite_matrix(b->view) || !finite_matrix(b->projection) ||
      !finite_matrix(b->viewport_matrix))
    return fail(e, "missing/invalid actual projection target");
  float view_projection[16], screen[16];
  bk_matrix_multiply(view_projection, b->view, b->projection);
  bk_matrix_multiply(screen, view_projection, b->viewport_matrix);
  int32_t result[2];
  if (!project_integer(result, b->world[node], screen))
    return fail(e, "target projection is outside native integer range");
  point[0] = result[0];
  point[1] = result[1];
  return 1;
}
static int pick_targets(const BkEndingUiPickBindings *b,
                         const float pointer[2], float *distance,
                         int32_t *selected, int allow_absent, char e[256]) {
  static const unsigned nodes[24] = {24, 20, 25, 21, 20, 18, 21, 19,
                                     33, 35, 34, 36, 35, 31, 36, 32,
                                     31, 37, 32, 38, 37, 29, 38, 30};
  if (!b || !b->world || !b->present || !pointer || !distance || !selected ||
      !isfinite(*distance) || !isfinite(pointer[0]) || !isfinite(pointer[1]) ||
      !b->camera_position || !isfinite(b->camera_position[0]) ||
      !isfinite(b->camera_position[1]) || !isfinite(b->camera_position[2]) ||
      !isfinite(b->ring_width) || b->ring_width < 0 ||
      !finite_matrix(b->view) || !finite_matrix(b->projection) ||
      !finite_matrix(b->viewport_matrix))
    return fail(e, "invalid query input");
  for (unsigned i = 0; i < 24; i++)
    if (nodes[i] >= b->count ||
        (!b->present[nodes[i]] && !allow_absent) ||
        (b->present[nodes[i]] && !finite_matrix(b->world[nodes[i]])))
      return fail(e, "missing/invalid required cached node");
  float vp[16], screen[16], points[24][2], depths[24], radii[24];
  bk_matrix_multiply(vp, b->view, b->projection);
  bk_matrix_multiply(screen, vp, b->viewport_matrix);
  for (unsigned i = 0; i < 24; i++) {
    if (!b->present[nodes[i]])
      continue;
    const float *world = b->world[nodes[i]];
    float delta[3];
    for (unsigned j = 0; j < 3; j++)
      delta[j] = (float)((double)b->camera_position[j] - world[12 + j]);
    float square =
        (float)((double)delta[0] * delta[0] + (double)delta[1] * delta[1] +
                (double)delta[2] * delta[2]);
    depths[i] = (float)sqrt((double)square);
    if (!isfinite(depths[i]) || !project(points[i], world, screen))
      return fail(e, "invalid target distance/projection");
    radii[i] = depths[i] == 0
                   ? INFINITY
                   : (float)((i == 0 || i == 2 ? 60.0 : 40.0) / depths[i]);
  }
  float nearest = *distance;
  int32_t result = -1;
  for (unsigned i = 0; i < 12; i++) {
    unsigned a = i * 2, z = a + 1;
    if (!b->present[nodes[a]] || !b->present[nodes[z]])
      continue;
    float radius = (float)(((double)b->ring_width / 2) *
                           (((double)radii[a] + radii[z]) / 2));
    int hit;
    if (!bk_ending_ui_segment_hit(points[a], points[z], pointer, radius, &hit))
      return fail(e, "invalid segment geometry");
    double average = ((double)depths[a] + depths[z]) / 2;
    if (hit && average < nearest) {
      nearest = (float)average;
      result = (int32_t)i;
    }
  }
  *distance = nearest;
  *selected = result;
  return 1;
}
int bk_ending_ui_pick_targets(const BkEndingUiPickBindings *b,
                              const float pointer[2], float *distance,
                              int32_t *selected, char e[256]) {
  return pick_targets(b, pointer, distance, selected, 0, e);
}
int bk_ending_ui_pick_available_targets(const BkEndingUiPickBindings *b,
                                        const float pointer[2], float *distance,
                                        int32_t *selected, char e[256]) {
  return pick_targets(b, pointer, distance, selected, 1, e);
}
int bk_ending_target_eligible(uint8_t group, int32_t action_variant,
                               uint8_t scene_variant, float progress,
                               int32_t node) {
  static const int32_t tables[7][9] = {
      {1, 11, 12, 9, 10, 26, 27, 5, 6},
      {1, 11, 12, 9, 10, -1, -1, 5, 6},
      {17, 13, 5, 0, 6, -1, -1, -1, -1},
      {32, 13, 5, 0, 6, -1, -1, -1, -1},
      {17, 13, 5, 1, 6, -1, -1, -1, -1},
      {17, 7, 5, 0, 6, -1, -1, -1, -1},
      {32, 12, 5, 0, 6, -1, -1, -1, -1}};
  const int32_t zero[9] = {0};
  const int32_t *table = !action_variant ? tables[group == 1 || group == 2]
                         : group < 5 ? tables[group + 2] : zero;
  int limit = scene_variant == 0 ? 8 : scene_variant == 5 ? 4 : -1;
  for (int i = 0; i <= limit; ++i)
    if (table[i] != -1 && table[i] == node &&
        (i < limit || progress >= .39f))
      return 1;
  return 0;
}
static int target_scale(const BkEndingUiPickBindings *b, unsigned node,
                         float *scale, char e[256]) {
  static const float sizes[39] = {
      150, 50, 40, 100, 100, 100, 80, 100, 100, 50, 50, 100, 100,
      100, 40, 40, 40, 100, 40, 40, 40, 40, 40, 40, 60, 60, 100,
      100, 100, 40, 40, 40, 100, 40, 40, 40, 40, 40, 40};
  float d[3];
  for (unsigned i = 0; i < 3; ++i)
    d[i] = (float)((double)b->camera_position[i] - b->world[node][12 + i]);
  float square = (float)((double)d[0] * d[0] + (double)d[1] * d[1] +
                          (double)d[2] * d[2]);
  if (!isfinite(square))
    return fail(e, "invalid target length");
  /*40de40 performs a non-popping FST. Its sqrt stays unrounded until the
   * ratio store here, unlike the explicit depth store in4da76f. */
  *scale = square == 0 ? INFINITY : (float)(sizes[node] / sqrt((double)square));
  return 1;
}
int bk_ending_target_step(const BkEndingTargetBindings *b,
                            const float pointer[2], int *result, char e[256]) {
  if (!b || !b->frame || !b->control || !b->auxiliary || !b->active_clip ||
      !b->actions || !b->targets || !b->action_kind || !b->action_column ||
      !b->ring || !b->geometry || !pointer || !result ||
      !isfinite(pointer[0]) || !isfinite(pointer[1]) ||
      !finite_matrix(b->camera_local))
    return fail(e, "missing target controller bindings");
  const BkEndingUiPickBindings *g = b->geometry;
  if (!g->world || !g->present || g->count < 39 || !g->camera_position ||
      !isfinite(g->camera_position[0]) || !isfinite(g->camera_position[1]) ||
      !isfinite(g->camera_position[2]) || !finite_matrix(g->view) ||
      !finite_matrix(g->projection) || !finite_matrix(g->viewport_matrix) ||
      !isfinite(b->ring->rect[2]) || b->ring->rect[2] < 0)
    return fail(e, "invalid actual target geometry");
  BkEndingFrameState *f = b->frame;
  f->camera_cached = *b->action_kind = *b->action_column = -1;
  float nearest = 1000000, vp[16], screen[16];
  bk_matrix_multiply(vp, g->view, g->projection);
  bk_matrix_multiply(screen, vp, g->viewport_matrix);
  for (unsigned i = 0; i < 39; ++i) {
    if (!g->present[i])
      continue;
    if (!finite_matrix(g->world[i]) ||
        !project_integer(b->targets[i], g->world[i], screen))
      return fail(e, "invalid existing target projection");
    float scale;
    if (!target_scale(g, i, &scale, e))
      return 0;
    float radius = (float)(((double)b->ring->rect[2] / 2) * scale);
    float point[2] = {(float)b->targets[i][0], (float)b->targets[i][1]};
    float distance = 0;
    int hit;
    if (!bk_ending_ui_circle_hit(point, radius, pointer, &hit, &distance))
      return fail(e, "invalid target circle");
    if (!hit || (!bk_ending_target_eligible(f->group, b->auxiliary->variant,
                    b->control->variant, b->auxiliary->progress, (int32_t)i) &&
                 f->camera_cached != -1) || nearest < distance)
      continue;
    int low = 200, high = 320;
    if (f->group == 1 && f->phase == 3 && *b->active_clip > 3) {
      low = 359;
      high = 0;
    }
    if (f->group == 2 || f->group == 3 || f->group == 4) {
      if (f->phase == 1) {
        low = 120;
        high = 240;
      } else if (f->phase == 3) {
        low = 135;
        high = 265;
      }
    }
    int outside, category;
    if (!bk_ending_ui_camera_sector(b->camera_local, f->camera_cached,
                                      low, high, &outside, &category))
      return fail(e, "invalid target camera sector");
    int back = i == 26 || i == 27 || i == 28 || i == 8 || i == 4 || i == 3;
    if (outside != back) {
      nearest = distance;
      f->camera_cached = (int32_t)i;
    }
  }
  *result = 0;
  if (f->camera_cached == -1)
    return 1;
  for (unsigned row = 0; row < 16; ++row) {
    if (f->camera_cached != b->actions[row * 5])
      continue;
    int low, high, allowed, outside, category;
    if (b->control->variant == 0) {
      int special = f->group == 2 || f->group == 3 || f->group == 4;
      low = special ? 120 : 200;
      high = special ? 240 : 320;
      if (!bk_ending_ui_camera_sector(b->camera_local, f->camera_cached,
                                        low, high, &outside, &category))
        return fail(e, "invalid action camera sector");
      allowed = category == 2 || category == 3;
    } else if (b->control->variant == 5) {
      low = 140;
      high = 220;
      if (f->group == 1) {
        if (*b->active_clip > 3) {
          low = 359;
          high = 0;
        }
      } else if (f->group == 2 || f->group == 3 || f->group == 4) {
        low = 135;
        high = 265;
      }
      if (!bk_ending_ui_camera_sector(b->camera_local, f->camera_cached,
                                        low, high, &outside, &category))
        return fail(e, "invalid action camera sector");
      allowed = outside;
    } else
      return fail(e, "undefined native action flag for scene variant");
    if (!allowed) {
      *b->action_kind = 3;
      return 1;
    }
    unsigned target = (unsigned)f->camera_cached;
    float scale;
    if (!project_integer(b->targets[target], g->world[target], screen) ||
        !target_scale(g, target, &scale, e))
      return fail(e, "invalid action target projection/scale");
    b->ring->transform.scale[0] = b->ring->transform.scale[1] = scale;
    b->ring->transform.pivot[0] = b->ring->transform.pivot[1] = .5f;
    if (row < 5)
      *b->action_kind = 0;
    else if (row < 10)
      *b->action_kind = 1;
    else if (row < 14)
      *b->action_kind = 2;
    else if (row == 15) {
      *b->action_kind = 3;
      if (!(b->auxiliary->progress >= .39f && f->camera_cached == 6))
        return 1;
    }
    *b->action_column = (int32_t)row % 5;
    *result = 1;
    return 1;
  }
  *b->action_kind = 3;
  return 1;
}
static int secondary_scale(const BkEndingUiPickBindings *g, const float *world,
                            float size, float *scale, char e[256]) {
  float d[3];
  for (unsigned i=0;i<3;++i)
    d[i]=(float)((double)g->camera_position[i]-world[12+i]);
  float square=(float)((double)d[0]*d[0]+(double)d[1]*d[1]+(double)d[2]*d[2]);
  if (!isfinite(square)) return fail(e,"nonfinite secondary target distance");
  *scale=square==0 ? INFINITY : (float)(size/sqrt((double)square));
  return 1;
}
static int secondary_circle(const int32_t center[2], float width, float scale,
                            const float point[2], int *hit, float *distance,
                            char e[256]) {
  float xy[2]={(float)center[0],(float)center[1]};
  float radius=(float)(((double)width/2)*scale);
  return bk_ending_ui_circle_hit(xy,radius,point,hit,distance)
      ? 1 : fail(e,"invalid secondary target circle");
}
static void secondary_ring(BkEndingUiSprite *ring, const int32_t point[2],
                            float scale) {
  ring->rect[0]=(float)point[0]; ring->rect[1]=(float)point[1];
  ring->transform.scale[0]=ring->transform.scale[1]=scale;
  ring->transform.pivot[0]=ring->transform.pivot[1]=.5f;
}
int bk_ending_secondary_pick(const BkEndingSecondaryPickBindings *b,
                              const float pointer[2], int32_t preferred,
                              int32_t *result, char e[256]) {
  if (!b || !b->frame || !b->kind || !b->column || !b->targets ||
      !b->alternate || !b->ring || !b->geometry || !pointer || !result ||
      !isfinite(pointer[0]) || !isfinite(pointer[1]))
    return fail(e,"missing secondary target owners");
  const BkEndingUiPickBindings *g=b->geometry;
  if (!g->world || !g->present || g->count<39 || !g->camera_position ||
      !isfinite(g->camera_position[0]) || !isfinite(g->camera_position[1]) ||
      !isfinite(g->camera_position[2]) || !finite_matrix(g->view) ||
      !finite_matrix(g->projection) || !finite_matrix(g->viewport_matrix) ||
      !isfinite(b->ring->rect[2]) || b->ring->rect[2]<0)
    return fail(e,"invalid secondary projection geometry");
  BkEndingFrameState *f=b->frame;
  f->camera_cached=*b->kind=*b->column=-1;
  float nearest=1000000.f,vp[16],screen[16];
  bk_matrix_multiply(vp,g->view,g->projection);
  bk_matrix_multiply(screen,vp,g->viewport_matrix);
  for (unsigned i=0;i<39;++i) {
    if (!g->present[i]) continue;
    if (!finite_matrix(g->world[i]) ||
        !project_integer(b->targets[i],g->world[i],screen))
      return fail(e,"invalid secondary target projection");
    float scale,distance=0;
    int hit;
    if (!secondary_scale(g,g->world[i],i==0 ? 100 : i==4 ? 150 : 200,&scale,e) ||
        !secondary_circle(b->targets[i],b->ring->rect[2],scale,pointer,&hit,&distance,e))
      return 0;
    if (hit && ((int32_t)i==preferred || f->camera_cached==-1) &&
        (nearest>distance || (int32_t)i==preferred)) {
      nearest=distance;
      f->camera_cached=(int32_t)i;
    }
  }
  int alternate_preferred=0;
  if (f->camera_cached!=-1 && f->camera_cached==preferred && b->alternate_world) {
    float scale,distance=0;
    int hit;
    if (!finite_matrix(b->alternate_world) ||
        !project_integer(b->alternate,b->alternate_world,screen) ||
        !secondary_scale(g,b->alternate_world,150,&scale,e) ||
        !secondary_circle(b->alternate,b->ring->rect[2],scale,pointer,&hit,&distance,e))
      return fail(e,"invalid secondary alternate comparison");
    alternate_preferred=hit && nearest>distance;
  }
  if (f->camera_cached!=-1 && f->camera_cached==preferred && !alternate_preferred) {
    unsigned i=(unsigned)f->camera_cached;
    if (b->targets[i][0]>=0 && b->targets[i][1]>=0) {
      float scale;
      if (!project_integer(b->targets[i],g->world[i],screen) ||
          !secondary_scale(g,g->world[i],i==0 ? 100 : i==4 ? 150 : 200,&scale,e))
        return fail(e,"invalid selected secondary target");
      secondary_ring(b->ring,b->targets[i],scale);
      f->camera_event=*result=1;
      return 1;
    }
  } else if (b->alternate_world) {
    float scale,distance=0;
    int hit;
    if (!finite_matrix(b->alternate_world) ||
        !project_integer(b->alternate,b->alternate_world,screen) ||
        !secondary_scale(g,b->alternate_world,100,&scale,e) ||
        !secondary_circle(b->alternate,b->ring->rect[2],scale,pointer,&hit,&distance,e))
      return fail(e,"invalid secondary alternate target");
    if (hit) {
      secondary_ring(b->ring,b->alternate,scale);
      f->camera_event=*result=2;
      return 1;
    }
  }
  f->camera_event=*result=0;
  return 1;
}
