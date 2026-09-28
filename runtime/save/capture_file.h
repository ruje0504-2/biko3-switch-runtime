#ifndef BK_SAVE_CAPTURE_FILE_H
#define BK_SAVE_CAPTURE_FILE_H
#include "resource/store.h"
typedef struct BkCaptureFiles BkCaptureFiles;
typedef struct {
  unsigned year, month, day, hour, minute, second;
  uint32_t ticks_ms;
} BkCaptureTime;
/* Output root is owned port data, never the read-only original game root.
 * Create root and album child, parent must already exist. Single-threaded.
 * This owns screenshot files only; it is not a game save-state format. */
BkCaptureFiles *bk_capture_files_create(const char *output_root,
                                        char error[256]);
void bk_capture_files_destroy(BkCaptureFiles *);
/* Native49cf48 stamp and49c7e3 group prefix. Clock supplied at capture time,
 * including the signed low32-bit tick remainder from original timeGetTime. */
int bk_capture_photo_name(char out[128], unsigned album_group,
                          const BkCaptureTime *);
/* Atomic replace in the same directory after successful close. Pause writes
 * root/sy_99.bmp; photos write album/name. Old file survives write failure. */
int bk_capture_file_write(BkCaptureFiles *, int photo, const char *name,
                          const BkBlob *, char error[256]);
/* Read only root/sy_99.bmp with an explicit allocation bound. Output must be
 * empty and remains empty on missing/error. Missing is distinct from corrupt
 * or oversized input; decoding belongs to the caller. */
BkResourceResult bk_capture_file_read_pause(BkCaptureFiles *, size_t limit,
                                            BkBlob *, char error[256]);
/* Delete only the pause capture; absent is already removed. Does not remove
 * directories or touch the photo album. */
int bk_capture_file_remove_pause(BkCaptureFiles *, char error[256]);
#endif
