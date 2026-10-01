#include "scene/item_notice_render.h"
#include <stdlib.h>
#include <string.h>
struct BkItemNoticeRender {
  BkRenderer *renderer;
  BkResourceStore *store;
  BkNoticeTextKind text_kind;
  unsigned draw_mask;
  BkTexture *textures[7];
  BkGpuMesh *meshes[7];
  BkTextRender *text;
  const BkMessage *message;
  BkMessage empty;
  BkItemNoticeFrame frame;
  unsigned group, width, height;
  int ready;
};
static int fail(char *error, const char *why) {
  snprintf(error, 256, "item notice render: %s", why);
  return 0;
}
void bk_item_notice_render_destroy(BkItemNoticeRender *n) {
  if (!n)
    return;
  bk_text_render_destroy(n->text);
  for (unsigned i = 0; i < 7; ++i) {
    bk_mesh_destroy(n->renderer, n->meshes[i]);
    bk_texture_destroy(n->renderer, n->textures[i]);
  }
  free(n);
}
int bk_item_notice_render_reload(BkItemNoticeRender *n, BkResourceStore *store,
                                 unsigned group, int retain_icons,
                                 char error[256]) {
  BkItemNoticeSprite layout[7];
  if (!n || !store || !bk_item_notice_layout(layout, group, 1280))
    return fail(error, "invalid services/profile");
  BkTexture *textures[7] = {0};
  BkBlob blob = {0};
  BkImage image = {0};
  layout[6] = (BkItemNoticeSprite){"ma_05.tga", 1097, 890, 40, 40};
  unsigned count = 7;
  for (unsigned i = 0; i < count; ++i) {
    if (retain_icons && i > 0 && i < 6)
      continue;
    BkResourceResult result =
        bk_resources_read(store, "bk3_00", layout[i].name, &blob, error);
    if (result != BK_RESOURCE_OK) {
      if (result == BK_RESOURCE_MISSING)
        fail(error, "missing notice image");
      goto bad;
    }
    if (!bk_image_decode(blob.data, blob.size, &image, error))
      goto bad;
    if (image.width != (unsigned)layout[i].width ||
        image.height != (unsigned)layout[i].height) {
      fail(error, "changed authored notice image extent");
      goto bad;
    }
    textures[i] = bk_texture_create(n->renderer, &image, error);
    bk_blob_free(&blob);
    bk_image_free(&image);
    if (!textures[i])
      goto bad;
  }
  for (unsigned i = 0; i < count; ++i) {
    if (retain_icons && i > 0 && i < 6)
      continue;
    bk_texture_destroy(n->renderer, n->textures[i]);
    n->textures[i] = textures[i];
  }
  if (!retain_icons)
    n->group = group;
  n->store = store;
  n->ready = 0;
  return 1;
bad:
  bk_blob_free(&blob);
  bk_image_free(&image);
  for (unsigned i = 0; i < 7; ++i)
    bk_texture_destroy(n->renderer, textures[i]);
  return 0;
}
BkItemNoticeRender *bk_item_notice_render_create(BkRenderer *renderer,
                                                 BkResourceStore *store,
                                                 unsigned group,
                                                 char error[256]) {
  if (!renderer || !store || group >= 5) {
    fail(error, "invalid services/profile");
    return NULL;
  }
  BkItemNoticeRender *n = calloc(1, sizeof(*n));
  if (!n) {
    fail(error, "allocation failed");
    return NULL;
  }
  n->renderer = renderer;
  n->text_kind = BK_NOTICE_TEXT_ITEMS;
  n->message = &n->empty;
  if (!bk_item_notice_render_reload(n, store, group, 0, error))
    goto bad;
  float zoom = (bk_resources_patch_flags(store) & BK_PATCH_CHINESE) ? 1.3f : 1;
  n->text = bk_text_render_create_scaled(renderer, store, "Type_S.FTT", 316,
                                          268, zoom, error);
  if (!n->text)
    goto bad;
  const BkVertex v[4] = {{0, 0, 0, 0, 0, 1, 1, 1, 1},
                         {1, 0, 0, 1, 0, 1, 1, 1, 1},
                         {1, 1, 0, 1, 1, 1, 1, 1, 1},
                         {0, 1, 0, 0, 1, 1, 1, 1, 1}};
  const uint16_t indices[] = {0, 1, 2, 3, 0, 2};
  for (unsigned i = 0; i < 7; ++i)
    if (!(n->meshes[i] = bk_mesh_create(renderer, v, 4, indices, 6, error)))
      goto bad;
  return n;
bad:
  bk_item_notice_render_destroy(n);
  return NULL;
}
static int prepare(BkItemNoticeRender *n, const BkItemNoticeState *s,
                   const BkPulseSprite *prompt, BkTextFlow *flow, float seconds,
                   unsigned width, unsigned height, char error[256]) {
  n->ready = 0;
  unsigned target_width, target_height;
  bk_renderer_extent(n->renderer, &target_width, &target_height);
  BkItemNoticeSprite layout[7];
  if (!height || height > target_height || width > target_width ||
      !bk_item_notice_layout(layout, n->group, width))
    return fail(error, "invalid viewport extent");
  float scale = (float)((double)width / 1280);
  layout[6] = (BkItemNoticeSprite){"ma_05.tga", 1097 * scale, 890 * scale,
                                   40 * scale, 40 * scale};
  for (unsigned i = 0; i < 7; ++i) {
    if (!(n->draw_mask & (1u << i)))
      continue;
    const BkItemNoticeSprite *p = &layout[i];
    const BkFadeSprite *f = i == 6 ? &prompt->fade
                            : i    ? &s->icons[i - 1]
                                   : &s->panel;
    /* 443daa truncates the vertex alpha to8 bits before texture modulation. */
    float a = (uint32_t)((double)f->alpha * 255) / 255.f;
    BkVertex v[4] = {{p->x, p->y, 0, 0, 0, 1, 1, 1, a},
                     {p->x + p->width, p->y, 0, 1, 0, 1, 1, 1, a},
                     {p->x + p->width, p->y + p->height, 0, 1, 1, 1, 1, 1, a},
                     {p->x, p->y + p->height, 0, 0, 1, 1, 1, 1, a}};
    if (!bk_mesh_update(n->renderer, n->meshes[i], v, 4, error))
      return 0;
  }
  BkTextStyle style;
  if (!bk_notice_text_style(n->text_kind, &style))
    return fail(error, "invalid font kind");
  if (n->frame.draw_text &&
      !bk_text_render_prepare(n->text, &style, n->message->bytes,
                              n->message->length, seconds, width, height, flow,
                              error))
    return 0;
  n->width = width;
  n->height = height;
  n->ready = 1;
  return 1;
}
int bk_item_notice_render_prepare(BkItemNoticeRender *n, BkItemNoticeState *s,
                                  BkItemPickupState *pickup,
                                  const BkMessage *message, BkTextFlow *flow,
                                  float seconds, uint32_t now, unsigned width,
                                  unsigned height, char error[256]) {
  if (!n || !message || message->length > sizeof(message->bytes))
    return fail(error, "invalid notice message");
  n->ready = 0;
  if (!bk_item_notice_step(s, pickup, flow, seconds, now, &n->frame))
    return fail(error, "invalid notice state/time");
  if (n->frame.bind_message)
    n->message = message;
  n->draw_mask = 0x3f;
  return prepare(n, s, NULL, flow, seconds, width, height, error);
}
int bk_item_notice_render_prepare_opening(
    BkItemNoticeRender *n, BkItemNoticeState *s, BkPulseSprite *prompt,
    uint8_t old_phase, uint8_t visible, BkTextFlow *flow, float seconds,
    unsigned width, unsigned height, char error[256]) {
  if (!n || !flow)
    return fail(error, "missing opening state");
  n->ready = 0;
  if (!bk_opening_notice_step(s, prompt, old_phase, visible, seconds,
                              &n->frame))
    return fail(error, "invalid opening notice state/time");
  n->draw_mask = old_phase == 0 ? 0x41 : old_phase == 2 ? 1 : 0;
  return prepare(n, s, prompt, flow, seconds, width, height, error);
}
static int recreate(void *context, BkNoticeTextKind kind, char error[256]) {
  BkItemNoticeRender *n = context;
  BkTextStyle style;
  if (!n || !bk_notice_text_style(kind, &style))
    return fail(error, "invalid font kind");
  float zoom = (bk_resources_patch_flags(n->store) & BK_PATCH_CHINESE) ? 1.3f : 1;
  BkTextRender *text = bk_text_render_create_scaled(
      n->renderer, n->store, "Type_S.FTT", (uint32_t)style.width,
      (uint32_t)style.height, zoom, error);
  if (!text)
    return 0;
  bk_text_render_destroy(n->text);
  n->text = text;
  n->text_kind = kind;
  n->message = &n->empty;
  n->ready = 0;
  return 1;
}
static int clear(void *context, char error[256]) {
  BkItemNoticeRender *n = context;
  if (!n)
    return fail(error, "missing font owner");
  bk_text_render_destroy(n->text);
  n->text = NULL;
  n->ready = 0;
  return 1;
}
static int bind(void *context, const BkMessage *message, char error[256]) {
  BkItemNoticeRender *n = context;
  if (!n || !message || message->length > sizeof(message->bytes))
    return fail(error, "invalid bound message");
  n->message = message;
  return 1;
}
BkNoticeTextOps bk_item_notice_render_text_ops(BkItemNoticeRender *n) {
  return n ? (BkNoticeTextOps){n, recreate, clear, bind} : (BkNoticeTextOps){0};
}
int bk_item_notice_render_draw(BkItemNoticeRender *n, char error[256]) {
  if (!n || !n->ready)
    return fail(error, "missing prepared notice");
  float matrix[16];
  memcpy(matrix, bk_identity, sizeof(matrix));
  matrix[0] = 2.f / n->width;
  matrix[5] = 2.f / n->height;
  matrix[12] = (float)(1.0 / n->width - 1);
  matrix[13] = (float)(1.0 / n->height - 1);
  for (unsigned i = 0; i < 7; ++i)
    if ((n->draw_mask & (1u << i)) &&
        !bk_renderer_draw_mesh(
            n->renderer, n->textures[i], n->meshes[i], matrix,
            (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, error))
      return 0;
  return !n->frame.draw_text || bk_text_render_draw(n->text, error);
}
const BkTextRender *bk_item_notice_render_text(const BkItemNoticeRender *n) {
  return n ? n->text : NULL;
}
const BkItemNoticeFrame *
bk_item_notice_render_frame(const BkItemNoticeRender *n) {
  return n && n->ready ? &n->frame : NULL;
}

int bk_item_notice_render_prepare_failure(BkItemNoticeRender *n,
                                          const BkItemNoticeState *state,
                                          const BkPulseSprite *prompt,
                                          const BkFailureHudFrame *frame,
                                          BkTextFlow *flow, float seconds,
                                          unsigned width, unsigned height,
                                          char error[256]) {
  if (!n || !state || !prompt || !frame || !flow)
    return fail(error, "missing failure snapshot");
  BkItemNoticeState snapshot = *state;
  BkPulseSprite pulse = *prompt;
  snapshot.panel.alpha = frame->panel_alpha;
  pulse.fade.alpha = frame->prompt_alpha;
  n->frame = (BkItemNoticeFrame){.draw_text = frame->draw_text};
  n->draw_mask = 0x41;
  return prepare(n, &snapshot, &pulse, flow, seconds, width, height, error);
}
