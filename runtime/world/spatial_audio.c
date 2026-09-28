#include "world/spatial_audio.h"
#include "world/placement.h"
#include <limits.h>
#include <math.h>
int bk_spatial_audio(BkSpatialAudio *out, const float source[3],
                     const float listener[3], float listener_yaw,
                     int32_t master_volume, float attenuation) {
  if (!out || !source || !listener || !isfinite(listener_yaw) ||
      !isfinite(attenuation))
    return 0;
  double distance2 = 0;
  for (unsigned i = 0; i < 3; i++) {
    if (!isfinite(source[i]) || !isfinite(listener[i]))
      return 0;
    double d = (double)listener[i] - source[i];
    distance2 += d * d;
  }
  float bearing;
  if (!bk_route_heading(&bearing, listener[0], listener[2], source[0],
                        source[2]))
    return 0;
  float distance = (float)sqrt(distance2);
  float yaw = (float)((double)listener_yaw - 180.f);
  /* fst stores yaw difference but keeps its extended value for the multiply. */
  double difference = (double)bearing - 180.f - yaw;
  float sine = (float)sin(difference * 0.01745329238474369f);
  double volume = (double)master_volume - (double)distance * attenuation;
  if (!isfinite(volume) || volume >= 2147483648.0 || volume < INT32_MIN)
    return 0;
  int32_t v = (int32_t)volume;
  *out = (BkSpatialAudio){v <= -6000 ? -6000 : v,
                          (int32_t)((double)sine * 1500.f)};
  return 1;
}
