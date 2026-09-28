#ifndef BK_SCENE_SAVE_MENU_LABELS_H
#define BK_SCENE_SAVE_MENU_LABELS_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
  uint32_t area;
  char stamp[32];
} BkSaveMenuLabel;
/*50a2ae's row policy AFTER bank read, localized to Japanese Shift-JIS.
 * Ten concatenated fixed-width rows, エリア and fullwidth digits/punctuation
 * matching original Type_G.FTT. Occupied rows show YY/MM/DD-HH:MM, not seconds.
 * Unsupported timestamp characters are skipped just as the original loop.
 * out is512 bytes; size excludes NUL. Failure leaves both outputs unchanged.*/
int bk_save_menu_labels(const BkSaveMenuLabel records[10], uint8_t out[512],
                        size_t *size, char error[256]);
#endif
