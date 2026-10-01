#include "scene/dialogue_ui_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkDialogueUiRender {
  BkRenderer *renderer;
  BkTexture *textures[2];
  BkGpuMesh *meshes[2];
  BkTextRender *text;
  unsigned width, height;
  int text_ready, ready;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "dialogue UI render: %s", why);
  return 0;
}
void bk_dialogue_ui_render_destroy(BkDialogueUiRender *r) {
  if (!r)
    return;
  bk_text_render_destroy(r->text);
  for (unsigned i = 0; i < 2; ++i) {
    bk_mesh_destroy(r->renderer, r->meshes[i]);
    bk_texture_destroy(r->renderer, r->textures[i]);
  }
  free(r);
}
BkDialogueUiRender *bk_dialogue_ui_render_create(BkRenderer *gpu,
                                                 BkResourceStore *store,
                                                 char e[256]) {
  if (!gpu || !store) {
    fail(e, "missing services");
    return NULL;
  }
  BkDialogueUiRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(e, "allocation failed");
    return NULL;
  }
  r->renderer = gpu;
  BkDialogueUiSprite sprite[2];
  bk_dialogue_ui_layout(sprite, 1280);
  const BkVertex v[4] = {{0, 0, 0, 0, 0, 1, 1, 1, 1},
                         {1, 0, 0, 1, 0, 1, 1, 1, 1},
                         {1, 1, 0, 1, 1, 1, 1, 1, 1},
                         {0, 1, 0, 0, 1, 1, 1, 1, 1}};
  const uint16_t ix[] = {0, 1, 2, 3, 0, 2};
  for (unsigned i = 0; i < 2; ++i) {
    BkBlob raw = {0};
    BkImage image = {0};
    BkResourceResult result =
        bk_resources_read(store, "bk3_00", sprite[i].image, &raw, e);
    if (result == BK_RESOURCE_MISSING)
      snprintf(e, 256, "dialogue UI render: missing %s", sprite[i].image);
    if (result == BK_RESOURCE_OK &&
        bk_image_decode(raw.data, raw.size, &image, e))
      r->textures[i] =
          bk_texture_create_sampled(gpu, &image, BK_WRAP_REPEAT, e);
    bk_blob_free(&raw);
    bk_image_free(&image);
    if (!r->textures[i] ||
        !(r->meshes[i] = bk_mesh_create(gpu, v, 4, ix, 6, e)))
      goto bad;
  }
  float zoom = (bk_resources_patch_flags(store) & BK_PATCH_CHINESE) ? 1.3f : 1;
  r->text = bk_text_render_create_scaled(gpu, store, "Type_S.FTT", 432, 64,
                                         zoom, e);
  if (!r->text)
    goto bad;
  return r;
bad:
  bk_dialogue_ui_render_destroy(r);
  return NULL;
}
int bk_dialogue_ui_render_text(BkDialogueUiRender *r, const BkMessage *m,
                               BkTextFlow *flow, float seconds, unsigned width,
                               unsigned height, char e[256]) {
  if (!r || !m || m->length > BK_MESSAGE_CAPACITY)
    return fail(e, "invalid text");
  r->ready = r->text_ready = 0;
  BkTextStyle style;
  bk_dialogue_ui_text_style(&style);
  if (!bk_text_render_prepare(r->text, &style, m->bytes, m->length, seconds,
                              width, height, flow, e))
    return 0;
  r->width = width;
  r->height = height;
  r->text_ready = 1;
  return 1;
}
int bk_dialogue_ui_render_prepare(BkDialogueUiRender *r,
                                  const BkDialogueUiFrame *f, unsigned width,
                                  unsigned height, char e[256]) {
  if (!r || !f || !r->text_ready || width != r->width || height != r->height ||
      !isfinite(f->panel_alpha) || f->panel_alpha < 0 || f->panel_alpha > 1 ||
      !isfinite(f->prompt_alpha) || f->prompt_alpha < 0 || f->prompt_alpha > 1)
    return fail(e, "invalid widgets/text snapshot");
  BkDialogueUiSprite sprite[2];
  if (!bk_dialogue_ui_layout(sprite, width))
    return fail(e, "invalid extent");
  r->ready = 0;
  for (unsigned i = 0; i < 2; ++i) {
    BkDialogueUiSprite *q = &sprite[i];
    float right = (float)((double)q->x + q->width),
          bottom = (float)((double)q->y + q->height);
    float a = (uint32_t)((double)(i ? f->prompt_alpha : f->panel_alpha) * 255) /
              255.f;
    const BkVertex v[4] = {{q->x, q->y, 0, 0, 0, 1, 1, 1, a},
                           {right, q->y, 0, 1, 0, 1, 1, 1, a},
                           {right, bottom, 0, 1, 1, 1, 1, 1, a},
                           {q->x, bottom, 0, 0, 1, 1, 1, 1, a}};
    if (!bk_mesh_update(r->renderer, r->meshes[i], v, 4, e))
      return 0;
  }
  r->ready = 1;
  return 1;
}
int bk_dialogue_ui_render_draw(BkDialogueUiRender *r, char e[256]) {
  if (!r || !r->ready)
    return fail(e, "missing prepared frame");
  float matrix[16];
  memcpy(matrix, bk_identity, sizeof(matrix));
  matrix[0] = 2.f / r->width;
  matrix[5] = 2.f / r->height;
  matrix[12] = (float)(1.0 / r->width - 1);
  matrix[13] = (float)(1.0 / r->height - 1);
  for (unsigned i = 0; i < 2; ++i)
    if (!bk_renderer_draw_mesh(
            r->renderer, r->textures[i], r->meshes[i], matrix,
            (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, e))
      return 0;
  return bk_text_render_draw(r->text, e);
}
const BkImage *bk_dialogue_ui_render_text_image(const BkDialogueUiRender *r) {
  return r ? bk_text_render_image(r->text) : NULL;
}
const BkTextDraw *
bk_dialogue_ui_render_text_snapshot(const BkDialogueUiRender *r) {
  return r ? bk_text_render_snapshot(r->text) : NULL;
}
