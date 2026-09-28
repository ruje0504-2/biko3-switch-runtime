#ifndef BK_GAME_DIALOGUE_ENTRY_H
#define BK_GAME_DIALOGUE_ENTRY_H
#include <stdint.h>
typedef struct {
  char file[16], font[16];
  int32_t first, last;
} BkDialogueEntry;
/*4efdfb..4f0955. Packed retail script/range selection; independent of IO.
 * previous38 starts the selected actor's introduction. previous2/16 use
 * area/response, not the result-gallery choice. Other previous values use
 * the native group1 default. Reject native uninitialized branches atomically.
 */
int bk_dialogue_entry_select(BkDialogueEntry *, uint8_t previous, int32_t group,
                             int32_t area, uint8_t response, char error[256]);
#endif
