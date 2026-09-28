#include "scene/ending_stage_ui.h"
#include "game/ending_reload.h"
#include <stdio.h>
#include <string.h>
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "ending stage UI: %s", why);
  return 0;
}
/* Resource names copied from the fixed EXE tables; no filename inference. */
static const char *const secondary[5][1][2] = {
    {
        {"hs_59.tga", "hs_73.tga"},
    },
    {
        {"hs_67.tga", "hs_73.tga"},
    },
    {
        {"hs_59.tga", "hs_73.tga"},
    },
    {
        {"hs_59.tga", "hs_73.tga"},
    },
    {
        {"hs_59.tga", "hs_73.tga"},
    },
};
static const char *const auxiliary[5][2][3] = {
    {
        {"hs_51.tga", "hs_54.tga", "hs_57.tga"},
        {"hs_51.tga", "hs_54.tga", "hs_57.tga"},
    },
    {
        {"hs_51.tga", "hs_54.tga", "hs_57.tga"},
        {"hs_51.tga", "hs_63.tga", "hs_54.tga"},
    },
    {
        {"hs_95.tga", "hs_54.tga", "hs_57.tga"},
        {"hs_88.tga", "hs_56.tga", "hs_57.tga"},
    },
    {
        {"hs_96.tga", "hs_54.tga", "hs_57.tga"},
        {"hs_95.tga", "hs_54.tga", "hs_57.tga"},
    },
    {
        {"hs_51.tga", "hs_54.tga", "hs_57.tga"},
        {"hs_88.tga", "hs_87.tga", "hs_81.tga"},
    },
};
static const char *const third[5][1][5] = {
    {
        {"hs_81.tga", "hs_78.tga", "hs_79.tga", "hs_80.tga", "hi_06.tga"},
    },
    {
        {"hs_52.tga", "hs_78.tga", "hs_79.tga", "hs_83.tga", "hi_09.tga"},
    },
    {
        {"hs_70.tga", "hs_89.tga", "hs_91.tga", "hs_90.tga", "hi_05.tga"},
    },
    {
        {"hs_70.tga", "hs_78.tga", "hs_92.tga", "hs_80.tga", "hi_08.tga"},
    },
    {
        {"hs_65.tga", "hs_84.tga", "hs_86.tga", "hs_85.tga", "hi_07.tga"},
    },
};
static const char *const fourth[5][1][1] = {
    {
        {"hs_65.tga"},
    },
    {
        {"hs_82.tga"},
    },
    {
        {"hs_71.tga"},
    },
    {
        {"hs_94.tga"},
    },
    {
        {"hs_93.tga"},
    },
};
static const unsigned slots[6][9] = {{0},
                                     {63, 64, 65, 66, 67, 68, 69},
                                     {67, 70, 71, 72, 73, 63, 64, 65, 66},
                                     {63, 64, 65, 66, 67, 8},
                                     {63, 64},
                                     {74}};
