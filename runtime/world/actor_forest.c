#include "world/actor_forest.h"
#include "core/matrix.h"
#include "world/actor_pose_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  uint32_t actor, frame;
} Binding;
typedef struct {
  float local[16], world[16], parent[16];
  uint32_t hidden;
} Anchor;
struct BkActorForest {
  BkActorPose **actors;
  uint32_t actors_count, count, *starts, *hidden, *path;
  Binding *bindings;
  BkFrameTree *tree, *pending;
  BkFrameVisit *visits, *scratch_visits;
  uint32_t visits_count, scratch_count;
  float *world, view[16];
  Anchor anchors[2];
};
static const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                   0, 0, 1, 0, 0, 0, 0, 1};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "actor forest: %s", message);
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
void bk_actor_forest_destroy(BkActorForest *f) {
  if (!f)
    return;
  free(f->actors);
  free(f->starts);
  free(f->hidden);
  free(f->path);
  free(f->bindings);
  free(f->visits);
  free(f->scratch_visits);
  free(f->world);
  bk_frame_tree_destroy(f->tree);
  bk_frame_tree_destroy(f->pending);
  free(f);
}
BkActorForest *bk_actor_forest_create(BkActorPose *const *actors,
                                      uint32_t count, char error[256]) {
  if ((count && !actors) || count > BK_FRAME_TREE_LIMIT - 2) {
    fail(error, "invalid registry");
    return NULL;
  }
  uint64_t total = 2;
  for (uint32_t i = 0; i < count; ++i) {
    const BkModel *m = bk_actor_pose_model(actors[i]);
    if (!m) {
      fail(error, "missing actor");
      return NULL;
    }
    for (uint32_t j = 0; j < i; ++j)
      if (actors[i] == actors[j]) {
        fail(error, "duplicate actor alias");
        return NULL;
      }
    total += m->frame_count;
  }
  if (total > BK_FRAME_TREE_LIMIT) {
    fail(error, "registry capacity exceeded");
    return NULL;
  }
  BkActorForest *f = calloc(1, sizeof(*f));
  if (!f)
    goto oom;
  f->actors_count = count;
  f->count = (uint32_t)total;
  f->actors = calloc(count ? count : 1, sizeof(*f->actors));
  f->starts = calloc(count ? count : 1, sizeof(*f->starts));
  f->bindings = malloc(total * sizeof(*f->bindings));
  f->hidden = malloc(total * sizeof(*f->hidden));
  f->path = malloc(total * sizeof(*f->path));
  f->visits = malloc(total * sizeof(*f->visits));
  f->scratch_visits = malloc(total * sizeof(*f->scratch_visits));
  f->world = malloc(total * 16 * sizeof(*f->world));
  f->tree = bk_frame_tree_create(f->count);
  f->pending = bk_frame_tree_create(f->count);
  if (!f->actors || !f->starts || !f->bindings || !f->hidden || !f->path ||
      !f->visits || !f->scratch_visits || !f->world || !f->tree || !f->pending)
    goto oom;
  for (unsigned a = 0; a < 2; ++a) {
    memcpy(f->anchors[a].local, identity, 64);
    memcpy(f->anchors[a].world, identity, 64);
    memcpy(f->anchors[a].parent, identity, 64);
    f->bindings[a] = (Binding){BK_FRAME_NONE, BK_FRAME_NONE};
  }
  memcpy(f->view, identity, 64);
  uint32_t start = 2;
  int refresh;
  if (!bk_frame_tree_attach(f->tree, 0, 1, &refresh))
    goto invalid;
  for (uint32_t a = 0; a < count; ++a) {
    f->actors[a] = actors[a];
    f->starts[a] = start;
    const BkModel *m = bk_actor_pose_model(actors[a]);
    for (uint32_t n = 0; n < m->frame_count; ++n) {
      f->bindings[start + n] = (Binding){a, n};
      uint32_t p = m->frames[n].parent_index;
      if (p != BK_MODEL_NONE &&
          (p >= m->frame_count ||
           !bk_frame_tree_attach(f->tree, start + p, start + n, &refresh)))
        goto invalid;
    }
    start += m->frame_count;
  }
  return f;
oom:
  fail(error, "allocation failed");
  bk_actor_forest_destroy(f);
  return NULL;
invalid:
  fail(error, "invalid model hierarchy");
  bk_actor_forest_destroy(f);
  return NULL;
}
int bk_actor_forest_append(BkActorForest *f, BkActorPose *pose, uint32_t *actor,
                           char error[256]) {
  if (!f || !pose || !actor || f->actors_count >= BK_FRAME_TREE_LIMIT - 2)
    return fail(error, "invalid appended actor");
  uint32_t index = f->actors_count;
  BkActorPose **poses = malloc((index + 1) * sizeof(*poses));
  if (!poses)
    return fail(error, "allocation failed");
  memcpy(poses, f->actors, index * sizeof(*poses));
  poses[index] = pose;
  BkActorForest *next = bk_actor_forest_create(poses, index + 1, error);
  free(poses);
  if (!next)
    return 0;
  if (!bk_frame_tree_copy_prefix(next->tree, f->tree)) {
    bk_actor_forest_destroy(next);
    return fail(error, "invalid appended topology");
  }
  memcpy(next->anchors, f->anchors, sizeof(f->anchors));
  memcpy(next->view, f->view, sizeof(f->view));
  BkActorForest old = *f;
  *f = *next;
  *next = old;
  bk_actor_forest_destroy(next);
  *actor = index;
  return 1;
}
uint32_t bk_actor_forest_node(const BkActorForest *f, uint32_t a,
                              uint32_t frame) {
  return f && a < f->actors_count &&
                 frame < bk_actor_pose_model(f->actors[a])->frame_count
             ? f->starts[a] + frame
             : BK_FRAME_NONE;
}
int bk_actor_forest_restore_global(BkActorForest *f, uint32_t actor,
                                    uint32_t frame, char error[256]) {
  uint32_t node = bk_actor_forest_node(f, actor, frame);
  if (node == BK_FRAME_NONE ||
      bk_actor_pose_model(f->actors[actor])->frames[frame].parent_index != BK_MODEL_NONE ||
      bk_frame_tree_parent(f->tree, node) != BK_FRAME_NONE)
    return fail(error, "invalid retained global root");
  int refresh;
  if (!bk_frame_tree_copy(f->pending, f->tree) ||
      !bk_frame_tree_attach(f->pending, 0, node, &refresh) || !refresh)
    return fail(error, "cannot restore retained global topology");
  BkFrameTree *old = f->tree;
  f->tree = f->pending;
  f->pending = old;
  f->visits_count = 0;
  return 1;
}
int bk_actor_forest_binding(const BkActorForest *f, uint32_t node,
                            uint32_t *actor, uint32_t *frame) {
  if (!f || node < 2 || node >= f->count || !actor || !frame)
    return 0;
  *actor = f->bindings[node].actor;
  *frame = f->bindings[node].frame;
  return 1;
}
const BkFrameTree *bk_actor_forest_tree(const BkActorForest *f) {
  return f ? f->tree : NULL;
}
const float *bk_actor_forest_world(const BkActorForest *f, uint32_t node) {
  if (!f || node >= f->count)
    return NULL;
  if (node < 2)
    return f->anchors[node].world;
  Binding b = f->bindings[node];
  return bk_actor_pose_frame(f->actors[b.actor], b.frame);
}
static const float *local(const BkActorForest *f, uint32_t node) {
  if (node < 2)
    return f->anchors[node].local;
  Binding b = f->bindings[node];
  return bk_actor_pose_local(f->actors[b.actor], b.frame);
}
static uint32_t subtree_next(const BkFrameTree *tree, uint32_t root,
                             uint32_t node) {
  uint32_t next = bk_frame_tree_first(tree, node);
  if (next != BK_FRAME_NONE)
    return next;
  while (node != root) {
    next = bk_frame_tree_next(tree, node);
    if (next != BK_FRAME_NONE)
      return next;
    node = bk_frame_tree_parent(tree, node);
  }
  return BK_FRAME_NONE;
}
int bk_actor_forest_find(const BkActorForest *f, uint32_t root,
                         const char *name, uint32_t *out, char error[256]) {
  if (!f || root >= f->count || !name || !out)
    return fail(error, "invalid subtree name lookup");
  for (uint32_t n = root; n != BK_FRAME_NONE;
       n = subtree_next(f->tree, root, n)) {
    if (n < 2)
      return fail(error, "anchor has no asset name");
    Binding b = f->bindings[n];
    const char *candidate =
        bk_actor_pose_model(f->actors[b.actor])->frames[b.frame].name;
    const char *space = strchr(candidate, ' ');
    if (!strcmp(name, space ? space + 1 : candidate)) {
      *out = n;
      return 1;
    }
  }
  *out = BK_FRAME_NONE;
  return 1;
}
int bk_actor_forest_visibility(BkActorForest *f, uint32_t root, uint32_t hidden,
                               char error[256]) {
  if (!f || root >= f->count)
    return fail(error, "invalid visibility root");
  for (uint32_t n = root; n != BK_FRAME_NONE;
       n = subtree_next(f->tree, root, n)) {
    if (n < 2)
      f->anchors[n].hidden = hidden;
    else {
      Binding b = f->bindings[n];
      bk_actor_pose_commit_hidden(f->actors[b.actor], b.frame, hidden);
    }
  }
  f->visits_count = 0;
  return 1;
}
int bk_actor_forest_anchor_reference(const BkActorForest *f, uint32_t node,
                                     BkNodeReference *out, char error[256]) {
  if (!f || node > 1 || !out)
    return fail(error, "invalid anchor reference");
  memcpy(out->local, f->anchors[node].local, 64);
  memcpy(out->world, f->anchors[node].world, 64);
  memcpy(out->parent_world, f->anchors[node].parent, 64);
  return 1;
}
int bk_actor_forest_commit_anchor_reference(BkActorForest *f, uint32_t node,
                                            const BkNodeReference *value,
                                            char error[256]) {
  if (!f || node > 1 || !value || !finite_matrix(value->local) ||
      !finite_matrix(value->world) ||
      memcmp(value->parent_world, f->anchors[node].parent, 64))
    return fail(error, "invalid/stale anchor reference");
  memcpy(f->anchors[node].local, value->local, 64);
  memcpy(f->anchors[node].world, value->world, 64);
  f->visits_count = 0;
  return 1;
}
int bk_actor_forest_anchor(BkActorForest *f, uint32_t node,
                           const float matrix[16], uint32_t hidden,
                           char error[256]) {
  if (!f || node > 1 || !finite_matrix(matrix))
    return fail(error, "invalid anchor");
  float world[16], copy[16];
  memcpy(copy, matrix, 64);
  bk_matrix_multiply(world, copy, f->anchors[node].parent);
  if (!finite_matrix(world))
    return fail(error, "anchor overflow");
  memcpy(f->anchors[node].local, copy, 64);
  memcpy(f->anchors[node].world, world, 64);
  f->anchors[node].hidden = hidden;
  f->visits_count = 0;
  return 1;
}
static int compose(BkActorForest *f, const BkFrameTree *tree, const float *base,
                   char error[256]) {
  memcpy(f->world, base, 64);
  for (uint32_t i = 0; i < f->scratch_count; ++i) {
    uint32_t n = f->scratch_visits[i].node, p = bk_frame_tree_parent(tree, n);
    const float *l = local(f, n);
    if (p >= f->count || !finite_matrix(l))
      return fail(error, "invalid node local/parent");
    bk_matrix_multiply(f->world + n * 16, l, f->world + p * 16);
    if (!finite_matrix(f->world + n * 16))
      return fail(error, "world composition overflow");
  }
  return 1;
}
/* All locals and results were validated before this commit. No external
 * callbacks or allocation can change them between preflight and publication. */
