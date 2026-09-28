#ifndef BK_RESOURCE_DIALOGUE_H
#define BK_RESOURCE_DIALOGUE_H
#include "resource/message.h"
typedef struct {
  uint32_t cursor;
  int32_t first_label, last_label, current_label;
  char filename[256];
  BkMessage text;
  /* Literal #C/#F/#E/#M values; consumers interpret the selected scene. */
  int32_t code_c, code_f, code_e, code_m;
  char previous_sound[256], sound[256], image[256], music[256];
  uint8_t sound_pending, image_kind, music_pending;
} BkDialogue;
/*51765d's metadata/reset/initial-label scan over already loaded bytes.
 * Owner sets first/last_label as517640, retains current_label and unassigned
 * metadata. Source bytes remain externally owned; next receives the same
 * immutable blob. File I/O/allocation and media submission are separate.
 * A missing first label reaching #end resets cursor0, as native. Raw-byte
 * strings, case-sensitive directives, no Unicode recoding. Atomic failure. */
int bk_dialogue_open(BkDialogue *, const void *raw, size_t size,
                     const char *filename, char error[256]);
/*517c8e. Equality current==last clears text/previous_sound and returns done1.
 * Otherwise seek numeric header, consume ordered metadata, extract body.
 * Quoted body leaves cursor at body START; '#' body moves cursor to that '#'.
 * Pending media fields persist until their consumer acknowledges them.
 * Malformed/truncated/oversized scans fail atomically instead of native OOB.
 * done is the original AL completion, not whether a message was produced. */
int bk_dialogue_next(BkDialogue *, const void *raw, size_t size, int *done,
                     char error[256]);
/*517ba9 metadata only. Raw storage and any sound handle belong to the owner.
 * Retains CR count, C/F/E/M, media names and pending flags. */
void bk_dialogue_close(BkDialogue *);
#endif
