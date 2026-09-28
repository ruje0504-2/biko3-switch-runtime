#include "world/collision.h"
#include "core/matrix.h"
#include "world/ground.h"
#include "world/proximity.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkCollision {
  uint32_t count, static_count;
  BkCollisionMesh *meshes;
  int8_t *kinds;
};
int bk_collision_prop_blocked(const BkCollision *c, int32_t kind,
                              const float start[3], const float end[3],
                              int *blocked, char error[256]) {
  int hit;
  if (!c || !blocked ||
      !bk_segments_intersect_xz(&hit, start, end, start, end)) {
    snprintf(error, 256, "prop collision: invalid query");
    return 0;
  }
  const float limit = (float)cos(1.047);
  for (uint32_t i = 0; i < c->count; i++) {
    if (c->kinds[i] == -1 || c->kinds[i] == kind || c->kinds[i] == 10)
      continue;
    const BkCollisionMesh *m = &c->meshes[i];
    for (uint32_t t = 0; t < m->index_count; t += 3) {
      if (m->normals[t / 3][1] >= limit)
        continue;
      for (unsigned e = 0; e < 3; e++) {
        const float *a = m->vertices[m->indices[t + e]];
        const float *b = m->vertices[m->indices[t + (e + 1) % 3]];
        if (!bk_segments_intersect_xz(&hit, a, b, start, end)) {
          snprintf(error, 256, "prop collision: nonfinite intersection");
          return 0;
        }
        if (hit) {
          *blocked = 1;
          return 1;
        }
      }
    }
  }
  *blocked = 0;
  return 1;
}
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
int bk_collision_normal(float out[3], const float t[3][3]) {
  if (!out || !t)
    return 0;
  for (unsigned i = 0; i < 3; i++)
    for (unsigned j = 0; j < 3; j++)
      if (!isfinite(t[i][j]))
        return 0;
  float n[3];
  for (unsigned i = 0; i < 3; i++) {
    unsigned j = (i + 1) % 3, k = (i + 2) % 3;
    n[i] = (float)(((double)t[1][k] - t[2][k]) * t[0][j] +
                   ((double)t[2][k] - t[0][k]) * t[1][j] +
                   ((double)t[0][k] - t[1][k]) * t[2][j]);
    if (!isfinite(n[i]))
      return 0;
  }
  double squared =
      (double)n[1] * n[1] + (double)n[2] * n[2] + (double)n[0] * n[0];
  if (squared - 1 < -9.999999747378752e-6f ||
      squared - 1 > 9.999999747378752e-6f) {
    if (squared <= 9.999999439624929e-11f)
      memset(n, 0, sizeof(n));
    else {
      double inverse = 1 / sqrt(squared);
      for (unsigned i = 0; i < 3; i++)
        n[i] = (float)(n[i] * inverse);
    }
  }
  memcpy(out, n, sizeof(n));
  return 1;
}
void bk_collision_destroy(BkCollision *c) {
  if (!c)
    return;
  for (uint32_t i = 0; i < c->count; i++) {
    free(c->meshes[i].vertices);
    free(c->meshes[i].indices);
    free(c->meshes[i].normals);
  }
  free(c->meshes);
  free(c->kinds);
  free(c);
}
static int append(BkCollision *c, const BkModelSubmesh *s,
                  const float position[3], const float *world, const char *name,
                  char error[256]) {
  if (!s->vertices || !s->indices || !s->vertex_count || !s->index_count ||
      s->index_count % 3 || c->count >= 16384)
    goto invalid;
  BkCollisionMesh *grown = realloc(c->meshes, (c->count + 1) * sizeof(*grown));
  if (!grown)
    goto allocation;
  c->meshes = grown;
  BkCollisionMesh *m = &grown[c->count++];
  memset(m, 0, sizeof(*m));
  snprintf(m->name, sizeof(m->name), "%s", name);
  m->vertex_count = s->vertex_count;
  m->index_count = s->index_count;
  m->vertices = calloc(m->vertex_count, sizeof(*m->vertices));
  m->indices = calloc(m->index_count, sizeof(*m->indices));
  m->normals = calloc(m->index_count / 3, sizeof(*m->normals));
  if (!m->vertices || !m->indices || !m->normals)
    goto allocation;
  for (uint32_t i = 0; i < m->vertex_count; i++) {
    if (world) {
      if (!bk_collision_prop_point(m->vertices[i], s->vertices[i].position,
                                   world, position))
        goto invalid;
    } else
      for (unsigned j = 0; j < 3; j++) {
        m->vertices[i][j] =
            (float)((double)s->vertices[i].position[j] + position[j]);
        if (!isfinite(m->vertices[i][j]))
          goto invalid;
      }
  }
  for (uint32_t i = 0; i < m->index_count; i++) {
    if (s->indices[i] >= m->vertex_count)
      goto invalid;
    m->indices[i] = s->indices[i];
  }
  for (uint32_t i = 0; i < m->index_count; i += 3) {
    float t[3][3];
    for (unsigned j = 0; j < 3; j++)
      memcpy(t[j], m->vertices[m->indices[i + j]], sizeof(t[j]));
    if (!bk_collision_normal(m->normals[i / 3], t))
      goto invalid;
  }
  return 1;
invalid:
  snprintf(error, 256, "Collision: invalid/nonfinite mesh");
  return 0;
allocation:
  snprintf(error, 256, "Collision: allocation failed");
  return 0;
}
BkCollision *bk_collision_create(const BkModel *model, const float *world,
                                 size_t world_floats, const char *model_name,
                                 const void *atr, size_t atr_size,
                                 char error[256]) {
  BkCollision *c = NULL;
  if (!model || !world || world_floats != (size_t)model->frame_count * 16 ||
      !model_name || !*model_name || strlen(model_name) > 191 || !atr ||
      atr_size != BK_COLLISION_ATR_SIZE) {
    snprintf(error, 256, "Collision: invalid model/ATR input");
    return NULL;
  }
  const uint8_t *data = atr;
  uint32_t count = u32(data + 0x418200);
  if (count > 128)
    goto invalid;
  c = calloc(1, sizeof(*c));
  if (!c) {
    snprintf(error, 256, "Collision: allocation failed");
    return NULL;
  }
  for (uint32_t i = 0; i < count; i++) {
    const uint8_t *record = data + i * 0x8304;
    uint32_t names = u32(record + 0x8300), frame_index;
    if (!record[0] || !memchr(record, 0, 256) || names > 128)
      goto invalid;
    for (uint32_t j = 0; j < names; j++)
      if (!memchr(record + 256 + j * 260, 0, 256))
        goto invalid;
    if (!bk_model_find_frame(model, (const char *)record, &frame_index, error))
      goto fail;
    const BkModelFrame *frame = &model->frames[frame_index];
    if (frame->mesh_index == BK_MODEL_NONE)
      continue;
    if (frame->mesh_index >= model->mesh_count)
      goto invalid;
    const BkModelMesh *mesh = &model->meshes[frame->mesh_index];
    if (mesh->first_submesh > model->submesh_count ||
        mesh->submesh_count > model->submesh_count - mesh->first_submesh)
      goto invalid;
    const float *position = world + frame_index * 16 + 12;
    /* Native singleton object type3ea is copied regardless of ATR names. */
    uint32_t requests = mesh->submesh_count == 1 ? 1 : names;
    for (uint32_t j = 0; j < requests; j++) {
      int found = 0;
      for (uint32_t k = 0; k < mesh->submesh_count; k++) {
        const BkModelSubmesh *s = &model->submeshes[mesh->first_submesh + k];
        char name[260];
        /* 0x418b90: grouped runtime names use parent name and ordinal,
         * ignoring the child header's padded name. */
        int length =
            mesh->submesh_count == 1
                ? snprintf(name, sizeof(name), "%s@%s", mesh->name, model_name)
                : snprintf(name, sizeof(name), "%s_%u@%s", mesh->name, k,
                           model_name);
        if (length < 0 || (size_t)length >= sizeof(name))
          goto invalid;
        if (mesh->submesh_count != 1 &&
            strcmp(name, (const char *)record + 256 + j * 260))
          continue;
        if (!append(c, s, position, NULL, name, error))
          goto fail;
        found = 1;
        break;
      }
      /* Missing references must be visible, unlike original silent skips. */
      if (!found) {
        snprintf(error, 256, "Collision: ATR submesh missing: %.120s",
                 (const char *)record + 256 + j * 260);
        goto fail;
      }
    }
  }
  c->static_count = c->count;
  return c;
invalid:
  snprintf(error, 256, "Collision: invalid ATR selection/model mapping");
fail:
  bk_collision_destroy(c);
  return NULL;
}
uint32_t bk_collision_count(const BkCollision *c) { return c ? c->count : 0; }
const BkCollisionMesh *bk_collision_mesh(const BkCollision *c, uint32_t index) {
  return c && index < c->count ? &c->meshes[index] : NULL;
}
int bk_collision_ground(const BkCollisionMesh *mesh, const float point[3],
                        int *hit, float *height, char error[256]) {
  if (!mesh || !point || !hit || !height || !isfinite(*height) ||
      !mesh->vertices || !mesh->indices || !mesh->normals ||
      mesh->index_count % 3)
    goto invalid;
  for (unsigned i = 0; i < 3; i++)
    if (!isfinite(point[i]))
      goto invalid;
  int any = 0;
  float best = -99999, candidate = point[1];
  const float threshold = (float)cos(1.047);
  for (uint32_t i = 0; i < mesh->index_count; i += 3) {
    if (!isfinite(mesh->normals[i / 3][1]))
      goto invalid;
    if (mesh->normals[i / 3][1] < threshold)
      continue;
    float t[3][3];
    for (unsigned j = 0; j < 3; j++) {
      if (mesh->indices[i + j] >= mesh->vertex_count)
        goto invalid;
      memcpy(t[j], mesh->vertices[mesh->indices[i + j]], sizeof(t[j]));
    }
    int found;
    if (!bk_ground_triangle(&found, &candidate, point, t))
      goto invalid;
    if (found) {
      any = 1;
      if ((double)point[1] + 15 >= candidate &&
          (double)point[1] - 15 <= candidate && best <= candidate)
        best = candidate;
    }
  }
  if (any)
    *height = best;
  *hit = any;
  return 1;
invalid:
  snprintf(error, 256, "Collision: invalid ground query/geometry");
  return 0;
}

