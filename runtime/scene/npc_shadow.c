#include "scene/npc_shadow.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkNpcShadow {
  BkModel *model;
  BkClipSet *clips;
  BkActorPose *pose;
  uint32_t root;
};
void bk_npc_shadow_destroy(BkNpcShadow *s) {
  if (!s)
    return;
  bk_actor_pose_destroy(s->pose);
  bk_clip_set_destroy(s->clips);
  bk_model_destroy(s->model);
  free(s);
}
BkNpcShadow *bk_npc_shadow_create(BkResourceStore *resources, char error[256]) {
  if (!resources) {
    snprintf(error, 256, "NPC shadow: missing resources");
    return NULL;
  }
  BkNpcShadow *s = calloc(1, sizeof(*s));
  if (!s) {
    snprintf(error, 256, "NPC shadow: allocation failed");
    return NULL;
  }
  BkBlob blob = {0};
  if (bk_resources_read(resources, "bk3_01", "kage_01.xan", &blob, error) !=
      BK_RESOURCE_OK)
    goto bad;
  s->clips = bk_clip_set_decode(blob.data, blob.size, error);
  bk_blob_free(&blob);
  if (!s->clips)
    goto bad;
  if (bk_resources_read(resources, "bk3_01", bk_clip_model_name(s->clips),
                        &blob, error) != BK_RESOURCE_OK)
    goto bad;
  int decoded =
      bk_model_decode(blob.data, blob.size, &s->model, error) == BK_MODEL_OK;
  bk_blob_free(&blob);
  if (!decoded)
    goto bad;
  s->root = BK_MODEL_NONE;
  for (uint32_t i = 0; i < s->model->frame_count; i++)
    if (s->model->frames[i].parent_index == BK_MODEL_NONE) {
      if (s->root != BK_MODEL_NONE) {
        snprintf(error, 256, "NPC shadow: multiple roots");
        goto bad;
      }
      s->root = i;
    }
  const float origin[3] = {0};
  s->pose = bk_actor_pose_create(s->model, s->clips, s->root, NULL, origin, 0,
                                 0, 1, error);
  if (!s->pose)
    goto bad;
  return s;
bad:
  bk_blob_free(&blob);
  bk_npc_shadow_destroy(s);
  return NULL;
}
const BkActorPose *bk_npc_shadow_pose(const BkNpcShadow *s) {
  return s ? s->pose : NULL;
}
BkActorPose *bk_npc_shadow_bind_pose(BkNpcShadow *s) {
  return s ? s->pose : NULL;
}
int bk_npc_shadow_place(BkNpcShadow *s, const BkActorPlacement *body,
                        char error[256]) {
  if (!s || !body) {
    snprintf(error, 256, "NPC shadow: missing placement");
    return 0;
  }
  float position[3];
  memcpy(position, body->position, sizeof(position));
  position[1] = (float)((double)position[1] + .1f);
  return bk_actor_pose_place(s->pose, position, body->yaw_degrees, error);
}
int bk_npc_shadow_step(BkNpcShadow *s, uint8_t hidden, float seconds,
                       char error[256]) {
  if (!s || !isfinite(seconds) || seconds < 0 ||
      (double)seconds * 60 >= INT32_MAX) {
    snprintf(error, 256, "NPC shadow: invalid instance/timestep");
    return 0;
  }
  uint32_t previous;
  if (!bk_actor_pose_hidden(s->pose, s->root, &previous))
    return 0;
  BkActorVisibilityEdit edit = {s->root, hidden};
  if (!bk_actor_pose_visibility(s->pose, &edit, 1, error))
    return 0;
  const BkActorPlacement *p = bk_actor_pose_placement(s->pose);
  if (!bk_actor_pose_step(s->pose, p->position, p->yaw_degrees, -1, seconds,
                          error)) {
    char ignored[256];
    edit.hidden = previous;
    bk_actor_pose_visibility(s->pose, &edit, 1, ignored);
    return 0;
  }
  return 1;
}
void bk_npc_shadow_publish(BkNpcShadow *s) {
  if (s)
    bk_actor_pose_publish(s->pose);
}

int bk_npc_shadow_visibility(BkNpcShadow *s, uint8_t hidden, char error[256]) {
  if (!s) {
    snprintf(error, 256, "mesh shadow: missing instance");
    return 0;
  }
  BkActorVisibilityEdit edit = {s->root, hidden};
  return bk_actor_pose_visibility(s->pose, &edit, 1, error);
}
int bk_npc_shadow_place_exact(BkNpcShadow *s, const BkActorPlacement *root,
                              char error[256]) {
  if (!s || !root) {
    snprintf(error, 256, "mesh shadow: missing placement");
    return 0;
  }
  return bk_actor_pose_place(s->pose, root->position, root->yaw_degrees, error);
}
