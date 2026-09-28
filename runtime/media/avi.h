#ifndef BK_MEDIA_AVI_H
#define BK_MEDIA_AVI_H
#include <stddef.h>
#include <stdint.h>
typedef struct BkAvi BkAvi;
typedef struct BkAviDecoder BkAviDecoder;
typedef struct {
  uint32_t width, height, frames, scale, rate;
} BkAviInfo;
/* Owned RIFF AVI: one video stream, MS Video 1 / CRAM RGB555, positive
 * dimensions divisible by four, start0, legacy idx1. Other codecs, audio,
 * palettes and OpenDML are explicit failures. The PP entry may have padding
 * beyond the RIFF extent. Chunk padding and index offsets are validated.
 * Limits:256MiB RIFF,2048x2048,65536 frames. Input may be freed after open.
 * Parsing validates structure; compressed packet errors fail on decoding. */
BkAvi *bk_avi_open(const void *bytes, size_t size, char error[256]);
void bk_avi_destroy(BkAvi *);
const BkAviInfo *bk_avi_info(const BkAvi *);
/* Decoder borrows its immutable AVI; destroy it before the AVI. Seeking
 * reconstructs delta frames from the preceding indexed key frame. Output is
 * tightly packed, TOP-DOWN host uint16 RGB555 with bit15 zero. This is a
 * codec image, not the original game's VFW-to-surface copy/layout policy.
 * Returned pixels remain valid until the next successful decode/destroy.
 * Failure preserves the previous frame/pixels; no partial frame publication.
 * Decoders are independent; each requires externally serialized calls. */
BkAviDecoder *bk_avi_decoder_create(const BkAvi *, char error[256]);
void bk_avi_decoder_destroy(BkAviDecoder *);
int bk_avi_decoder_frame(BkAviDecoder *, uint32_t frame, char error[256]);
const uint16_t *bk_avi_decoder_pixels(const BkAviDecoder *);
uint32_t
bk_avi_decoder_index(const BkAviDecoder *); /* UINT32_MAX until valid */
#endif
