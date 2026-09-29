#ifndef BK_GAME_ENDING_SECONDARY_H
#define BK_GAME_ENDING_SECONDARY_H
#include <stdint.h>
/*4D00FA, reached with camera variant1 by4CC582. This is the independent
 * bk3_09 primary-only loader, not action variant1 of normal4CF318. */
typedef struct {
  char primary[32], face[32], visible_nodes[3][32];
  float position[3];
  int32_t yaw, camera_yaw, expression_a, expression_b;
  uint32_t camera_table[5][4];
} BkEndingSecondaryConfig;
int bk_ending_secondary_config(BkEndingSecondaryConfig *, unsigned group);
/*4D0ADD..4D0B97, before the background is inserted. Inputs are the cached
 * world XYZ of node0 and A_okosi. The third vector is deliberately not a
 * midpoint: X=anchor.X, Y=(node.Y-anchor.Y)/2, Z=(node.Z-anchor.Z)/2.
 * Input/output aliasing is supported; invalid input leaves output unchanged.
 * Native nonfinite coordinates are an explicit portable rejection policy. */
int bk_ending_secondary_targets(float output[3][3], const float node[3],
                                 const float anchor[3], char error[256]);
typedef struct {
  void *context;
  int (*load)(void *, unsigned slot, const char *name, char error[256]);
  int (*play)(void *, unsigned slot, int32_t flags, int32_t volume, char error[256]);
} BkEndingSecondarySpeechOps;
/*47d9ee: PH{group+1}03{cue:02}.wav. Commit the existing two retained names
 * before loading, then read the live volume and sign-extend the low flags
 * byte for Play. Keep a private formatted name across callbacks. Failure
 * retains the executed prefix; no guessed duration or successful silent load.
 * Parent uses cues1..18, speech slots0/1. Volume is borrowed process state. */
int bk_ending_secondary_speech(unsigned group, int32_t cue, unsigned slot,
                                uint32_t flags, char names[2][32],
                                const int32_t *volume,
                                const BkEndingSecondarySpeechOps *,
                                char error[256]);
#endif
