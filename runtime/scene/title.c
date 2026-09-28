#include "scene/scene_internal.h"
#include "ui/debug_overlay.h"
#include <stdlib.h>
typedef struct {
  BkScene base;
  BkRenderer *renderer;
  BkTexture *title, *overlay;
} TitleScene;
static int quad(BkRenderer *renderer, BkTexture *texture, float x, float y,
                float w, float h, char error[256]) {
  float x0 = x / 640 - 1, y0 = y / 360 - 1;
  float x1 = (x + w) / 640 - 1, y1 = (y + h) / 360 - 1;
  BkVertex vertices[6] = {
      {x0, y0, 0, 0, 0, 1, 1, 1, 1}, {x1, y0, 0, 1, 0, 1, 1, 1, 1},
      {x1, y1, 0, 1, 1, 1, 1, 1, 1}, {x0, y0, 0, 0, 0, 1, 1, 1, 1},
      {x1, y1, 0, 1, 1, 1, 1, 1, 1}, {x0, y1, 0, 0, 1, 1, 1, 1, 1}};
  return bk_renderer_draw(renderer, texture, vertices, 6, bk_identity, error);
}
static int draw(BkScene *base, const BkSceneFrame *frame, char error[256]) {
  (void)frame; /* Static title; no gameplay simulation is claimed. */
  TitleScene *scene = (TitleScene *)base;
  return quad(scene->renderer, scene->title, 160, 0, 960, 720, error) &&
         quad(scene->renderer, scene->overlay, 0, 656, 1280, 64, error);
}
static void destroy(BkScene *base) {
  TitleScene *scene = (TitleScene *)base;
  bk_texture_destroy(scene->renderer, scene->overlay);
  bk_texture_destroy(scene->renderer, scene->title);
  free(scene);
}
BkScene *bk_title_create(const BkSceneServices *services, char error[256]) {
  TitleScene *scene = calloc(1, sizeof(*scene));
  if (!scene) {
    snprintf(error, 256, "title scene allocation failed");
    return NULL;
  }
  scene->base = (BkScene){.step = NULL, .draw = draw, .destroy = destroy};
  scene->renderer = services->renderer;
  BkBlob blob = {0};
  BkImage image = {0};
  if (bk_resources_read(services->resources, "bk3_00", "op_00.bmp", &blob,
                        error) != BK_RESOURCE_OK ||
      !bk_image_decode(blob.data, blob.size, &image, error))
    goto fail;
  bk_blob_free(&blob);
  scene->title = bk_texture_create(scene->renderer, &image, error);
  bk_image_free(&image);
  if (!scene->title || !bk_debug_banner_create(&image, error))
    goto fail;
  scene->overlay = bk_texture_create(scene->renderer, &image, error);
  bk_image_free(&image);
  if (!scene->overlay)
    goto fail;
  return &scene->base;
fail:
  bk_blob_free(&blob);
  bk_image_free(&image);
  destroy(&scene->base);
  return NULL;
}
