#ifndef BK_APP_PAUSE_SESSION_H
#define BK_APP_PAUSE_SESSION_H
#include "app/pause_resources.h"
#include "scene/system_audio.h"
typedef struct BkPauseSession BkPauseSession;
typedef struct {
  void *context;
  int (*warp)(void *, float x, float y, char error[256]);
  int (*pointer)(void *, float position[2], float motion[2], char error[256]);
  /* Releases other flows (currently game2). Pause4 is owned by this session.
   * Must retain the borrowed live UI/flow state while the last frame finishes.
   */
  int (*release_other)(void *, uint8_t flow, char error[256]);
} BkPauseSessionOps;
/* Borrows live CPU state, bindings, files and persistent system
 * sounds0/1/2/3/5. Owns GPU menu resources. Loads the actual sy_99.bmp before
 * applying5158a0. The caller initializes shared cursor once at process startup.
 * No game-state reset, cursor reset, album deletion or fabricated resource
 * release here. */
BkPauseSession *bk_pause_session_create(BkRenderer *, BkResourceStore *,
                                        BkCaptureFiles *, size_t capture_limit,
                                        BkPauseState *, const BkPauseBindings *,
                                        BkSystemAudio *const sounds[8],
                                        const BkPauseSessionOps *,
                                        unsigned width, unsigned height,
                                        char error[256]);
/* Outside an active GPU frame, snapshot backdrop, execute UI and prepare its
 * ordered draws. Background4f720c must run earlier in the enclosing update.
 * After current flow changes, submit this last snapshot before destroying the
 * session outside the GPU frame. The destructor waits via renderer resources.
 * Any failed step/draw terminates the session; do not retry partial state. */
int bk_pause_session_step(BkPauseSession *, const BkPauseInput *,
                          BkPauseFrame *snapshot, char error[256]);
int bk_pause_session_draw(BkPauseSession *, char error[256]);
void bk_pause_session_destroy(BkPauseSession *);
#endif
