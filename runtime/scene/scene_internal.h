#ifndef BK_SCENE_INTERNAL_H
#define BK_SCENE_INTERNAL_H
#include "scene/scene.h"
/* Private implementation ABI; do not include outside scene/. */
struct BkScene {
  int (*step)(BkScene *scene, double seconds, const BkInput *input,
              char error[256]);
  int (*draw)(BkScene *scene, const BkSceneFrame *frame, char error[256]);
  void (*destroy)(BkScene *scene);
};
BkScene *bk_title_create(const BkSceneServices *services, char error[256]);
BkScene *bk_static_world_create(const BkSceneServices *services,
                                char error[256]);
BkScene *bk_camera_track_create(const BkSceneServices *services,
                                char error[256]);
BkScene *bk_actor_preview_create(const BkSceneServices *services,
                                 char error[256]);
#endif
