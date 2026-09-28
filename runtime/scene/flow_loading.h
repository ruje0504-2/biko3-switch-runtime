#ifndef BK_SCENE_FLOW_LOADING_H
#define BK_SCENE_FLOW_LOADING_H
#include "game/flow_transition.h"
#include "scene/common_hud.h"
typedef struct {
  BkFadeSprite background, special_background, prompt_base;
  BkPulseSprite prompt;
  uint8_t awaiting; /*BFBBA8; retains across requests and initialization*/
} BkFlowLoadingState;
typedef enum {
  BK_LOADING_BACKGROUND, /*ma_03.bmp,BE9898*/
  BK_LOADING_SPECIAL,    /*te_01.bmp,B53568*/
  BK_LOADING_CURTAIN,    /*ma_01.tga,BEEA18*/
  BK_LOADING_PROMPT,     /*za_00.bmp,B53960*/
  BK_LOADING_PULSE       /*za_01.bmp,B53ACC*/
} BkFlowLoadingAsset;
typedef struct {
  BkFlowLoadingAsset asset;
  float alpha;
} BkFlowLoadingDraw;
typedef struct {
  unsigned count;
  BkFlowLoadingDraw draws[4];
} BkFlowLoadingFrame;
typedef struct {
  void *context;
  /* Actual4e7671 resource loader; failures are fatal, not pretend success. */
  int (*load)(void *, uint8_t target, char error[256]);
  /* System slot1/se001 restart at original46435e boundary. */
  int (*confirm)(void *, char error[256]);
} BkFlowLoadingOps;
/* Represented4e6dee sprites only; special resource reset only if special==1.
 * Retains awaiting and the pulse direction. Common curtain owned separately. */
void bk_flow_loading_initialize(BkFlowLoadingState *, uint8_t special);
/*4e6dee constructor geometry x,y,width,height. Backgrounds use width/1280;
 * the48px prompts independently use width/1024 and height/768. */
int bk_flow_loading_layout(float out[4], BkFlowLoadingAsset, unsigned width,
                           unsigned height);
const char *bk_flow_loading_image(BkFlowLoadingAsset);
/* Complete51c4bf, called only during flow50's UI dispatch. advance is the
 * edge pair chain0/Z/33450(1,0). Draw snapshots precede later loader changes;
 * in particular confirmation still draws both prompts on the accepting frame.
 * mode0/2 loads immediately, mode3 only switches, other modes keep holding.
 * All services required; partial native-ordered changes survive failures. */
int bk_flow_loading_step(BkFlowLoadingState *, BkCommonHudState *,
                         BkFlowTransition *, uint8_t special, int advance,
                         float seconds, const BkFlowLoadingOps *,
                         BkFlowLoadingFrame *, char error[256]);
#endif
