#include "model/material.h"
#include <inttypes.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: material-probe archive.pp model.x\n");
    return 2;
  }
  char error[256] = {0};
  BkResourceStore *store = bk_resources_create(error);
  BkBlob blob = {0};
  BkModel *m = NULL;
  int result = 1, *alpha = NULL;
  uint64_t bytes = 0;
  uint32_t modes[3] = {0}, hidden = 0, texture_alpha = 0;
  if (!store || !bk_resources_mount(store, "scene", argv[1], error) ||
      bk_resources_read(store, "scene", argv[2], &blob, error) !=
          BK_RESOURCE_OK ||
      bk_model_decode(blob.data, blob.size, &m, error) != BK_MODEL_OK)
    goto done;
  bk_blob_free(&blob);
  alpha = calloc(m->texture_count ? m->texture_count : 1, sizeof(*alpha));
  if (!alpha) {
    snprintf(error, 256, "texture flags allocation failed");
    goto done;
  }
  for (uint32_t i = 0; i < m->texture_count; i++) {
    BkModelTextureImage t;
    if (!bk_model_texture_load(m, i, store, "scene", &t, error))
      goto done;
    alpha[i] = t.alpha_hint;
    texture_alpha += !!t.alpha_hint;
    bytes += (uint64_t)t.image.width * t.image.height * 4;
    bk_image_free(&t.image);
  }
  for (uint32_t i = 0; i < m->submesh_count; i++) {
    BkModelSubmesh *s = &m->submeshes[i];
    if (s->material_index == BK_MODEL_NONE || s->texture_count > 1) {
      snprintf(error, 256,
               "submesh %u: unsupported material/stage configuration", i);
      goto done;
    }
    int a = s->texture_indices[0] == BK_MODEL_NONE
                ? 0
                : alpha[s->texture_indices[0]];
    BkMaterialState state;
    if (!bk_material_state(&m->materials[s->material_index], a, &state, error))
      goto done;
    modes[state.blend]++;
    hidden += !state.visible;
  }
  printf(
      "{\"textures\":%u,\"texture_alpha_flags\":%u,\"decoded_bytes\":%" PRIu64
      ",\"submeshes\":%u,\"normal\":%u,\"additive\":%u,\"inverse_color\":%u,"
      "\"hidden\":%u}\n",
      m->texture_count, texture_alpha, bytes, m->submesh_count, modes[0],
      modes[1], modes[2], hidden);
  result = 0;
done:
  if (result)
    fprintf(stderr, "%s\n", error);
  free(alpha);
  bk_blob_free(&blob);
  bk_model_destroy(m);
  bk_resources_destroy(store);
  return result;
}
