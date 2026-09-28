#include "scene/ending_ui_batch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkEndingUiBatch {
  BkCurtainRender *curtain;
  BkEndingUiRender *owners[4]; /*before base/stage, after base/stage*/
  BkEndingUiFrame groups[4];
  unsigned group[BK_ENDING_UI_DRAWS], index[BK_ENDING_UI_DRAWS];
  unsigned count, curtain_after;
  int begun, ready;
};
static int fail(char e[256], const char *why) {
  if (e)
    snprintf(e, 256, "ending UI batch: %s", why);
  return 0;
}
BkEndingUiBatch *bk_ending_ui_batch_create(BkCurtainRender *curtain,
                                           char e[256]) {
  if (!curtain) {
    fail(e, "missing common curtain");
    return NULL;
  }
  BkEndingUiBatch *b = calloc(1, sizeof(*b));
  if (!b) {
    fail(e, "allocation failed");
    return NULL;
  }
  b->curtain = curtain;
  return b;
}
void bk_ending_ui_batch_clear(BkEndingUiBatch *b) {
  if (!b)
    return;
  for (unsigned i = 0; i < 4; i++) {
    bk_ending_ui_render_destroy(b->owners[i]);
    b->owners[i] = NULL;
  }
  b->ready = b->begun = 0;
}
void bk_ending_ui_batch_destroy(BkEndingUiBatch *b) {
  bk_ending_ui_batch_clear(b);
  free(b);
}
static int retain(BkEndingUiRender *source[2], BkEndingUiRender *out[2],
                  char e[256]) {
  out[0] = out[1] = NULL;
  for (unsigned i = 0; i < 2; i++)
    if (source[i]) {
      out[i] = bk_ending_ui_render_retain(source[i]);
      if (!out[i]) {
        bk_ending_ui_render_destroy(out[0]);
        out[0] = NULL;
        return fail(e, "image reference limit");
      }
    }
  return 1;
}
int bk_ending_ui_batch_begin(BkEndingUiBatch *b, BkEndingUiRender *base,
                             BkEndingUiRender *stage, char e[256]) {
  if (!b || !base)
    return fail(e, "missing batch/base images");
  BkEndingUiRender *source[2] = {base, stage}, *next[2];
  if (!retain(source, next, e))
    return 0;
  bk_ending_ui_batch_clear(b);
  b->owners[0] = next[0];
  b->owners[1] = next[1];
  b->begun = 1;
  return 1;
}
int bk_ending_ui_batch_prepare(BkEndingUiBatch *b,
                               const BkEndingUiCompositeFrame *f,
                               BkEndingUiRender *base, BkEndingUiRender *stage,
                               unsigned width, unsigned height, char e[256]) {
  if (b)
    b->ready = 0;
  if (!b || !b->begun || !f || !f->complete ||
      f->sprites.count > BK_ENDING_UI_DRAWS ||
      f->curtain_after > f->sprites.count ||
      (f->early_return && f->curtain_after != f->sprites.count))
    return fail(e, "invalid or incomplete CPU frame");
  BkEndingUiRender *source[2] = {base, stage}, *next[2];
  if (!retain(source, next, e))
    return 0;
  for (unsigned i = 0; i < 2; i++) {
    bk_ending_ui_render_destroy(b->owners[2 + i]);
    b->owners[2 + i] = next[i];
  }
  memset(b->groups, 0, sizeof(b->groups));
  for (unsigned i = 0; i < f->sprites.count; i++) {
    const BkEndingUiDraw *d = &f->sprites.draws[i];
    unsigned owner =
        (i < f->curtain_after ? 0 : 2) + (d->slot == 8 || d->slot >= 63);
    if (!b->owners[owner])
      return fail(e, "missing captured image owner");
    for (unsigned j = 0; j < owner; j++)
      if (b->owners[j] == b->owners[owner]) {
        owner = j;
        break;
      }
    b->group[i] = owner;
    b->index[i] = b->groups[owner].count;
    b->groups[owner].draws[b->groups[owner].count++] = *d;
  }
  for (unsigned i = 0; i < 4; i++)
    if (b->groups[i].count)
      if (!bk_ending_ui_render_prepare(b->owners[i], &b->groups[i], width,
                                       height, e))
        return 0;
  if (!bk_curtain_render_prepare(b->curtain, &f->curtain, width, height, e))
    return 0;
  b->count = f->sprites.count;
  b->curtain_after = f->curtain_after;
  b->ready = 1;
  return 1;
}
int bk_ending_ui_batch_draw(BkEndingUiBatch *b, char e[256]) {
  if (!b || !b->ready)
    return fail(e, "no prepared batch");
  for (unsigned i = 0; i <= b->count; i++) {
    if (i == b->curtain_after && !bk_curtain_render_draw(b->curtain, e))
      return 0;
    if (i < b->count && !bk_ending_ui_render_draw_range(b->owners[b->group[i]],
                                                        b->index[i], 1, e))
      return 0;
  }
  return 1;
}
