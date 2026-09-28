#ifndef BK_SCENE_NPC_MATERIAL_H
#define BK_SCENE_NPC_MATERIAL_H
#include "model/material_pose.h"
/* Adapt game fade policy to recursive model material edits. Actor alpha and
 * material batch commit together; original mark order is 02,01,00,03.
 * No GPU submission, visibility or animation occurs. */
int bk_npc_fade_apply(float *alpha, uint8_t fade_out, unsigned group,
                      float seconds, BkMaterialPose *materials, uint32_t root,
                      const uint32_t mark_frames[4], char error[256]);
#endif
