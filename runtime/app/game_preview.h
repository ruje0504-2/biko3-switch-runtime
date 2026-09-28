#ifndef BK_APP_GAME_PREVIEW_H
#define BK_APP_GAME_PREVIEW_H
#include "scene/failure_hud.h"
#include "scene/game_frame.h"
#include "scene/rain_render.h"
#include "scene/scene.h"
BkScene *bk_game_preview_create(const BkSceneServices *, char error[256]);
/* Borrow a read-only state from a scene created by this factory. */
const BkGameFrameState *bk_game_preview_state(BkScene *);
BkAudio *bk_game_preview_audio(BkScene *);
/* Entry loader borrows retained process state/progress. Never applies boot
 * defaults. The caller owns these until the entry is destroyed. */
BkScene *bk_game_preview_create_entry(const BkSceneServices *,
                                      BkGameFrameState *,
                                      const BkEntryProgress *, uint32_t group,
                                      uint32_t area, uint8_t previous_flow,
                                      double elapsed, char error[256]);
void bk_game_preview_block(BkScene *, uint8_t blocked);
uint32_t bk_game_preview_now(BkScene *);
void bk_game_preview_clock(BkScene *, double elapsed);
/*51a77c: music/continuous ambient gain only; timer/RNG/animation hold. */
int bk_game_preview_background_step(BkScene *, double seconds, double elapsed,
                                    char error[256]);
int bk_game_preview_resume(BkScene *, char error[256]);
/*4ec87c when cancelling the load page back to retained pause/game. */
int bk_game_preview_restore_item_text(BkScene *, char error[256]);
/* Reuses the retained entry but rebuilds player borrowers outside frames. */
int bk_game_preview_load_failure(BkScene *, char error[256]);
int bk_game_preview_release_failure_audio(BkScene *, char error[256]);
int bk_game_preview_failure_step(BkScene *, BkFailureHudState *,
                                 BkCommonHudState *, uint8_t *overlay,
                                 const BkFailureHudOps *flow_ops,
                                 double seconds, double elapsed,
                                 const BkInput *, BkFailureHudFrame *,
                                 char error[256]);
/*flow20 retains actors/HUD/audio while releasing the previous world. All
 * calls outside active GPU frames. Failure terminates this session. */
int bk_game_preview_suspend_area(BkScene *, char error[256]);
int bk_game_preview_advance_area(BkScene *, uint8_t previous_flow,
                                 char error[256]);
/* Live field borrowed by checkpoint menu; valid until this game is retired. */
float *bk_game_preview_hud_reserve(BkScene *);
/* Read-only integration diagnostics; no RNG/animation or GPU mutation. */
typedef struct {
  BkRainState rain;
  BkRainDraw rain_draw;
  uint32_t random_before, random_after;
  int snow_present;
  BkClipState snow_clock;
  uint32_t snow_instances;
} BkGameWeatherSnapshot;
int bk_game_preview_weather(BkScene *, BkGameWeatherSnapshot *);
#endif