int bk_collision_prop_point(float out[3], const float point[3],
                            const float world[16], const float position[3]) {
  if (!out || !point || !world || !position)
    return 0;
  for (unsigned i = 0; i < 16; ++i)
    if (!isfinite(world[i]))
      return 0;
  for (unsigned i = 0; i < 3; ++i)
    if (!isfinite(point[i]))
      return 0;
  if (!isfinite(position[0]) || !isfinite(position[2]))
    return 0;
  float degrees =
      (float)(atan2((double)world[8], world[10]) * 180 / 3.141592025756836);
  if (degrees < -180)
    degrees = (float)((double)degrees + 360);
  else if (degrees > 180)
    degrees = (float)((double)degrees - 360);
  float angle = (float)((double)degrees * .01745329238474369f);
  float sn = (float)sin((double)angle), cs = (float)cos((double)angle);
  const float rotate[16] = {cs, 0, -sn, 0, 0, 1, 0, 0,
                            sn, 0, cs,  0, 0, 0, 0, 1};
  float translate[16] = {1, 0, 0, 0, 0,        1,        0,        0,
                         0, 0, 1, 0, point[0], point[1], point[2], 1};
  float frame[16], temp[16], matrix[16];
  memcpy(frame, world, sizeof(frame));
  frame[12] = frame[13] = frame[14] = 0;
  bk_matrix_multiply(temp, translate, rotate);
  bk_matrix_multiply(matrix, frame, temp);
  float result[3] = {(float)((double)matrix[12] + position[0]), matrix[13],
                     (float)((double)matrix[14] + position[2])};
  for (unsigned i = 0; i < 3; ++i)
    if (!isfinite(result[i]))
      return 0;
  memcpy(out, result, sizeof(result));
  return 1;
}
static const char *prop_frame(int32_t kind) {
  switch (kind) {
  case 0:
  case 13:
  case 14:
  case 15:
  case 16:
  case 17:
    return "Kuruma_Atari_Layer1";
  case 1:
    return "qqq1_train0_Layer1__1_";
  case 10:
    return "qqq1_poliLOW_Layer1";
  case 18:
  case 19:
    return "qqq2_Atari_Hantei_Layer1";
  default:
    return NULL;
  }
}
void bk_collision_end_props(BkCollision *c) {
  if (!c)
    return;
  for (uint32_t i = c->static_count; i < c->count; ++i) {
    free(c->meshes[i].vertices);
    free(c->meshes[i].indices);
    free(c->meshes[i].normals);
    memset(&c->meshes[i], 0, sizeof(c->meshes[i]));
  }
  c->count = c->static_count;
}
int bk_collision_kind(const BkCollision *c, uint32_t index, int8_t *kind) {
  if (!c || !kind || index >= c->count)
    return 0;
  *kind = index < c->static_count ? -1 : c->kinds[index];
  return 1;
}
int bk_collision_begin_props(BkCollision *c, const BkCollisionProp *props,
                             size_t count, char error[256]) {
  if (!c || count > 16 || (count && !props)) {
    snprintf(error, 256, "Collision props: invalid input");
    return 0;
  }
  int candidates = 0;
  for (size_t i = 0; i < count; i++)
    candidates |= props[i].active && prop_frame(props[i].kind) != NULL;
  if (!candidates) {
    bk_collision_end_props(c);
    return 1;
  }
  BkCollision *next = calloc(1, sizeof(*next));
  if (!next) {
    snprintf(error, 256, "Collision props: allocation failed");
    return 0;
  }
  for (size_t i = 0; i < count; ++i) {
    const BkCollisionProp *p = &props[i];
    const char *name = prop_frame(p->kind);
    if (!p->active || !name)
      continue;
    const BkModel *model = p->model;
    if (!model || !p->world ||
        p->world_floats != (size_t)model->frame_count * 16 || !p->model_name ||
        !*p->model_name || strlen(p->model_name) > 191)
      goto invalid;
    uint32_t frame;
    if (!bk_model_find_frame(model, name, &frame, error))
      goto fail;
    uint32_t mi = model->frames[frame].mesh_index;
    if (mi == BK_MODEL_NONE)
      continue;
    if (mi >= model->mesh_count)
      goto invalid;
    const BkModelMesh *mesh = &model->meshes[mi];
    if (mesh->first_submesh > model->submesh_count ||
        mesh->submesh_count > model->submesh_count - mesh->first_submesh)
      goto invalid;
    for (uint32_t j = 0; j < mesh->submesh_count; ++j) {
      uint32_t si = mesh->first_submesh + j;
      char runtime_name[256];
      if (!bk_model_submesh_name(model, si, p->model_name, runtime_name,
                                 error) ||
          !append(next, &model->submeshes[si], p->position,
                  p->world + frame * 16, runtime_name, error))
        goto fail;
      int8_t *grown = realloc(next->kinds, next->count);
      if (!grown)
        goto allocation;
      next->kinds = grown;
      next->kinds[next->count - 1] = (int8_t)p->kind;
    }
  }
  if (next->count > 16384 - c->static_count)
    goto invalid;
  uint32_t total = c->static_count + next->count;
  if (!next->count) {
    bk_collision_end_props(c);
    bk_collision_destroy(next);
    return 1;
  }
  BkCollisionMesh *meshes = malloc(total * sizeof(*meshes));
  int8_t *kinds = malloc(total);
  if (!meshes || !kinds) {
    free(meshes);
    free(kinds);
    goto allocation;
  }
  if (c->static_count)
    memcpy(meshes, c->meshes, c->static_count * sizeof(*meshes));
  if (next->count) {
    memcpy(meshes + c->static_count, next->meshes,
           next->count * sizeof(*meshes));
    memcpy(kinds + c->static_count, next->kinds, next->count);
  }
  memset(kinds, -1, c->static_count);
  bk_collision_end_props(c);
  free(c->meshes);
  free(c->kinds);
  c->meshes = meshes;
  c->kinds = kinds;
  c->count = total;
  next->count = 0;
  bk_collision_destroy(next);
  return 1;
invalid:
  snprintf(error, 256, "Collision props: invalid model/geometry");
  goto fail;
allocation:
  snprintf(error, 256, "Collision props: allocation failed");
fail:
  bk_collision_destroy(next);
  return 0;
}
