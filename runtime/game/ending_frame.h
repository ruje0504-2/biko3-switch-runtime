#ifndef BK_GAME_ENDING_FRAME_H
#define BK_GAME_ENDING_FRAME_H
#include <stdint.h>
/* Original4d74c9..4d7a95 after pointer capture/conversion. Operation names
 * retain original addresses until each child controller is recovered.
 * They identify portable services, never callable native addresses. */
typedef enum {
  BK_ENDING_COMMON_4D7AC4,
  BK_ENDING_STAGE_4DB608,
  BK_ENDING_STAGE_4DF6C0,
  BK_ENDING_STAGE_47A5D0,
  BK_ENDING_STAGE_47D3CB,
  BK_ENDING_STAGE_48D8E9,
  BK_ENDING_STAGE_494015,
  BK_ENDING_STAGE_476720,
  BK_ENDING_STAGE_479137,
  BK_ENDING_STAGE_47DC79,
  BK_ENDING_STAGE_48181F,
  BK_ENDING_STAGE_48302B,
  BK_ENDING_STAGE_48BCBB,
  BK_ENDING_STAGE_4E2223,
  BK_ENDING_AUXILIARY_4965B9,
  BK_ENDING_CAMERA_4BB0A4,
  BK_ENDING_CAMERA_4E1D16,
  BK_ENDING_CAMERA_4E1711,
  BK_ENDING_CAMERA_4E0ECB,
  BK_ENDING_CAMERA_4BC444,
  BK_ENDING_CAMERA_4DF411
} BkEndingOperation;
enum {
  BK_ENDING_PLAYER = 1,   /*71af38*/
  BK_ENDING_CONTROL = 2,  /*70d370*/
  BK_ENDING_PRIMARY = 3,  /*709ef8*/
  BK_ENDING_SECONDARY = 4 /*719b44*/
};
typedef struct {
  int32_t phase;                    /*721e00*/
  int32_t camera_mode, camera_clip; /*721e0c/721e08*/
  int32_t auxiliary_mode;           /*721ec8*/
  int32_t state_721ee0, state_721ee4;
  uint32_t
      camera_values[3]; /* raw float bits721e14..1c, copied not evaluated */
  uint32_t camera_table[5]
                       [4]; /* live709fcc, populated by actual event loader */
  int32_t camera_request, camera_cached, camera_event; /*722110/721ed0/7220e0*/
  uint32_t clock_sample, previous_clock,
      finish_elapsed;                               /*70d368/709fc4/719c50*/
  uint8_t group, finish_fade_stage, finish_blocked; /*721b3c/73aac4/beeb4c*/
  uint8_t transition_action, curtain_wanted, camera_manual; /*beeb7e/7f/719b9c*/
} BkEndingFrameState;
/* Captured by-value11-word controller input. Native parent initializes only
 * its pointer fields; caller must explicitly supply any other fields needed
 * by a recovered child. Dispatcher does not synthesize uninitialized data. */
typedef struct {
  uint32_t words[11];
} BkEndingFrameInput;
typedef struct {
  BkEndingOperation operation;
  int with_input;
  BkEndingFrameInput input;
  unsigned count;
  uint32_t args[6]; /* ordered scalar words and portable object tokens above */
} BkEndingCall;
typedef struct {
  void *context;
  /* Must implement the requested child or fail. State is shared and may
   * change in the call; branch decisions are made at the original points. */
  int (*invoke)(void *, const BkEndingCall *, uint32_t *native_result,
                char error[256]);
  int (*clock)(void *, uint32_t *milliseconds, char error[256]);
  int (*key)(void *, unsigned key, int *pressed, char error[256]);
} BkEndingFrameOps;
/* Owns branch scheduling, six immediate working-flag writes, end-wait timer
 * and camera dispatch. Working flags are the independent721dc6 copy; this
 * NEVER writes persistent progress. Eight flags are not implied by phase8.
 * Later callback failure retains earlier original side effects and ends the
 * frame. Does not implement child animation/media/UI/camera/resource stages. */
int bk_ending_frame_step(BkEndingFrameState *, uint8_t working[5][8],
                         const BkEndingFrameInput *, const BkEndingFrameOps *,
                         char error[256]);
#endif