static const unsigned counts[6] = {0, 7, 9, 6, 2, 1};
static unsigned index_for(unsigned slot) {
  return slot == 8 ? 0 : slot >= 63 && slot < 75 ? slot - 62 : 13;
}
static const char *name_for(BkEndingUiStageKind kind, unsigned g, unsigned v,
                            unsigned slot) {
  if (slot == 63)
    return "hs_50.tga";
  switch (kind) {
  case BK_ENDING_UI_SECONDARY:
    if (slot <= 65)
      return secondary[g][0][slot - 64];
    return (const char *const[]){"hs_72.tga", "hs_74.tga", "hs_76.tga",
                                 "hs_75.tga"}[slot - 66];
  case BK_ENDING_UI_AUXILIARY:
    if (slot <= 66)
      return auxiliary[g][v][slot - 64];
    if (slot == 67)
      return "hs_77.tga";
    return (const char *const[]){"hs_105.tga", "hs_106.tga", "hs_107.tga",
                                 "hs_108.tga"}[slot - 70];
  case BK_ENDING_UI_THIRD:
    return third[g][0][slot == 8 ? 4 : slot - 64];
  case BK_ENDING_UI_FOURTH:
    return fourth[g][0][0];
  case BK_ENDING_UI_FINAL:
    return bk_ending_final_image(g);
  default:
    return NULL;
  }
}
const char *bk_ending_stage_ui_image(const BkEndingStageUi *s, unsigned slot) {
  unsigned i = index_for(slot);
  return s && i < 13 && (s->loaded & (1u << i)) ? s->images[i] : NULL;
}
int bk_ending_stage_ui_initialize(BkEndingUi *base, BkEndingStageUi *s,
                                  BkEndingUiStageKind kind, unsigned group,
                                  int32_t variant, unsigned width,
                                  char e[256]) {
  if (!base || !s || (unsigned)kind > BK_ENDING_UI_FINAL || group >= 5 ||
      !width || width > 16384 ||
      (kind == BK_ENDING_UI_AUXILIARY && (variant < 0 || variant > 1)))
    return fail(e, "invalid initializer");
  BkEndingStageUi next = *s;
  BkEndingUiSprite cursor = base->sprites[8];
  float scale = (float)((double)width / 1280);
  for (unsigned k = 0; k < counts[kind]; ++k) {
    unsigned slot = slots[kind][k], i = index_for(slot);
    BkEndingUiSprite *p = slot == 8 ? &cursor : &next.sprites[slot - 63];
    float r[4] = {640, 480, 200, 40};
    uint8_t enter = 1, exit = 1, idle = 0, wanted = 1;
    float px = 0, py = 0;
    if (slot == 63) {
      r[2] = 48;
      r[3] = 24;
      px = 1;
      py = .5f;
    } else if (slot == 8) {
      r[2] = r[3] = 96;
      px = py = .5f;
      wanted = 0;
    } else if (slot == 70)
      memcpy(r, (float[4]){800, 896, 480, 64}, sizeof(r));
    else if (slot == 71)
      memcpy(r, (float[4]){640, 905, 48, 48}, sizeof(r));
    else if (slot == 72) {
      memcpy(r, (float[4]){1136, 788, 128, 128}, sizeof(r));
      enter = 2;
      exit = 3;
      idle = 5;
      px = py = .5f;
      wanted = 0;
    } else if (slot == 73) {
      memcpy(r, (float[4]){1200, 852, 128, 128}, sizeof(r));
      enter = 3;
      exit = 2;
      idle = 5;
      px = py = .5f;
      wanted = 0;
    } else if (slot == 74)
      memcpy(r, (float[4]){0, 0, 1280, 960}, sizeof(r));
    for (unsigned j = 0; j < 4; j++)
      p->rect[j] = r[j] * scale;
    if (!bk_effect_sprite_initialize(&p->transform, p->rect, enter, exit,
                                     idle) ||
        !bk_fade_sprite_request(&p->transform.fade, wanted))
      return fail(e, "invalid constructor");
    p->transform.pivot[0] = px;
    p->transform.pivot[1] = py;
    memcpy(p->uv, (float[4]){0, 0, 1, 1}, sizeof(p->uv));
    p->rgb = 0xffffff;
    next.images[i] = name_for(kind, group, (unsigned)variant, slot);
    next.loaded |= (uint16_t)(1u << i);
  }
  *s = next;
  if (kind == BK_ENDING_UI_THIRD) {
    base->sprites[8] = cursor;
    base->loaded |= UINT64_C(1) << 8;
  }
  return 1;
}
int bk_ending_stage_ui_release(BkEndingUi *base, BkEndingStageUi *s,
                               BkEndingUiStageKind kind, char e[256]) {
  if (!base || !s || (unsigned)kind > BK_ENDING_UI_FINAL)
    return fail(e, "invalid release");
  for (unsigned k = 0; k < counts[kind]; k++) {
    unsigned slot = slots[kind][k];
    s->loaded &= (uint16_t)~(1u << index_for(slot));
    if (slot == 8)
      base->loaded &= ~(UINT64_C(1) << 8);
  }
  return 1;
}
int bk_ending_stage_ui_sprite_step(BkEndingUi *base, BkEndingStageUi *s,
                                   unsigned slot, float seconds,
                                   BkEndingUiDraw *out, char e[256]) {
  if (!base || !bk_ending_stage_ui_image(s, slot))
    return fail(e, "unconstructed sprite");
  if (slot == 8)
    return bk_ending_ui_sprite_step(base, slot, seconds, out, e);
  return bk_ending_ui_element_step(&s->sprites[slot - 63], slot, seconds, out,
                                   e);
}

int bk_ending_ui_dispatch_sprite(BkEndingUi *base, BkEndingStageUi *s,
                                 unsigned slot, float seconds,
                                 BkEndingUiFrame *frame, char e[256]) {
  if (!base || !s || !frame || slot >= BK_ENDING_UI_TOTAL_SPRITES ||
      frame->count > BK_ENDING_UI_DRAWS)
    return fail(e, "invalid dispatch");
  BkEndingUiSprite *p = slot < BK_ENDING_UI_SPRITES ? &base->sprites[slot]
                                                    : &s->sprites[slot - 63];
  int image = slot < 63 ? (base->loaded & (UINT64_C(1) << slot)) != 0
                        : (s->loaded & (1u << (slot - 62))) != 0;
  if (slot == 8 && image != ((s->loaded & 1) != 0))
    return fail(e, "inconsistent shared cursor owner");
  if (image && (slot == 8 || slot >= 63) && !bk_ending_stage_ui_image(s, slot))
    return fail(e, "missing stage image identity");
  if (!image) {
    if (!bk_effect_sprite_advance_detached(&p->transform, p->rect, seconds))
      return fail(e, "invalid detached animation");
    return 1;
  }
  if (frame->count == BK_ENDING_UI_DRAWS)
    return fail(e, "draw capacity exceeded");
  BkEndingUiDraw draw;
  if (!bk_ending_ui_element_step(p, slot, seconds, &draw, e))
    return 0;
  frame->draws[frame->count++] = draw;
  return 1;
}
