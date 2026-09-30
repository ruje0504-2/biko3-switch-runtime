#ifndef BK_GAME_ENDING_GALLERY_PRESENTATION_H
#define BK_GAME_ENDING_GALLERY_PRESENTATION_H
#include "game/ending_auxiliary.h"
#include "world/face_controller.h"

typedef enum {
  BK_ENDING_GALLERY_PRIMARY,
  BK_ENDING_GALLERY_SECONDARY,
  BK_ENDING_GALLERY_THIRD,
  BK_ENDING_GALLERY_BACKGROUND
} BkEndingGalleryActor;
typedef struct {
  BkEndingFrameState *frame;
  BkEndingAuxiliaryState *auxiliary;
  BkFaceState *face;
  const int8_t *area;                 /*721b3d*/
  const int8_t *action, *requested;   /*6ddce0/6d1be1*/
  const int32_t *cursor, *workspace;  /*6c7f74/6c7f80*/
  uint32_t workspace_capacity;
  const int32_t *reverse;            /*725704; only1 selects402e18*/
  const int32_t *face_mode;          /*721dfc*/
  int32_t *expression_override;      /*6c7f78*/
  int32_t *eye_lower;                /*721df8*/
  uint8_t *expression_latch;         /*6dde52, one byte*/
  uint8_t *mouth_falling;            /*6dde68, one byte*/
  float *mouth_level;                /*6dde6c, process-retained*/
  const uint8_t *toggles;
  const uint32_t *primary_root, *background_root;
  const uint32_t *secondary_root, *third_root; /*zero means absent*/
  const uint32_t *hidden_nodes;      /*three709ef8 nodes*/
  const uint32_t *secondary_node;    /*719b44+4*/
  const int32_t *binding_count;
  uint32_t binding_capacity;
} BkEndingGalleryPresentationBindings;
typedef struct {
  void *context;
  int (*clock)(void *, uint32_t *, char[256]);
  /*reverse0 is4026fe; reverse1 is402e18 with a negative amount. Neither
   *may be replaced with a source-time edit without the actual sampling.*/
  int (*advance)(void *, BkEndingGalleryActor, float amount, int reverse,
                  char[256]);
  int (*active)(void *, BkEndingGalleryActor, int32_t *, char[256]);
  int (*find)(void *, uint32_t root, const char *, uint32_t *, char[256]);
  int (*hide)(void *, uint32_t node, uint32_t hidden, char[256]);
  /*552554's five rows of four names, verified equal to56e368.*/
  int (*material)(void *, unsigned group, unsigned slot, uint32_t hidden,
                   float alpha, char[256]);
  int (*disable_bom)(void *, unsigned binding, uint32_t disabled, char[256]);
  int (*effect)(void *, char[256]); /*mandatory48cc18 boundary in state6*/
  /*Read-only live timing; rewind writes source=start on the same private
   *descriptor, without selection, sampling or publication.*/
  int (*timing)(void *, unsigned slot, BkEndingClipTiming *, char[256]);
  int (*rewind)(void *, unsigned slot, char[256]);
  int (*publish)(void *, char[256]);
  int (*follow)(void *, unsigned binding, int direction, char[256]);
  int (*eye_range)(void *, float minimum, float maximum, char[256]);
  int (*gaze)(void *, float minimum, float maximum, char[256]);
  int (*expression)(void *, int32_t, char[256]);
  int (*blink)(void *, uint32_t, char[256]);
  int (*level)(void *, float *, char[256]);
  int (*mouth)(void *, float, uint32_t, char[256]);
} BkEndingGalleryPresentationOps;
/*Entire48bcbb orchestration. All aliases remain owned by the process or
 *live scene; this call does not initialize/reset them. Captures one clock,
 *advances/hides the background, selects BOM and directional playback,
 *rewinds only slots17/18 at the native gates, publishes twice around BOM
 *alignment, then runs the distinct range/gaze/expression/blink/mouth order.
 *Services may change live aliases; later branches reread them. Bounded
 *workspace/pair accesses reject at the access point, preserving the prefix.
 *Missing child services fail. This is not48302b,48cc18 or a gallery loader.*/
int bk_ending_gallery_presentation_step(
    const BkEndingGalleryPresentationBindings *, float seconds,
    const BkEndingGalleryPresentationOps *, char error[256]);
#endif
