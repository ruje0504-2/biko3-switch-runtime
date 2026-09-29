#include "game/ending_presentation.h"
#include <math.h>
#include <stdio.h>
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending presentation: %s", why);
  return 0;
}
const char *bk_ending_presentation_material(unsigned group, unsigned index) {
  /* Fixed-EXE56e368, five groups of four260-byte names; empty is queried. */
  static const char *const names[5][4] = {
      {"S_sobi2a", "S_sobi2b", "S_sobi1", ""},
      {"S_sobi2a", "S_sobi2b", "S_sobi3a", "S_sobi3b"},
      {"S_sobi2a", "S_sobi2b", "S_sobi5a", "S_sobi5b"},
      {"S_sobi2a", "S_sobi2b", "S_sobi1", ""},
      {"S_sobi2a", "S_sobi2b", "S_sobi4", ""}};
  return group < 5 && index < 4 ? names[group][index] : NULL;
}
int bk_ending_presentation_step(const BkEndingPresentationBindings *b,
                                 const BkEndingFrameInput *in, float seconds,
                                 const BkEndingPresentationOps *o,
                                 char e[256]) {
  if (!b || !b->frame || !b->auxiliary || !b->normal_ready || !b->toggles ||
      !b->primary_root || !b->background_root || !b->secondary ||
      !b->direct_node || !b->direct_reference || !b->flip || !in || !o ||
      !isfinite(seconds) || seconds < 0)
    return fail(e, "invalid live bindings or time");
#define CALL(member, ...)                                                      \
  do {                                                                         \
    if (!o->member)                                                            \
      return fail(e, "missing " #member " service");                          \
    if (!o->member(o->context, __VA_ARGS__))                                    \
      return 0;                                                                \
  } while (0)
  uint32_t timestamp = 0;
  CALL(clock, &timestamp, e);
  CALL(advance, BK_ENDING_PRESENT_BACKGROUND, seconds, e);
  if (b->frame->group == 2 && !b->auxiliary->variant) {
    uint32_t node = 0;
    CALL(find, *b->primary_root, "OYU", &node, e);
    CALL(hide, node, b->toggles[0], e);
  }
  CALL(hide, *b->background_root, b->toggles[0], e);
  for (unsigned i = 0; i < 4; ++i) {
    const char *name = bk_ending_presentation_material(b->frame->group, i);
    if (!name)
      return fail(e, "material group outside native table");
    CALL(material, name, b->toggles[i < 2 ? 3 : 5], i == 1 ? .2f : 1.f, e);
  }
  if (b->frame->state_721ee0 == 3) {
    if (b->frame->camera_cached == 11) {
      CALL(disable_bom, 0, 0, e);
    } else if (b->frame->camera_cached == 12) {
      CALL(disable_bom, 0, 0, e);
      CALL(disable_bom, 1, 0, e);
    } else if (b->frame->group == 1 && b->frame->camera_cached == 10) {
      CALL(disable_bom, 1, 0, e);
    }
  } else {
    CALL(disable_bom, 0, 1, e);
    CALL(disable_bom, 1, 1, e);
  }
  float rate = *b->normal_ready == 6 ? .08f
                 : b->frame->group == 1 ? .6f : .5f;
  CALL(advance, BK_ENDING_PRESENT_PRIMARY,
       (float)((double)seconds * (double)rate), e);
  if (b->frame->state_721ee0 == 3 && *b->secondary)
    CALL(advance, BK_ENDING_PRESENT_SECONDARY,
         (float)((double)seconds * .5), e);
  CALL(publish, e);
  if (*b->secondary && b->frame->phase == 1) {
    if (b->frame->state_721ee0 == 3 && *b->normal_ready == 1 &&
        b->frame->phase == 1) {
      if (b->frame->camera_cached == 11 || b->frame->camera_cached == 12)
        CALL(manual, 0, in, 0, 0, *b->flip, e);
      if (b->frame->camera_cached == 12)
        CALL(manual, 1, in, 0, 0, *b->flip, e);
      if ((b->frame->camera_cached == 9 || b->frame->camera_cached == 10 ||
           b->frame->camera_cached == 26 || b->frame->camera_cached == 27) &&
          (*b->direct_node || *b->direct_reference))
        CALL(manual, 2, in, *b->direct_reference, *b->direct_node, *b->flip, e);
    }
    CALL(follow, e);
  }
  CALL(publish, e);
  CALL(face, BK_ENDING_PRESENT_EYE_RANGE,
       (float)b->auxiliary->expression_a, 0, 0, e);
  CALL(gaze, e);
  CALL(face, BK_ENDING_PRESENT_EXPRESSION, 0, b->auxiliary->expression_b, 0, e);
  CALL(face, BK_ENDING_PRESENT_BLINK, 0, 0, timestamp, e);
  float level = 0;
  CALL(level, &level, e);
  CALL(face, BK_ENDING_PRESENT_MOUTH, level, 0, timestamp, e);
#undef CALL
  return 1;
}