static int commit(BkActorForest *f, const BkFrameTree *tree) {
  for (uint32_t i = 0; i < f->scratch_count; ++i) {
    uint32_t n = f->scratch_visits[i].node, p = bk_frame_tree_parent(tree, n);
    if (n == 1) {
      memcpy(f->anchors[1].world, f->world + 16, 64);
      memcpy(f->anchors[1].parent, f->world + p * 16, 64);
    } else {
      Binding b = f->bindings[n];
      bk_actor_pose_commit_composed(f->actors[b.actor], b.frame,
                                    f->world + n * 16, f->world + p * 16);
    }
  }
  return 1;
}
int bk_actor_forest_refresh(BkActorForest *f, char error[256]) {
  if (!f)
    return fail(error, "missing forest");
  if (!bk_frame_tree_refresh_walk(f->tree, f->scratch_visits, f->count,
                                  &f->scratch_count) ||
      !compose(f, f->tree, f->anchors[0].world, error) || !commit(f, f->tree))
    return 0;
  f->visits_count = 0;
  return 1;
}
int bk_actor_forest_attach(BkActorForest *f, uint32_t parent, uint32_t child,
                           char error[256]) {
  int refresh;
  if (!f || !bk_frame_tree_copy(f->pending, f->tree) ||
      !bk_frame_tree_attach(f->pending, parent, child, &refresh))
    return fail(error, "invalid reparent");
  if (!refresh)
    return 1;
  if (!bk_frame_tree_refresh_walk(f->pending, f->scratch_visits, f->count,
                                  &f->scratch_count) ||
      !compose(f, f->pending, f->anchors[0].world, error) ||
      !commit(f, f->pending))
    return 0;
  BkFrameTree *old = f->tree;
  f->tree = f->pending;
  f->pending = old;
  f->visits_count = 0;
  return 1;
}
int bk_actor_forest_detach(BkActorForest *f, uint32_t parent, uint32_t child,
                           char error[256]) {
  if (!f || !bk_frame_tree_detach(f->tree, parent, child))
    return fail(error, "invalid detach");
  f->visits_count = 0;
  return 1;
}
static int camera_view(BkActorForest *f, float view[16], char error[256]) {
  uint32_t path_count;
  float camera[16];
  memcpy(view, f->view, 64);
  memcpy(camera, f->anchors[0].local, 64);
  if (!bk_frame_tree_camera_path(f->tree, 1, f->path, f->count, &path_count))
    return fail(error, "invalid camera path");
  for (uint32_t i = 0; i < path_count; ++i)
    bk_matrix_multiply(camera, local(f, f->path[i]), camera);
  if (path_count && !bk_matrix_inverse(view, camera))
    return fail(error, "singular camera path");
  return 1;
}
int bk_actor_forest_camera_publish(BkActorForest *f, char error[256]) {
  float view[16];
  if (!f)
    return fail(error, "missing camera forest");
  if (!camera_view(f, view, error))
    return 0;
  memcpy(f->view, view, 64);
  return 1;
}
int bk_actor_forest_draw(BkActorForest *f, uint32_t target,
                         const BkFrameVisit **visits, uint32_t *count,
                         char error[256]) {
  if (!f || !visits || !count)
    return fail(error, "invalid draw outputs");
  float view[16];
  if (!camera_view(f, view, error))
    return 0;
  for (uint32_t n = 0; n < f->count; ++n) {
    if (n < 2)
      f->hidden[n] = f->anchors[n].hidden;
    else {
      Binding b = f->bindings[n];
      if (!bk_actor_pose_hidden(f->actors[b.actor], b.frame, &f->hidden[n]))
        return fail(error, "invalid actor visibility");
    }
  }
  if (!bk_frame_tree_draw_walk(f->tree, target, f->hidden, f->scratch_visits,
                               f->count, &f->scratch_count) ||
      !compose(f, f->tree, f->anchors[0].local, error) || !commit(f, f->tree))
    return 0;
  BkFrameVisit *old = f->visits;
  f->visits = f->scratch_visits;
  f->scratch_visits = old;
  f->visits_count = f->scratch_count;
  memcpy(f->view, view, 64);
  *visits = f->visits;
  *count = f->visits_count;
  return 1;
}
const float *bk_actor_forest_view(const BkActorForest *f) {
  return f ? f->view : NULL;
}
