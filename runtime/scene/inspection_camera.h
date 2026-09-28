#ifndef BK_INSPECTION_CAMERA_H
#define BK_INSPECTION_CAMERA_H
#include "core/input.h"
/* Development inspection camera, not the original game's camera controller. */
typedef struct {
  float eye[3], yaw, pitch;
} BkInspectionCamera;
void bk_inspection_reset(BkInspectionCamera *camera);
int bk_inspection_world(const BkInspectionCamera *camera, float world[16]);
int bk_inspection_step(BkInspectionCamera *camera, double seconds,
                       const BkInput *input, char error[256]);
#endif
