#include "scene/entry_forest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define OBJECTS 40
struct BkEntryForest {
  BkEntryAssets *entry;
  BkActorForest *frames;
  BkEntryTreeObject objects[OBJECTS];
  uint32_t count;
};
static int fail(char *error, const char *message) {
  snprintf(error, 256, "entry forest: %s", message);
  return 0;
}
void bk_entry_forest_destroy(BkEntryForest *f) {
  if (f) {
    bk_actor_forest_destroy(f->frames);
    free(f);
  }
}
static int add(BkEntryForest *f, BkEntryTreeKind kind, uint32_t index,
               BkActorPose *pose, char *error) {
  if (!pose)
    return 1;
  if (f->count == OBJECTS)
    return fail(error, "object capacity exceeded");
  const BkModel *m = bk_actor_pose_model(pose);
  uint32_t root = BK_MODEL_NONE;
  for (uint32_t i = 0; i < m->frame_count; ++i)
    if (m->frames[i].parent_index == BK_MODEL_NONE) {
      if (root != BK_MODEL_NONE)
        return fail(error, "multiple asset roots unsupported");
      root = i;
    }
  if (root == BK_MODEL_NONE)
    return fail(error, "missing asset root");
  f->objects[f->count++] = (BkEntryTreeObject){kind, index, root, pose};
  return 1;
}
static BkEntryForest *create(BkEntryAssets *entry,
                             BkBackgroundAssets *background,
                             BkPropAssets *props, BkItemAssets *items,
                             int player_last, char error[256]) {
  if (!entry || !background || !props || !items) {
    fail(error, "missing scene owner");
    return NULL;
  }
  BkEntryForest *f = calloc(1, sizeof(*f));
  if (!f) {
    fail(error, "allocation failed");
    return NULL;
  }
  f->entry = entry;
  if (!add(f, BK_ENTRY_TREE_PLAYER, 0, bk_entry_assets_player(entry), error) ||
      !add(f, BK_ENTRY_TREE_PLAYER_SHADOW, 0,
           bk_npc_shadow_bind_pose(bk_entry_assets_player_shadow(entry)),
           error) ||
      !add(f, BK_ENTRY_TREE_NPC, 0, bk_entry_assets_actor(entry), error) ||
      !add(f, BK_ENTRY_TREE_NPC_SHADOW, 0,
           bk_npc_shadow_bind_pose(bk_entry_assets_shadow(entry)), error) ||
      !add(f, BK_ENTRY_TREE_TRACK, 0,
           bk_follow_camera_bind_track(bk_entry_assets_camera(entry)), error) ||
      !add(f, BK_ENTRY_TREE_BACKGROUND, 0,
           bk_background_assets_bind_pose(background, 0), error) ||
      !add(f, BK_ENTRY_TREE_DOOR, 0,
           bk_background_assets_bind_pose(background, 1), error) ||
      !add(f, BK_ENTRY_TREE_SNOW, 0,
           bk_background_assets_bind_pose(background, 2), error))
    goto bad;
  for (uint32_t i = 0; i < bk_prop_assets_count(props); ++i)
    if (!add(f, BK_ENTRY_TREE_PROP, i, bk_prop_assets_bind_pose(props, i),
             error))
      goto bad;
  for (uint32_t i = 0; i < bk_item_assets_count(items); ++i)
    if (!add(f, BK_ENTRY_TREE_ITEM, i, bk_item_assets_bind_pose(items, i),
             error))
      goto bad;
  if (player_last) {
    BkEntryTreeObject held[2];
    uint32_t n = 0;
    while (n < f->count &&
           (f->objects[n].kind == BK_ENTRY_TREE_PLAYER ||
            f->objects[n].kind == BK_ENTRY_TREE_PLAYER_SHADOW)) {
      held[n] = f->objects[n];
      ++n;
    }
    memmove(f->objects, f->objects + n, (f->count - n) * sizeof(*f->objects));
    memcpy(f->objects + f->count - n, held, n * sizeof(*held));
  }
  BkActorPose *poses[OBJECTS];
  for (uint32_t i = 0; i < f->count; ++i)
    poses[i] = f->objects[i].pose;
  f->frames = bk_actor_forest_create(poses, f->count, error);
  if (!f->frames)
    goto bad;
  const BkCameraFollowPose *camera =
      bk_follow_camera_pose(bk_entry_assets_camera(entry));
  if (!bk_actor_forest_anchor(f->frames, 1, camera->world, 0, error))
    goto bad;
  for (uint32_t i = 0; i < f->count; ++i) {
    BkEntryTreeObject *o = &f->objects[i];
    o->root = bk_actor_forest_node(f->frames, i, o->root);
    if (!bk_actor_forest_attach(f->frames, 0, o->root, error))
      goto bad;
    if (o->kind == BK_ENTRY_TREE_SNOW &&
        !bk_actor_forest_attach(f->frames, 1, o->root, error))
      goto bad;
  }
  return f;
bad:
  bk_entry_forest_destroy(f);
  return NULL;
}
uint32_t bk_entry_forest_count(const BkEntryForest *f) {
  return f ? f->count : 0;
}
const BkEntryTreeObject *bk_entry_forest_object(const BkEntryForest *f,
                                                uint32_t i) {
  return f && i < f->count ? &f->objects[i] : NULL;
}
uint32_t bk_entry_forest_root(const BkEntryForest *f, BkEntryTreeKind kind,
                              uint32_t index) {
  if (f)
    for (uint32_t i = 0; i < f->count; ++i)
      if (f->objects[i].kind == kind && f->objects[i].index == index)
        return f->objects[i].root;
  return BK_FRAME_NONE;
}
BkActorForest *bk_entry_forest_frames(BkEntryForest *f) {
  return f ? f->frames : NULL;
}
int bk_entry_forest_draw(BkEntryForest *f, uint32_t target,
                         const BkFrameVisit **visits, uint32_t *count,
                         char error[256]) {
  if (!f || !visits || !count ||
      target >= bk_frame_tree_count(bk_actor_forest_tree(f->frames)))
    return fail(error, "invalid target/output");
  const BkCameraFollowPose *camera =
      bk_follow_camera_pose(bk_entry_assets_camera(f->entry));
  return bk_actor_forest_anchor(f->frames, 1, camera->world, 0, error) &&
         bk_actor_forest_draw(f->frames, target, visits, count, error);
}

BkEntryForest *bk_entry_forest_create(BkEntryAssets *e, BkBackgroundAssets *b,
                                      BkPropAssets *p, BkItemAssets *i,
                                      char error[256]) {
  return create(e, b, p, i, 0, error);
}
BkEntryForest *bk_entry_forest_create_failure(BkEntryAssets *e,
                                              BkBackgroundAssets *b,
                                              BkPropAssets *p, BkItemAssets *i,
                                              char error[256]) {
  return create(e, b, p, i, 1, error);
}
