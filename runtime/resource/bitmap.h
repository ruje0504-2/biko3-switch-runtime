#ifndef BK_RESOURCE_BITMAP_H
#define BK_RESOURCE_BITMAP_H
#include "resource/store.h"
/* Canonical24bit bottom-up BMP of RGBA pixels; alpha discarded like49c7e3.
 * Supports aligned row padding and writes correct total bfSize (the original
 * writer stored pixel bytes only there). Pixels/2834 pixels-per-metre fields otherwise match
 * 49c4f3. Output must start empty; failure leaves it untouched. */
int bk_bitmap_encode(const BkImage *, BkBlob *, char error[256]);
#endif
