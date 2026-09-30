#ifndef BK_APP_PLAY_SESSION_H
#define BK_APP_PLAY_SESSION_H
#include "save/checkpoint_file.h"
#include "save/unlock_file.h"
#include "save/record_file.h"
#include "scene/common_hud.h"
#include "scene/flow_loading.h"
#include "scene/game_frame.h"
#include "scene/scene.h"
/* Application owner for retained game/UI state and native flows2/4/50.
 * A step must be drawn and presented before the next step. after_present
 * performs screenshot-dependent ownership changes outside the GPU frame.
 * Zero-step redraws are allowed and never repeat transition side effects. */
BkScene *bk_play_session_create(const BkSceneServices *, char error[256]);
/* Writable port files are explicit, borrowed for the session lifetime.
 * Without this service entering flow28 fails rather than simulating saves. */
BkScene *bk_play_session_create_with_saves(const BkSceneServices *,
                                           BkCheckpointFiles *,
                                           char error[256]);
/* Normal application also reads the independent saved gallery table. Both
 * file owners are borrowed. Ending progress producer remains a separate flow. */
BkScene *bk_play_session_create_with_storage(const BkSceneServices *,
                                             BkCheckpointFiles *, BkUnlockFile *,
                                             char error[256]);
/* Production storage: read records once after the saved unlock table, write
 * all records before stopping flow10. Missing writable records at exit is
 * an explicit error. Legacy constructors above are in-memory diagnostics. */
BkScene *bk_play_session_create_with_progress(const BkSceneServices *,
    BkCheckpointFiles *, BkUnlockFile *, BkRecordFile *, char error[256]);
/* Explicit diagnostic entry for existing game/save/failure regressions.
 * Bypasses title/selection/dialogue; never the normal Switch startup. */
BkScene *bk_play_session_create_development(const BkSceneServices *,
                                            BkCheckpointFiles *,
                                            char error[256]);
/* Native game step and real wall time are independent. wall_seconds uses a
 * process-relative epoch >=1 and drives timers/face clocks, never audio rate.
 */
int bk_play_session_step_at(BkScene *, double game_seconds, double wall_seconds,
                            const BkInput *, char error[256]);
double bk_play_session_wall_seconds(BkScene *);
int bk_play_session_after_present(BkScene *, char error[256]);
const BkGameFrameState *bk_play_session_state(BkScene *);
const BkCommonHudState *bk_play_session_common(BkScene *);
const BkFlowTransition *bk_play_session_flow(BkScene *);
/* Original466448 terminal flow58, after the final loading snapshot. */
int bk_play_session_finished(BkScene *);
#endif
