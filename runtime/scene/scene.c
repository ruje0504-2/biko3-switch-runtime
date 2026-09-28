#include "scene/scene_internal.h"
#include <math.h>
#include <stdlib.h>
typedef struct {
  BkScene base;
  void *context;
  BkSceneCustomOps ops;
} BkCustomScene;
static int custom_step(BkScene *base, double seconds, const BkInput *input,
                       char error[256]) {
  BkCustomScene *s = (BkCustomScene *)base;
  return s->ops.step(s->context, seconds, input, error);
}
static int custom_draw(BkScene *base, const BkSceneFrame *frame,
                       char error[256]) {
  BkCustomScene *s = (BkCustomScene *)base;
  return s->ops.draw(s->context, frame, error);
}
static void custom_destroy(BkScene *base) {
  BkCustomScene *s = (BkCustomScene *)base;
  s->ops.destroy(s->context);
  free(s);
}
void *bk_scene_custom_context(BkScene *scene) {
  if (!scene || scene->step != custom_step)
    return NULL;
  return ((BkCustomScene *)scene)->context;
}
BkScene *bk_scene_custom_create(void *context, BkSceneCustomOps ops,
                                char error[256]) {
  if (!context || !ops.step || !ops.draw || !ops.destroy) {
    snprintf(error, 256, "custom scene services are incomplete");
    return NULL;
  }
  BkCustomScene *s = calloc(1, sizeof(*s));
  if (!s) {
    snprintf(error, 256, "custom scene allocation failed");
    return NULL;
  }
  s->base = (BkScene){custom_step, custom_draw, custom_destroy};
  s->context = context;
  s->ops = ops;
  return &s->base;
}
BkScene *bk_scene_create(BkSceneKind kind, const BkSceneServices *services,
                         char error[256]) {
  if (!services || !services->resources || !services->renderer) {
    snprintf(error, 256, "scene services are incomplete");
    return NULL;
  }
  if (kind == BK_SCENE_TITLE_PREVIEW)
    return bk_title_create(services, error);
  if (kind == BK_SCENE_STATIC_WORLD)
    return bk_static_world_create(services, error);
  if (kind == BK_SCENE_CAMERA_TRACK)
    return bk_camera_track_create(services, error);
  if (kind == BK_SCENE_ACTOR_PREVIEW)
    return bk_actor_preview_create(services, error);
  snprintf(error, 256, "scene %d is not implemented", (int)kind);
  return NULL;
}
int bk_scene_step(BkScene *scene, double seconds, const BkInput *input,
                  char error[256]) {
  if (!scene || !input || !isfinite(seconds) || seconds <= 0) {
    snprintf(error, 256, "invalid simulation step");
    return 0;
  }
  return scene->step ? scene->step(scene, seconds, input, error) : 1;
}
int bk_scene_draw(BkScene *scene, const BkSceneFrame *frame, char error[256]) {
  if (!scene || !frame) {
    snprintf(error, 256, "invalid scene/frame");
    return 0;
  }
  return scene->draw(scene, frame, error);
}
void bk_scene_destroy(BkScene *scene) {
  if (scene)
    scene->destroy(scene);
}
