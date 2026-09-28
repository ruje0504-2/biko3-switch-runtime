#include "scene/npc_material.h"
#include "game/npc_fade.h"
#include <stdio.h>
int bk_npc_fade_apply(float *alpha, uint8_t fade_out, unsigned group,
                      float seconds, BkMaterialPose *materials, uint32_t root,
                      const uint32_t marks[4], char error[256]) {
  if (!alpha || !materials || !marks) {
    snprintf(error, 256, "NPC material: missing instance or binding");
    return 0;
  }
  BkNpcFade next;
  if (!bk_npc_fade_plan(&next, *alpha, fade_out, group, seconds, error))
    return 0;
  if (!next.apply)
    return 1;
  BkMaterialAlphaRule rules[3];
  for (unsigned i = 0; i < next.rule_count; i++)
    rules[i] = (BkMaterialAlphaRule){next.rules[i].name, next.rules[i].alpha};
  BkMaterialAlphaEdit edits[5] = {{root, next.alpha, rules, next.rule_count}};
  if (next.restore_marks)
    for (unsigned i = 0; i < 4; i++)
      edits[i + 1] = (BkMaterialAlphaEdit){marks[i], 2, NULL, 0};
  if (!bk_material_pose_alpha(materials, edits, next.restore_marks ? 5 : 1,
                               error))
    return 0;
  *alpha = next.alpha;
  return 1;
}
