#ifndef BK_SCENE_ENDING_RETAINED_H
#define BK_SCENE_ENDING_RETAINED_H
#include <stdint.h>
/* Additional scalar/table owners written by the six native leave routines.
 * Address labels deliberately avoid guessing the remaining controllers'
 * semantics. Already modeled aliases live in BkEndingState/UI, never here.
 * No native pointers, resource handles, saved unlocks or persistent records.
 */
typedef struct {
  int32_t word_719b24;
  /* Old address-label aliases retain the verified snapshot ABI. Runtime
   * node aliases contain actual forest IDs, zero for absent; never x86
   * addresses. The owning scene must retire/reset them with its forest. */
  union { int32_t word_719448; uint32_t follow_target; };
  union { int32_t word_719b28; uint32_t direct_reference; };
  union { int32_t word_719b2c; uint32_t direct_node; };
  int32_t word_719b40, word_719b50, words_719b54[3];
} BkEndingRetainedNormal;
typedef struct {
  int32_t words_6bbe2c[2];
  uint8_t byte_6bbe34;
  int32_t word_6bbe48; /*48181F expression restore latch*/
} BkEndingRetainedStage3;
typedef struct {
  int32_t word_6ea16c;
  /*6ea180 contains five64-byte rows; each row's fourth word is the
   * existing ui_controller.auxiliary.group_seen entry. */
  int32_t group_prefix[5][3], group_suffix[5][12];
  int32_t words_6ea2c0[20], word_6ea314, word_5546a0;
  int32_t words_6ea028[75], words_6ea318[10];
  int32_t word_6ea340, word_6ddec0, word_6ddec8, word_6dde90;
  int32_t word_6ea348, word_6ea354, word_6ea020;
  uint8_t byte_6ea358;
} BkEndingRetainedAuxiliary;
typedef struct {
  int32_t word_6a3c20, words_6afcfc[3], word_6afd08;
  int32_t word_6a3c24, word_54ccc8;
  uint8_t byte_6afd18;
  float value_54ccd0;
  /*6afd0c/10 are BkEndingState.unavailable;6afd14 is the existing
   * ui_controller.hints.movement_ready owner. Neither is duplicated. */
} BkEndingRetainedStage2;
typedef struct {
  int32_t words_6c7f44[2];
  uint8_t bytes_6c7f54[10], bytes_6c7f60[10];
  int32_t word_54e2f8, word_6c7f48, word_6c7f4c;
  uint8_t byte_6c7f50;
  float value_54e310;
  /* State1's idle speech timer and randomized delay survive state4. */
  float timer_6c7f6c;
  int32_t delay_54f8e0;
} BkEndingRetainedStage4;
typedef struct {
  uint8_t byte_6c7f70, byte_6d1be0, byte_6d1bd4, byte_6d1c0d;
  uint8_t byte_6ddce0, byte_6d1be1;
  int32_t word_6c7f74, word_6dde4c, word_6d1bcc;
  uint8_t byte_6dde50, byte_6dde51;
  int32_t word_6d1bd8, word_6d1bdc, word_6dde54;
  uint8_t byte_6dde58;
  int32_t workspace_6c7f80[10000];
  int32_t words_6dde24[10], words_6d1be8[9], words_6ddce4[75];
  /*6d1c0c is BkEndingState.final_state. The40KB workspace is unrelated
   * to the five process recording lanes at B550B0. */
} BkEndingRetainedFinal;
typedef struct {
  BkEndingRetainedNormal normal;
  BkEndingRetainedStage3 stage3;
  BkEndingRetainedAuxiliary auxiliary;
  BkEndingRetainedStage2 stage2;
  BkEndingRetainedStage4 stage4;
  BkEndingRetainedFinal final;
} BkEndingRetained;
#endif
