#include "scene/player_hud_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkPlayerHudRender {
  BkRenderer *renderer;
  BkTexture *textures[BK_PLAYER_HUD_SLOTS];
  BkGpuMesh *meshes[BK_PLAYER_HUD_DRAWS];
  BkPlayerHudFrame frame;
  unsigned width, height;
  int ready;
};
static int fail(char *e, const char *why) {
  snprintf(e, 256, "player HUD render: %s", why);
  return 0;
}
void bk_player_hud_render_destroy(BkPlayerHudRender *h) {
  if (h) {
    for (unsigned i = 0; i < BK_PLAYER_HUD_DRAWS; ++i)
      bk_mesh_destroy(h->renderer, h->meshes[i]);
    for (unsigned i = 0; i < BK_PLAYER_HUD_SLOTS; ++i)
      bk_texture_destroy(h->renderer, h->textures[i]);
    free(h);
  }
}
int bk_player_hud_render_reload(BkPlayerHudRender *h, BkResourceStore *store,
                                unsigned group, uint8_t special, char e[256]) {
  BkPlayerHudLayout l[BK_PLAYER_HUD_SLOTS];
  if (!h || !store || !bk_player_hud_layout(l, group, special, 1280))
    return fail(e, "invalid services/profile");
  BkTexture *loaded[BK_PLAYER_HUD_SLOTS] = {0};
  BkBlob raw = {0};
  BkImage image = {0};
  for (unsigned i = 0; i < BK_PLAYER_HUD_SLOTS; ++i) {
    if (!l[i].name)
      continue;
    BkResourceResult result =
        bk_resources_read(store, "bk3_00", l[i].name, &raw, e);
    if (result != BK_RESOURCE_OK) {
      if (result == BK_RESOURCE_MISSING)
        snprintf(e, 256, "player HUD: missing %s", l[i].name);
      goto bad;
    }
    if (!bk_image_decode(raw.data, raw.size, &image, e))
      goto bad;
    /* ge_10's backwards-scrolling UV interval extends outside[0,1]. */
    loaded[i] =
        bk_texture_create_sampled(h->renderer, &image, BK_WRAP_REPEAT, e);
    bk_image_free(&image);
    bk_blob_free(&raw);
    if (!loaded[i])
      goto bad;
  }
  for (unsigned i = 0; i < BK_PLAYER_HUD_SLOTS; ++i) {
    if (!l[i].name)
      continue;
    bk_texture_destroy(h->renderer, h->textures[i]);
    h->textures[i] = loaded[i];
  }
  h->ready = 0;
  return 1;
bad:
  bk_image_free(&image);
  bk_blob_free(&raw);
  for (unsigned i = 0; i < BK_PLAYER_HUD_SLOTS; ++i)
    bk_texture_destroy(h->renderer, loaded[i]);
  return 0;
}
BkPlayerHudRender *bk_player_hud_render_create(BkRenderer *r,
                                               BkResourceStore *store,
                                               unsigned group, uint8_t special,
                                               char e[256]) {
  if (!r || !store) {
    fail(e, "missing services");
    return NULL;
  }
  BkPlayerHudRender *h = calloc(1, sizeof(*h));
  if (!h) {
    fail(e, "allocation failed");
    return NULL;
  }
  h->renderer = r;
  if (!bk_player_hud_render_reload(h, store, group, special, e))
    goto bad;
  const BkVertex v[4] = {{0, 0, 0, 0, 0, 1, 1, 1, 1},
                         {1, 0, 0, 1, 0, 1, 1, 1, 1},
                         {1, 1, 0, 1, 1, 1, 1, 1, 1},
                         {0, 1, 0, 0, 1, 1, 1, 1, 1}};
  const uint16_t ix[] = {0, 1, 2, 3, 0, 2};
  for (unsigned i = 0; i < BK_PLAYER_HUD_DRAWS; ++i)
    if (!(h->meshes[i] = bk_mesh_create(r, v, 4, ix, 6, e)))
      goto bad;
  return h;
bad:
  bk_player_hud_render_destroy(h);
  return NULL;
}
int bk_player_hud_render_prepare(BkPlayerHudRender *h,
                                 const BkPlayerHudFrame *f, unsigned width,
                                 unsigned height, char e[256]) {
  if (!h || !f || !width || !height || f->count > BK_PLAYER_HUD_DRAWS ||
      (f->capture && f->capture_after > f->count))
    return fail(e, "invalid frame/extent");
  unsigned tw, th;
  bk_renderer_extent(h->renderer, &tw, &th);
  if (width > tw || height > th)
    return fail(e, "viewport exceeds target");
  h->ready = 0;
  for (unsigned i = 0; i < f->count; ++i) {
    const BkPlayerHudDraw *d = &f->draws[i];
    const BkPlayerHudSprite *s = &d->sprite;
    float rect[4];
    if (d->slot >= BK_PLAYER_HUD_SLOTS || !h->textures[d->slot])
      return fail(e, "draw requires a missing/never-loaded retained slot");
    if (!bk_player_hud_rect(s, rect) || !isfinite(s->alpha) || s->alpha < 0 ||
        s->alpha > 1)
      return fail(e, "invalid sprite geometry/alpha");
    for (unsigned j = 0; j < 4; ++j)
      if (!isfinite(s->uv[j]))
        return fail(e, "invalid sprite UV");
    float r = ((s->rgb >> 16) & 255) / 255.f, g = ((s->rgb >> 8) & 255) / 255.f,
          b = (s->rgb & 255) / 255.f;
    float a = (uint32_t)((double)s->alpha * 255) / 255.f;
    const BkVertex v[4] = {
        {rect[0], rect[1], 0, s->uv[0], s->uv[1], r, g, b, a},
        {rect[2], rect[1], 0, s->uv[2], s->uv[1], r, g, b, a},
        {rect[2], rect[3], 0, s->uv[2], s->uv[3], r, g, b, a},
        {rect[0], rect[3], 0, s->uv[0], s->uv[3], r, g, b, a}};
    if (!bk_mesh_update(h->renderer, h->meshes[i], v, 4, e))
      return 0;
  }
  h->frame = *f;
  h->width = width;
  h->height = height;
  h->ready = 1;
  return 1;
}
int bk_player_hud_render_draw(BkPlayerHudRender *h,
                              const BkPlayerHudCapture *capture, char e[256]) {
  if (!h || !h->ready || (h->frame.capture && (!capture || !capture->capture)))
    return fail(e, "missing prepared HUD/capture service");
  float matrix[16];
  memcpy(matrix, bk_identity, sizeof(matrix));
  matrix[0] = 2.f / h->width;
  matrix[5] = 2.f / h->height;
  matrix[12] = (float)(1.0 / h->width - 1);
  matrix[13] = (float)(1.0 / h->height - 1);
  for (unsigned i = 0; i <= h->frame.count; ++i) {
    if (h->frame.capture && h->frame.capture_after == i &&
        !capture->capture(capture->context, e))
      return 0;
    if (i < h->frame.count &&
        !bk_renderer_draw_mesh(
            h->renderer, h->textures[h->frame.draws[i].slot], h->meshes[i],
            matrix, (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE}, e))
      return 0;
  }
  return 1;
}
