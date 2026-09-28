#include "model/skin.h"
#include "resource/assets.h"
#include <stdio.h>
#include <stdlib.h>
static int probe(BkArchive *archive, const char *name, char error[256]) {
  uint8_t *data = NULL;
  BkModel *model = NULL;
  BkModelSkin *skin = NULL;
  BkSkinMesh **meshes = NULL;
  float *world = NULL;
  uint32_t count = 0;
  int ok = 0;
  const BkEntry *entry = bk_archive_find(archive, name);
  if (!bk_archive_read(archive, entry, &data, error) ||
      bk_model_decode(data, entry->size, &model, error) != BK_MODEL_OK)
    goto done;
  free(data);
  data = NULL;
  skin = bk_model_skin_create(model, error);
  if (!skin)
    goto done;
  count = bk_model_skin_count(skin);
  size_t floats = (size_t)model->frame_count * 16;
  world = malloc(floats * sizeof(*world));
  meshes = calloc(count, sizeof(*meshes));
  if (!world || !meshes) {
    snprintf(error, 256, "skin probe allocation failed");
    goto done;
  }
  if (!bk_model_world_matrices(model, world, floats, error))
    goto done;
  for (uint32_t i = 0; i < count; i++) {
    const BkSkinEntry *e = bk_model_skin_entry(skin, i);
    meshes[i] =
        bk_skin_mesh_create(skin, i, &model->submeshes[e->submesh], error);
    if (!meshes[i])
      goto done;
  }
  bk_model_destroy(model);
  model = NULL;
  for (uint32_t i = 0; i < count; i++) {
    if (!bk_skin_mesh_apply(meshes[i], world, floats, NULL, 0, error) ||
        !bk_skin_mesh_apply(meshes[i], world, floats,
                            bk_skin_mesh_vertices(meshes[i]),
                            bk_skin_mesh_count(meshes[i]), error))
      goto done;
  }
  printf("PASS %s: %u skin meshes after model release\n", name, count);
  ok = 1;
done:
  if (meshes)
    for (uint32_t i = 0; i < count; i++)
      bk_skin_mesh_destroy(meshes[i]);
  free(meshes);
  free(world);
  bk_model_skin_destroy(skin);
  bk_model_destroy(model);
  free(data);
  return ok;
}
int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: skin-probe archive.pp model.x [model.x ...]\n");
    return 2;
  }
  BkArchive archive = {0};
  char error[256];
  int result = 1;
  if (!bk_archive_open(&archive, argv[1], error))
    goto done;
  for (int i = 2; i < argc; i++)
    if (!probe(&archive, argv[i], error))
      goto done;
  result = 0;
done:
  if (result)
    fprintf(stderr, "%s\n", error);
  bk_archive_close(&archive);
  return result;
}
