/* GPU contract tests and optional original-material swatches, not a scene. */
#include "model/material.h"
#include "render/renderer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static const uint16_t indices[] = {0, 1, 2, 0, 2, 3};
static const float background[] = {51.0f / 255, 102.0f / 255, 153.0f / 255, 1};
static BkGpuMesh *quad(BkRenderer *r, const float color[4], float u, float v,
                       float z, char error[256]) {
  BkVertex vertices[4] = {{-1, -1, z, u, v, 0, 0, 0, 0},
                          {1, -1, z, u, v, 0, 0, 0, 0},
                          {1, 1, z, u, v, 0, 0, 0, 0},
                          {-1, 1, z, u, v, 0, 0, 0, 0}};
  for (unsigned i = 0; i < 4; i++) {
    vertices[i].r = color[0];
    vertices[i].g = color[1];
    vertices[i].b = color[2];
    vertices[i].a = color[3];
  }
  return bk_mesh_create(r, vertices, 4, indices, 6, error);
}
static float clamp(float x) { return fmaxf(0, fminf(1, x)); }
static int pixel_check(BkRenderer *r, const float *expected, char error[256]) {
  uint8_t pixel[4];
  if (!bk_renderer_readback(r, pixel, sizeof(pixel), error))
    return 0;
  for (unsigned i = 0; i < 4; i++) {
    int want = (int)lroundf(255 * clamp(expected[i]));
    if (abs((int)pixel[i] - want) > 2) {
      snprintf(error, 256, "component %u: GPU %u, expected %d", i, pixel[i],
               want);
      return 0;
    }
  }
  return 1;
}
static int swatch(BkRenderer *r, BkTexture *white, BkGpuMesh *back,
                  BkTexture *texture, const uint8_t pixel[4], unsigned w,
                  unsigned h, const BkMaterialState *state, char error[256]) {
  const BkBlend modes[] = {BK_BLEND_ALPHA, BK_BLEND_ADDITIVE,
                           BK_BLEND_INVERSE_COLOR};
  BkGpuMesh *mesh = quad(r, state->diffuse, 0.5f / w, 0.5f / h, 0.25f, error);
  if (!mesh)
    return 0;
  int ok = bk_renderer_begin(r, error) &&
           bk_renderer_draw_mesh(
               r, white, back, bk_identity,
               (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE}, error) &&
           (!state->visible ||
            bk_renderer_draw_mesh(
                r, texture, mesh, bk_identity,
                (BkDrawState){modes[state->blend], 1, BK_CULL_NONE}, error)) &&
           bk_renderer_end(r, error);
  float expected[4];
  float a = clamp(pixel[3] / 255.0f * state->diffuse[3]);
  for (unsigned i = 0; i < 4; i++) {
    float src = clamp(pixel[i] / 255.0f * state->diffuse[i]),
          dst = background[i];
    expected[i] = !state->visible                     ? dst
                  : state->blend == BK_MATERIAL_ALPHA ? src * a + dst * (1 - a)
                  : state->blend == BK_MATERIAL_ADDITIVE ? src * a + dst
                                                         : dst * (1 - src);
  }
  if (ok)
    ok = pixel_check(r, expected, error);
  bk_mesh_destroy(r, mesh);
  return ok;
}
static int originals(BkRenderer *r, BkTexture *white, BkGpuMesh *back,
                     const char *pack, const char *name, char error[256]) {
  BkResourceStore *store = bk_resources_create(error);
  BkBlob blob = {0};
  BkModel *model = NULL;
  int ok = 0;
  BkModelTextureImage *images = NULL;
  BkTexture **textures = NULL;
  if (!store || !bk_resources_mount(store, "scene", pack, error) ||
      bk_resources_read(store, "scene", name, &blob, error) != BK_RESOURCE_OK ||
      bk_model_decode(blob.data, blob.size, &model, error) != BK_MODEL_OK)
    goto done;
  bk_blob_free(&blob);
  images =
      calloc(model->texture_count ? model->texture_count : 1, sizeof(*images));
  textures = calloc(model->texture_count ? model->texture_count : 1,
                    sizeof(*textures));
  if (!images || !textures) {
    snprintf(error, 256, "texture array allocation failed");
    goto done;
  }
  for (unsigned i = 0; i < model->texture_count; i++) {
    if (!bk_model_texture_load(model, i, store, "scene", &images[i], error))
      goto done;
    textures[i] =
        bk_texture_create_sampled(r, &images[i].image, BK_WRAP_REPEAT, error);
    if (!textures[i])
      goto done;
  }
  unsigned checked = 0;
  for (unsigned i = 0; i < model->submesh_count; i++) {
    BkModelSubmesh *sub = &model->submeshes[i];
    if (sub->material_index == BK_MODEL_NONE || sub->texture_count > 1) {
      snprintf(error, 256, "unsupported material/stages at submesh %u", i);
      goto done;
    }
    uint32_t ti = sub->texture_indices[0];
    uint8_t solid[] = {255, 255, 255, 255};
    BkImage im = {1, 1, solid};
    BkTexture *texture = white;
    int alpha = 0;
    if (ti != BK_MODEL_NONE) {
      im = images[ti].image;
      texture = textures[ti];
      alpha = images[ti].alpha_hint;
    }
    BkMaterialState state;
    if (!bk_material_state(&model->materials[sub->material_index], alpha,
                           &state, error) ||
        !swatch(r, white, back, texture, im.rgba, im.width, im.height, &state,
                error)) {
      fprintf(stderr, "original swatch failed at submesh %u\n", i);
      goto done;
    }
    checked++;
  }
  fprintf(stderr,
          "PASS: %u original textures uploaded, %u material swatches within "
          "2/255\n",
          model->texture_count, checked);
  ok = 1;
done:
  if (model)
    for (unsigned i = 0; i < model->texture_count; i++) {
      if (textures)
        bk_texture_destroy(r, textures[i]);
      if (images)
        bk_image_free(&images[i].image);
    }
  free(textures);
  free(images);
  bk_model_destroy(model);
  bk_blob_free(&blob);
  bk_resources_destroy(store);
  return ok;
}
int main(int argc, char **argv) {
  if (argc != 1 && argc != 3) {
    fprintf(stderr, "usage: material-render-probe [archive.pp model.x]\n");
    return 2;
  }
  char error[256] = {0};
  int result = 1;
  BkRenderer *r = bk_renderer_create(1, 1, stderr, error);
  BkTexture *white = NULL, *repeat = NULL, *clamped = NULL;
  BkGpuMesh *back = NULL, *front = NULL, *rear = NULL, *sample = NULL;
  if (!r)
    goto done;
  uint8_t solid[] = {255, 255, 255, 255};
  BkImage im = {1, 1, solid};
  white = bk_texture_create(r, &im, error);
  back = quad(r, background, 0.5f, 0.5f, 0.75f, error);
  if (!white || !back)
    goto done;
  const float alpha[] = {1, 0.5f, 1.5f, -1, 0};
  for (unsigned i = 0; i < 5; i++) {
    BkModelMaterial material = {0};
    material.diffuse[0] = 0.6f;
    material.diffuse[1] = 0.2f;
    material.diffuse[2] = 0.1f;
    material.diffuse[3] = alpha[i];
    BkMaterialState state;
    if (!bk_material_state(&material, 0, &state, error) ||
        !swatch(r, white, back, white, solid, 1, 1, &state, error))
      goto done;
  }
  float red[] = {1, 0, 0, 1}, green[] = {0, 1, 0, 1}, full[] = {1, 1, 1, 1};
  front = quad(r, red, 0.5f, 0.5f, 0.25f, error);
  rear = quad(r, green, 0.5f, 0.5f, 0.5f, error);
  if (!front || !rear)
    goto done;
  for (int write = 0; write < 2; write++) {
    if (!bk_renderer_begin(r, error) ||
        !bk_renderer_draw_mesh(
            r, white, front, bk_identity,
            (BkDrawState){BK_BLEND_OPAQUE, write, BK_CULL_NONE}, error) ||
        !bk_renderer_draw_mesh(r, white, rear, bk_identity,
                               (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE},
                               error) ||
        !bk_renderer_end(r, error) ||
        !pixel_check(r, write ? red : green, error))
      goto done;
  }
  uint8_t pattern[] = {255, 0, 0, 255, 0, 255, 0, 255};
  im = (BkImage){2, 1, pattern};
  repeat = bk_texture_create_sampled(r, &im, BK_WRAP_REPEAT, error);
  clamped = bk_texture_create(r, &im, error);
  sample = quad(r, full, 1.25f, 0.5f, 0.5f, error);
  if (!repeat || !clamped || !sample)
    goto done;
  for (int wrap = 0; wrap < 2; wrap++) {
    if (!bk_renderer_begin(r, error) ||
        !bk_renderer_draw_mesh(r, wrap ? repeat : clamped, sample, bk_identity,
                               (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE},
                               error) ||
        !bk_renderer_end(r, error) ||
        !pixel_check(r, wrap ? red : green, error))
      goto done;
  }
  /* Instance matrix must move a mesh out of view without changing the asset. */
  float moved[16];
  memcpy(moved, bk_identity, sizeof(moved));
  moved[12] = 3;
  if (!bk_renderer_begin(r, error) ||
      !bk_renderer_draw_mesh(r, white, back, bk_identity,
                             (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE},
                             error) ||
      !bk_renderer_draw_mesh(r, white, front, moved,
                             (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE},
                             error) ||
      !bk_renderer_end(r, error) || !pixel_check(r, background, error))
    goto done;
  /* An indexed material draw must not leak pipeline/buffer state into UI. */
  BkVertex ui[3] = {{-1, -1, 0, 0.5f, 0.5f, 0, 0, 1, 0.4f},
                    {3, -1, 0, 0.5f, 0.5f, 0, 0, 1, 0.4f},
                    {-1, 3, 0, 0.5f, 0.5f, 0, 0, 1, 0.4f}};
  /* Inverse-color pass also clears destination alpha (1 - source alpha).
   * UI's ONE alpha factor then contributes 0.4 rather than 0.16. */
  float ui_expected[] = {0, 0, 0.4f, 0.4f};
  if (!bk_renderer_begin(r, error) ||
      !bk_renderer_draw_mesh(
          r, white, front, bk_identity,
          (BkDrawState){BK_BLEND_INVERSE_COLOR, 1, BK_CULL_NONE}, error) ||
      !bk_renderer_draw(r, white, ui, 3, bk_identity, error) ||
      !bk_renderer_end(r, error) || !pixel_check(r, ui_expected, error))
    goto done;
  /* Reject invalid indexed input before issuing GPU commands. */
  BkVertex invalid[1] = {{0}};
  uint16_t bad[] = {0, 1, 0};
  BkGpuMesh *unexpected = bk_mesh_create(r, invalid, 1, bad, 3, error);
  if (unexpected) {
    bk_mesh_destroy(r, unexpected);
    snprintf(error, 256, "accepted bad index");
    goto done;
  }
  fprintf(stderr, "PASS: indexed uploads, three blend modes, zero-alpha "
                  "visibility, depth write, repeat/clamp, instance transform "
                  "and UI state restoration pixels\n");
  if (argc == 3 && !originals(r, white, back, argv[1], argv[2], error))
    goto done;
  result = 0;
done:
  if (result)
    fprintf(stderr, "material render probe FAILED: %s\n", error);
  bk_mesh_destroy(r, sample);
  bk_mesh_destroy(r, rear);
  bk_mesh_destroy(r, front);
  bk_mesh_destroy(r, back);
  bk_texture_destroy(r, clamped);
  bk_texture_destroy(r, repeat);
  bk_texture_destroy(r, white);
  bk_renderer_destroy(r);
  return result;
}
