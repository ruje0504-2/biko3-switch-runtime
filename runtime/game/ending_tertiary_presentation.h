#ifndef BK_GAME_ENDING_TERTIARY_PRESENTATION_H
#define BK_GAME_ENDING_TERTIARY_PRESENTATION_H
#include "game/ending_auxiliary.h"
#include "world/face_controller.h"

typedef enum {
  BK_ENDING_TERTIARY_PRIMARY,
  BK_ENDING_TERTIARY_AUXILIARY_FIRST,
  BK_ENDING_TERTIARY_AUXILIARY_SECOND,
  BK_ENDING_TERTIARY_BACKGROUND
} BkEndingTertiaryActor;
typedef enum {
  BK_ENDING_TERTIARY_4E18AD,
  BK_ENDING_TERTIARY_4A9019
} BkEndingTertiaryControlledAdvance;
typedef struct {
  BkEndingFrameState *frame;
  BkEndingAuxiliaryState *auxiliary;
  BkFaceState *face;
  const int32_t *state; /*721ee8, shared with476720*/
  const int32_t *face_mode; /*721dfc*/
  int32_t *expression_override; /*6a3c24*/
  int32_t *eye_lower; /*721df8*/
  int32_t *expression_latch; /*same6afd04 as476720's third voice latch*/
  const uint8_t *toggles;
  const uint32_t *background_root;
  const uint32_t *hidden_nodes; /*three709ef8 nodes; zero absent*/
  const uint32_t *secondary_node; /*second caller argument's node+4*/
  const int32_t *binding_count; /*original face owner+3a30*/
  uint32_t binding_capacity; /*actual pairs available from the scene owner*/
} BkEndingTertiaryPresentationBindings;
typedef struct {
  void *context;
  int (*clock)(void *, uint32_t *, char[256]);
  int (*advance)(void *, BkEndingTertiaryActor, float, char[256]);
  int (*active)(void *, BkEndingTertiaryActor, int32_t *, char[256]);
  int (*hide)(void *, uint32_t node, uint32_t hidden, char[256]);
  int (*disable_bom)(void *, unsigned binding, uint32_t disabled, char[256]);
  /*Original helpers receive the actual actor and a zero argument. They
   * remain mandatory distinct services; ordinary ANIM cannot replace them.*/
  int (*controlled)(void *, BkEndingTertiaryActor,
                     BkEndingTertiaryControlledAdvance, char[256]);
  /*54b828: four actual material names/group. The scene must resolve names
   * in its loaded registry; this selector is not a guessed material name.*/
  int (*material)(void *, unsigned group, unsigned slot, uint32_t hidden,
                   float alpha, char[256]);
  int (*publish)(void *, char[256]);
  /*Per-binding position422c49 followed by direction4230bd. The scene
   * resolves the live pair for each call. Native direction is(0,0,1),(0,1,0).*/
  int (*follow)(void *, unsigned binding, int direction, char[256]);
  int (*expression)(void *, int32_t, char[256]);
  int (*eye_range)(void *, float minimum, float maximum, char[256]);
  int (*gaze)(void *, float minimum, float maximum, char[256]);
  int (*blink)(void *, uint32_t timestamp, char[256]);
  int (*level)(void *, float *, char[256]);
  int (*mouth)(void *, float level, uint32_t timestamp, char[256]);
} BkEndingTertiaryPresentationOps;
/*Entire479137. Full-speed background before hiding, three optional nodes,
 * group2's five BOM gates and two independent auxiliary actors. Two forest
 * publications surround per-binding alignment. Expression switching uses
 * the actual face blink_phase/rapid_count and retains the captured clock.
 * No actor loader, input child, GPU scene or fabricated audio is implied.
 * All required services fail when absent; later failures keep their prefix.
 * The scene supplies its actual pair capacity; reject a larger access at
 * the first out-of-bounds iteration, retaining preceding valid operations.*/
int bk_ending_tertiary_presentation_step(
    const BkEndingTertiaryPresentationBindings *, float seconds,
    const BkEndingTertiaryPresentationOps *, char error[256]);
#endif
