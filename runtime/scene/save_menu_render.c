#include "scene/save_menu_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkSaveMenuRender {
  BkRenderer *renderer;
  BkResourceStore *store;
  BkTexture *textures[58];
  BkGpuMesh *meshes[BK_SAVE_MENU_DRAWS];
  BkTextRender *text;
  BkSaveMenuFrame frame;
  unsigned mode, group, width, height;
  int ready;
};
static int fail(char *e, const char *why) {
  snprintf(e, 256, "save menu render: %s", why);
  return 0;
}
void bk_save_menu_render_destroy(BkSaveMenuRender *r) {
  if (!r)
    return;
  bk_text_render_destroy(r->text);
  for (unsigned i = 0; i < 58; ++i)
    bk_texture_destroy(r->renderer, r->textures[i]);
  for (unsigned i = 0; i < BK_SAVE_MENU_DRAWS; ++i)
    bk_mesh_destroy(r->renderer, r->meshes[i]);
  free(r);
}
static BkTexture *texture(BkSaveMenuRender *r, unsigned slot, unsigned group,
                          char *e) {
  BkBlob b = {0};
  BkImage image = {0};
  BkTexture *t = NULL;
  const char *name = bk_save_menu_image(slot, group, r->mode);
  BkResourceResult result = bk_resources_read(r->store, "bk3_00", name, &b, e);
  if (result == BK_RESOURCE_MISSING)
    snprintf(e, 256, "save menu render: missing %s", name);
  if (result == BK_RESOURCE_OK && bk_image_decode(b.data, b.size, &image, e))
    t = bk_texture_create_sampled(r->renderer, &image, BK_WRAP_REPEAT, e);
  bk_image_free(&image);
  bk_blob_free(&b);
  return t;
}
int bk_save_menu_render_details(BkSaveMenuRender *r, unsigned group,
                                char e[256]) {
  if (!r || group >= 5)
    return fail(e, "invalid details group");
  BkTexture *loaded[14] = {0};
  for (unsigned i = 0; i < 14; ++i)
    if (!(loaded[i] = texture(r, 42 + i, group, e))) {
      for (unsigned j = 0; j < 14; ++j)
        bk_texture_destroy(r->renderer, loaded[j]);
      return 0;
    }
  for (unsigned i = 0; i < 14; ++i) {
    bk_texture_destroy(r->renderer, r->textures[42 + i]);
    r->textures[42 + i] = loaded[i];
  }
  r->group = group;
  r->ready = 0;
  return 1;
}
BkSaveMenuRender *bk_save_menu_render_create(BkRenderer *renderer,
                                             BkResourceStore *store,
                                             unsigned mode, unsigned group,
                                             char e[256]) {
  if (!renderer || !store || mode > 1 || group >= 5) {
    fail(e, "invalid services/profile");
    return NULL;
  }
  BkSaveMenuRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(e, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  r->store = store;
  r->mode = mode;
  r->group = group;
  for (unsigned i = 0; i < 58; ++i) {
    if ((mode && (i == 0 || i == 9)) || (!mode && (i == 19 || i == 20)))
      continue;
    if (!(r->textures[i] = texture(r, i, group, e)))
      goto bad;
  }
  r->text = bk_text_render_create(renderer, store, "Type_G.FTT", 314, 268, e);
  if (!r->text)
    goto bad;
  const BkVertex vertices[4] = {{0, 0, 0, 0, 0, 1, 1, 1, 1},
                                {1, 0, 0, 1, 0, 1, 1, 1, 1},
                                {1, 1, 0, 1, 1, 1, 1, 1, 1},
                                {0, 1, 0, 0, 1, 1, 1, 1, 1}};
  const uint16_t indices[] = {0, 1, 2, 3, 0, 2};
  for (unsigned i = 0; i < BK_SAVE_MENU_DRAWS; ++i)
    if (!(r->meshes[i] = bk_mesh_create(renderer, vertices, 4, indices, 6, e)))
      goto bad;
  return r;
bad:
  bk_save_menu_render_destroy(r);
  return NULL;
}
int bk_save_menu_render_prepare(BkSaveMenuRender *r, const BkSaveMenuFrame *f,
                                const uint8_t *labels, size_t size,
                                BkTextFlow *flow, float seconds, unsigned width,
                                unsigned height, char e[256]) {
  if (!r || !f || f->count > BK_SAVE_MENU_DRAWS ||
      (f->text_after != UINT32_MAX && f->text_after > f->count) || !width ||
      !height)
    return fail(e, "invalid snapshot");
  r->ready = 0;
  unsigned tw, th;
  bk_renderer_extent(r->renderer, &tw, &th);
  if (width > tw || height > th)
    return fail(e, "invalid extent");
  for (unsigned i = 0; i < f->count; ++i) {
    const BkPauseDraw *d = &f->draws[i];
    const float *q = d->rect;
    if (d->slot >= 58 || !r->textures[d->slot] || !isfinite(d->alpha) ||
        d->alpha < 0 || d->alpha > 1 || !isfinite(q[0]) || !isfinite(q[1]) ||
        !isfinite(q[2]) || !isfinite(q[3]) || q[2] <= 0 || q[3] <= 0 ||
        !isfinite(q[0] + q[2]) || !isfinite(q[1] + q[3]))
      return fail(e, "invalid draw/missing image");
    float x = q[0], y = q[1], right = x + q[2], bottom = y + q[3],
          a = (uint32_t)((double)d->alpha * 255) / 255.f;
    const BkVertex v[4] = {{x, y, 0, 0, 0, 1, 1, 1, a},
                           {right, y, 0, 1, 0, 1, 1, 1, a},
                           {right, bottom, 0, 1, 1, 1, 1, 1, a},
                           {x, bottom, 0, 0, 1, 1, 1, 1, a}};
    if (!bk_mesh_update(r->renderer, r->meshes[i], v, 4, e))
      return 0;
  }
  if (f->text_after != UINT32_MAX) {
    if (!flow || !labels || size > 511)
      return fail(e, "missing labels/flow");
    /*4eade8 white text,4aa32e shadow1/dist1;509979 resets only flags.
     *4aaa17 ignores its apparent two arguments and reads global seconds. */
    BkTextStyle style = {.x = 220,
                         .y = 118,
                         .width = 314,
                         .height = 268,
                         .step_x = 14,
                         .step_y = 27,
                         .shadow = 1,
                         .opacity = 1,
                         .color = {1, 1, 1},
                         .shadow_distance = 1};
    flow->started = 0;
    flow->enabled = 1;
    if (!bk_text_render_prepare(r->text, &style, labels, size, seconds, width,
                                height, flow, e))
      return 0;
  }
  r->frame = *f;
  r->width = width;
  r->height = height;
  r->ready = 1;
  return 1;
}
int bk_save_menu_render_draw(BkSaveMenuRender *r, char e[256]) {
  if (!r || !r->ready)
    return fail(e, "no prepared snapshot");
  float matrix[16];
  memcpy(matrix, bk_identity, sizeof(matrix));
  matrix[0] = 2.f / r->width;
  matrix[5] = 2.f / r->height;
  matrix[12] = (float)(1.0 / r->width - 1);
  matrix[13] = (float)(1.0 / r->height - 1);
  for (unsigned i = 0; i <= r->frame.count; ++i) {
    if (i == r->frame.text_after && !bk_text_render_draw(r->text, e))
      return 0;
    if (i == r->frame.count)
      break;
    if (!bk_renderer_draw_mesh(
            r->renderer, r->textures[r->frame.draws[i].slot], r->meshes[i],
            matrix, (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, e))
      return 0;
  }
  return 1;
}
