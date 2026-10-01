#ifndef BK_SCENE_H
#define BK_SCENE_H
#include "core/clock.h"
#include "core/input.h"
#include "core/audio_volume.h"
#include "media/audio.h"
#include "render/renderer.h"
#include "resource/store.h"
struct BkCaptureFiles;
typedef struct BkScene BkScene;
typedef enum {
  BK_SCENE_TITLE_PREVIEW,
  BK_SCENE_STATIC_WORLD,
  BK_SCENE_CAMERA_TRACK,
  BK_SCENE_ACTOR_PREVIEW,
  BK_SCENE_PAUSE_PREVIEW,
  BK_SCENE_GAME,
  BK_SCENE_ENDING_PREVIEW
} BkSceneKind;
typedef struct {
  BkResourceStore *resources;
  BkRenderer *renderer;
  FILE *log;
  BkAudio *audio; /* Borrowed; NULL only for explicitly silent diagnostics. */
  struct BkCaptureFiles *capture_files; /* Borrowed pause-output adapter. */
  const int32_t *audio_volumes; /* Borrowed process voice/BGM/effect values. */
} BkSceneServices;
typedef struct {
  BkClockFrame clock;
  BkInput input;
} BkSceneFrame;
typedef struct {
  int (*step)(void *, double, const BkInput *, char error[256]);
  int (*draw)(void *, const BkSceneFrame *, char error[256]);
  void (*destroy)(void *);
} BkSceneCustomOps;
/* App-owned scenes can use the same lifecycle without reaching into the
 * private scene ABI. The scene layer owns this wrapper and calls destroy once.
 */
BkScene *bk_scene_custom_create(void *context, BkSceneCustomOps,
                                char error[256]);
/* Services outlive scene. Constructor loads all resources before first draw.
 * Unsupported scenes fail explicitly. Scene owns textures and CPU scene data.
 * Renderer begin/end and simulation scheduling belong to the application. */
BkScene *bk_scene_create(BkSceneKind kind, const BkSceneServices *services,
                         char error[256]);
/* One update; caller chooses the diagnostic fixed step or native game step.
 * Input edges are supplied only once when consuming a pending sample.
 * Title has no update callback; the static office updates only its inspection
 * camera. The XAN clip diagnostic schedules ANIM without actor/game dispatch.
 */
int bk_scene_step(BkScene *scene, double seconds, const BkInput *input,
                  char error[256]);
int bk_scene_draw(BkScene *scene, const BkSceneFrame *frame, char error[256]);
void bk_scene_destroy(BkScene *scene);
void *bk_scene_custom_context(BkScene *scene);
#endif
