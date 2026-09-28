#ifndef BK_MEDIA_AVI_SURFACE_H
#define BK_MEDIA_AVI_SURFACE_H
#include <stddef.h>
#include <stdint.h>
/* Portable521ed2 surface policy: synthesize a compact positive-height RGB555
 * DIB (40-byte header), preserve native source+44 and no vertical flip. Thus
 * surface row0 reads the bottom codec row starting two pixels in, including
 * row spill. The final two out-of-DIB reads are explicitly black; native heap
 * tail values are undefined. Initial GPU contents are a separate owner policy.
 * green565 preserves the original optional RGB555->565 shift (green LSB0).
 * Output is opaque RGBA8 normalized from the chosen555/565 surface format.
 * Input is the TOP-DOWN canonical decoder image. Buffers must not overlap;
 * invalid dimensions/pointers/size preserve output. No GPU/media-clock use.
 * This explicit DIB/default-format policy is not a Windows VFW layout proof. */
int bk_avi_surface_rgba(const uint16_t *top_down, uint32_t width,
                        uint32_t height, int green565, uint8_t *rgba,
                        size_t bytes, char error[256]);
#endif
