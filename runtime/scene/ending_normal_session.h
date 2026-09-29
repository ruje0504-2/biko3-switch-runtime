#ifndef BK_SCENE_ENDING_NORMAL_SESSION_H
#define BK_SCENE_ENDING_NORMAL_SESSION_H
#include "scene/scene.h"
#include "scene/ending_state.h"
#include "game/ending_record.h"
#include "scene/ending_normal_controller.h"
#include "scene/ending_presentation.h"
#include "game/ending_secondary_control.h"
#include "game/ending_secondary_presentation.h"
#include "scene/ending_tertiary_controller.h"
typedef struct {
  BkCommonHudState *common; /*borrowed application curtain/action/wanted*/
  void *context;
  /* Original51c47e. May logically stop this scene; must retain its resources
   * until the prepared final snapshot has been presented. */
  int (*schedule)(void *, uint8_t target, uint8_t mode, char error[256]);
  double wall_seconds; /*application clock at load, independent of game dt*/
  BkEndingAuxiliaryCycle *auxiliary_cycle; /*process owner, survives reloads*/
  uint32_t *random; /*same process RNG used by gameplay and front-end scenes*/
  BkEndingNormalControllerRetained *normal_controller;
  BkEndingPresentationRetained *presentation;
  int32_t *duck_transition; /*process719c5c, initially0; survives resource reloads*/
  /*Independent4D00FA counters, shared across entries and stage reloads.
   * Required when selecting that loader with an application-owned flow. */
  BkEndingSecondaryControlState *secondary_controller;
  BkEndingSecondaryPresentationState *secondary_presentation;
  /* One process owner, initialized once with bk_ending_state_initialize.
   * Loaders borrow its live fields; retiring old draw snapshots must never
   * reset the state of an entry that has subsequently acquired this owner. */
  BkEndingState *state;
  BkEndingTertiaryControllerRetained *tertiary_controller;
} BkEndingNormalFlow;
/* Explicit normal-ending scene owner. The default diagnostic entry is the
 * gallery normal branch (previous flow0x18, selection0, action variant0/1); it owns the
 * actual CPU/GPU/audio resources and exposes no synthetic success callback. */
BkScene *bk_ending_normal_scene_create(const BkSceneServices *, unsigned group,
                                       unsigned variant, char error[256]);
/* Logical release: stop all owned audio at the next unsubmitted sample and
 * reject further updates. Keep the prepared CPU/GPU snapshot drawable until
 * destruction after present. Repeated stop/destruction must not touch a new
 * owner's mixer slots. Already submitted PCM remains consumable. */
int bk_ending_normal_scene_stop(BkScene *, char error[256]);
/* Must be called after the renderer has presented the prepared snapshot. */
int bk_ending_normal_scene_after_present(BkScene *, char error[256]);
/* Same independent game/wall clock contract as the outer play session.
 * The loader supplies its initial wall time; subsequent application updates
 * use this entry. Plain diagnostic steps accumulate fractional milliseconds
 * instead of truncating each frame. Redraws never advance either clock.
 * Nonfinite/backward time or a pending snapshot rejects before mutation. */
int bk_ending_normal_scene_step_at(BkScene *, double game_seconds,
                                   double wall_seconds, const BkInput *,
                                   char error[256]);
double bk_ending_normal_scene_wall_seconds(const BkScene *);
/* Actual modulo32 sample consumed by clock/face/movie services. */
uint32_t bk_ending_normal_scene_milliseconds(const BkScene *);
const BkEndingState *bk_ending_normal_scene_state(const BkScene *);
/*4eb8f6/50ca48 copies all saved bytes after loader initialization and before
 * the first actual ending frame. Both records and the saved table are
 * required. Only records remain borrowed; unlocked is copied during create.
 * Application entries supply flow with the same common owner as flow50 and
 * the live process RNG. Loading, face updates and UI/auxiliary events borrow
 * that RNG directly; scene creation and retirement never reseed it.
 * NULL flow is an explicit standalone diagnostic: transition requests fail
 * when they actually need the absent application scheduler. */
BkScene *bk_ending_normal_scene_create_story(const BkSceneServices *,
                                             unsigned group, unsigned variant,
                                             BkEndingRecords *,
                                             const uint8_t unlocked[5][8],
                                             const BkEndingNormalFlow *flow,
                                             char error[256]);
/*Actual4CC582 gallery selection1 dispatch into4D00FA, with its real parent,
 * presentation, common camera/UI, input, audio and dual-view renderer.
 * background_variant remains independent of the fixed camera variant1.
 * All step/stop/after-present/state accessors above accept this scene.
 * The supplied unlock table is copied after loading, before the first frame.
 * NULL flow is an explicit standalone diagnostic with local process owners;
 * transitions requiring the application scheduler still fail when reached.
 * This constructor does not implement the front-end gallery menu. */
BkScene *bk_ending_secondary_scene_create_gallery(const BkSceneServices *,
    unsigned group, unsigned background_variant, const uint8_t unlocked[5][8],
    const BkEndingNormalFlow *, char error[256]);
/*Actual4CC582 gallery selections2/5 into4D1025. The selected loader receives
 *the native group-specific argument and owns its primary/tracks/background
 *topology; this constructor does not replace the front-end gallery menu.*/
BkScene *bk_ending_selected_scene_create_gallery(const BkSceneServices *,
    unsigned group, unsigned background_variant, uint32_t selection,
    const uint8_t unlocked[5][8], const BkEndingNormalFlow *,
    char error[256]);
#endif
