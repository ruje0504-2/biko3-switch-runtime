#ifndef BK_CORE_AUDIO_VOLUME_H
#define BK_CORE_AUDIO_VOLUME_H
#include <stdint.h>
enum { BK_VOLUME_VOICE, BK_VOLUME_MUSIC, BK_VOLUME_EFFECT, BK_VOLUME_COUNT };
/* Borrowed process values in native hundredths of dB. NULL preserves explicit
 * standalone diagnostic gains; the production app always supplies all three. */
static inline int32_t bk_volume_get(const int32_t *values, unsigned kind,
                                    int32_t diagnostic_value) {
  return values ? values[kind] : diagnostic_value;
}
#endif
