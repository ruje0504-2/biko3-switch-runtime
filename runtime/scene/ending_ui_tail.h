#ifndef BK_SCENE_ENDING_UI_TAIL_H
#define BK_SCENE_ENDING_UI_TAIL_H
#include "scene/common_hud.h"
#include "scene/ending_ui_cursor.h"
/* Retained fields; entry/controller writes these same owners. No implicit
 * zero/reset here. Normal processed/input arrays have14 slots. */
typedef struct {
  int32_t frame, cycles;   /*719c3c/40*/
  float elapsed, uv_right; /*719c44/575638*/
} BkEndingUiNormalNotice;
typedef struct {
  int32_t mode, choice, once, random_latch;  /*6dde94/722104/6ea310/6ddea0*/
  int32_t processed[2], group_seen[5];       /*6ea178;6ea18c+group*64*/
  int32_t cycles, reset_a, reset_b, reset_c; /*6ea384/344/5546a4/6ddea8*/
  float elapsed, sequence_elapsed;           /*6ea388/350*/
  uint8_t sequence, sequence_count; /*6ea34c/34d, signed reads/wrapping writes*/
} BkEndingUiAuxNotice;
typedef struct {
  BkEndingFrameState *frame;
  const BkEndingControlState *control;
  BkEndingAuxiliaryState *auxiliary;
  BkEndingUiNoticeState *notices;
  uint8_t *flash_wanted;          /*slot52+167, separate from53..56*/
  const int8_t *final_state;      /*6d1c0c*/
  const int8_t *normal_side;      /*719b4c*/
  const int32_t *normal_target;   /*719444*/
  const int32_t *normal_inputs;   /*709c70[14]*/
  int32_t *normal_processed;      /*719b64[14]*/
  float *gauge_y;                 /*721e24*/
  const int32_t *contact_index;   /*721ed4*/
  const int32_t *aux_inputs;      /*6ea170[2]*/
  const int32_t (*aux_config)[6]; /*live6e9fa8[5][6]*/
  uint32_t *random;               /*shared CRT RNG*/
  char (*speech_names)[32];       /*retained722224/722344 filenames*/
  const int32_t *voice_volume;    /*be9a08, re-read after selection*/
} BkEndingUiTailBindings;
typedef struct {
  /* Actual clip/audio/eye adapters, as used by495d92. Required only at
   * their native gates. No stand-in successful callbacks allowed. */
  BkEndingAuxiliaryOps actor;
  int (*speech)(void *, unsigned slot, const char *name, int32_t volume,
                char error[256]); /*4e0956 then4ad2bf, actor.context*/
} BkEndingUiTailOps;
/*4d677b..679d only. Same outer common HUD owner; advances and captures
 * alpha before requesting blocked. Reload follows, and may return early. */
int bk_ending_ui_curtain(BkCommonHudState *, float seconds, BkCommonHudFrame *,
                         char error[256]);
/* Complete4d6e6e..7435 with actual495567/48cb8a and speech selectors.
 * Earlier UI draws are retained. Calls may change live bindings; later
 * branches re-read them. Failure preserves its prefix and terminates frame.
 * Does not perform preceding cursor/reload or imply full flow16 ownership. */
int bk_ending_ui_tail(BkEndingUi *, BkEndingStageUi *, BkEndingUiNormalNotice *,
                      BkEndingUiAuxNotice *, const BkEndingUiTailBindings *,
                      const BkEndingUiTailOps *, float scale, float seconds,
                      BkEndingUiFrame *, char error[256]);
#endif
