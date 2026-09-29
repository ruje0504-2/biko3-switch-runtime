#include "game/ending_tertiary.h"
#include "game/ending_normal.h"
#include <stdio.h>
#include <string.h>

const char *bk_ending_tertiary_material_name(BkEndingTertiaryMaterialTable table,
                                            unsigned group, unsigned slot) {
  static const char *const pair[5][2] = {
      {"", ""}, {"megane_nokori", "megane_renzu_nokori"},
      {"", ""}, {"", ""}, {"", ""}};
  static const char *const six[5][6] = {
      {"", "", "PB1_bura", "PA2_pantu", "", "PA1_pantu"},
      {"bura_nokosi", "bura_mof", "bura_tuujyou", "pantu_kiri",
       "pantu_kiri_nokosi", "pantu_nokosi"},
      {"karada_ue", "", "", "karada_sita", "", ""},
      {"PB2_skart", "", "PB1_skart", "PA2_pantu", "", "PA1_pantu"},
      {"PB2_huda", "", "PB1_huda", "PA2_pantu", "", "PA1_pantu"}};
  static const char *const four[5][4] = {
      {"S_sobi2a", "S_sobi2b", "S_sobi1", ""},
      {"S_sobi2a", "S_sobi2b", "S_sobi3a", "S_sobi3b"},
      {"S_sobi2a", "S_sobi2b", "S_sobi5a", "S_sobi5b"},
      {"S_sobi2a", "S_sobi2b", "S_sobi1", ""},
      {"S_sobi2a", "S_sobi2b", "S_sobi4", ""}};
  if (group >= 5) return NULL;
  switch (table) {
  case BK_ENDING_TERTIARY_MATERIAL_PAIR: return slot < 2 ? pair[group][slot] : NULL;
  case BK_ENDING_TERTIARY_MATERIAL_SIX: return slot < 6 ? six[group][slot] : NULL;
  case BK_ENDING_TERTIARY_MATERIAL_FOUR: return slot < 4 ? four[group][slot] : NULL;
  default: return NULL;
  }
}
const int32_t *bk_ending_tertiary_initial_targets(void) {
  static const int32_t targets[5] = {17, 32, 17, 17, 32};
  return targets;
}

int bk_ending_tertiary_config(BkEndingTertiaryConfig *out, unsigned group,
                              unsigned variant) {
  BkEndingNormalConfig normal;
  if (!out || !bk_ending_normal_config(&normal, group, variant)) return 0;
  BkEndingTertiaryConfig next = {0};
  snprintf(next.primary, sizeof(next.primary), "h%02u_10.xan", group + 1);
  snprintf(next.face, sizeof(next.face), "h%02u_10.fam", group + 1);
  if (group == 0) next.yaw = next.camera_yaw = 90;
  if (group == 2) {
    snprintf(next.auxiliaries[0], sizeof(next.auxiliaries[0]), "h03_30.xan");
    snprintf(next.auxiliaries[1], sizeof(next.auxiliaries[1]), "h03_31.xan");
    snprintf(next.visible_nodes[0], sizeof(next.visible_nodes[0]), "otoko");
  }
  next.expression_a = group < 2 ? 4 : 7;
  next.expression_b = group < 2 ? 4 : group == 3 ? 3 : 2;
  next.expression_mode = group == 3;
  /*4d4823 is the shared56f7f4 action-table copy;571148 is twenty2.f words. */
  memcpy(next.actions, normal.actions, sizeof(next.actions));
  for (unsigned i = 0; i < 5; ++i)
    for (unsigned j = 0; j < 4; ++j)
      next.camera_table[i][j] = UINT32_C(0x40000000);
  *out = next;
  return 1;
}
