#include "scene/rain_render.h"
#include <stdlib.h>
#include <string.h>
struct BkRainRender {
  BkRenderer *renderer;
  BkTexture *texture;
  BkGpuMesh *quad;
  BkRainState state;
  BkRainDraw draw;
  unsigned width, height;
  int ready;
};
static int fail(char *error, const char *why) {
  snprintf(error, 256, "rain render: %s", why);
  return 0;
}
void bk_rain_render_destroy(BkRainRender *r) {
  if (!r)
    return;
  bk_mesh_destroy(r->renderer, r->quad);
  bk_texture_destroy(r->renderer, r->texture);
  free(r);
}
BkRainRender *bk_rain_render_create(BkRenderer *renderer,
                                    BkResourceStore *store, unsigned group,
                                    unsigned area, int enabled,
                                    char error[256]) {
  BkRainState state;
  if (!renderer || !store ||
      !bk_rain_initialize(&state, group, area, enabled)) {
    fail(error, "invalid profile/services");
    return NULL;
  }
  BkRainRender *r = calloc(1, sizeof(*r));
  if (!r) {
    fail(error, "allocation failed");
    return NULL;
  }
  r->renderer = renderer;
  r->state = state;
  if (!state.present)
    return r;
  BkBlob blob = {0};
  BkImage image = {0};
  if (bk_resources_read(store, "bk3_20", "rain.tga", &blob, error) !=
          BK_RESOURCE_OK ||
      !bk_image_decode(blob.data, blob.size, &image, error))
    goto bad;
  if (image.width != 256 || image.height != 256) {
    fail(error, "changed rain texture dimensions");
    goto bad;
  }
  r->texture = bk_texture_create(renderer, &image, error);
  const BkVertex vertices[] = {{0, 0, 0, 0, 0, 1, 1, 1, 1},
                               {256, 0, 0, 1, 0, 1, 1, 1, 1},
                               {256, 512, 0, 1, 1, 1, 1, 1, 1},
                               {0, 512, 0, 0, 1, 1, 1, 1, 1}};
  const uint16_t indices[] = {0, 1, 2, 3, 0, 2};
  if (!r->texture ||
      !(r->quad = bk_mesh_create(renderer, vertices, 4, indices, 6, error)))
    goto bad;
  bk_blob_free(&blob);
  bk_image_free(&image);
  return r;
bad:
  bk_blob_free(&blob);
  bk_image_free(&image);
  bk_rain_render_destroy(r);
  return NULL;
}
int bk_rain_render_prepare(BkRainRender *r, uint32_t *random, int enabled,
                           unsigned width, unsigned height, char error[256]) {
  if (!r || !random || !width || !height || width > 16384 || height > 16384)
    return fail(error, "invalid frame extent/state");
  BkRainState next = r->state;
  uint32_t seed = *random;
  BkRainDraw draw;
  if (!bk_rain_draw(&next, &seed, enabled, (float)((double)width / 1280),
                    &draw))
    return fail(error, "invalid rain draw policy");
  r->state = next;
  r->draw = draw;
  r->width = width;
  r->height = height;
  r->ready = 1;
  *random = seed;
  return 1;
}
int bk_rain_render_draw(BkRainRender *r, char error[256]) {
  if (!r || !r->ready)
    return fail(error, "missing prepared draw");
  for (unsigned i = 0; i < r->draw.count; ++i) {
    const BkRainSprite *s = &r->draw.sprites[i];
    float matrix[16];
    memcpy(matrix, bk_identity, 64);
    matrix[0] = 2.f / r->width;
    matrix[5] = 2.f / r->height;
    /* D3D8/9 transformed sprite centers are integer coordinates; Vulkan
     * samples at half integers. Preserve native UVs with a +.5 pixel shift. */
    matrix[12] = (float)((2.0 * s->x + 1) / r->width - 1);
    matrix[13] = (float)((2.0 * s->y + 1) / r->height - 1);
    if (!bk_renderer_draw_mesh(r->renderer, r->texture, r->quad, matrix,
                               (BkDrawState){BK_BLEND_ALPHA, 0, BK_CULL_NONE},
                               error))
      return 0;
  }
  return 1;
}
const BkRainState *bk_rain_render_state(const BkRainRender *r) {
  return r ? &r->state : NULL;
}
const BkRainDraw *bk_rain_render_snapshot(const BkRainRender *r) {
  return r && r->ready ? &r->draw : NULL;
}
