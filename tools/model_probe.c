#include "model/model.h"
#include "resource/assets.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: model-probe archive.pp model.x\n");
    return 2;
  }
  char error[256];
  BkArchive archive = {0};
  uint8_t *data = NULL;
  BkModel *model = NULL;
  int result = 1;
  if (!bk_archive_open(&archive, argv[1], error))
    goto done;
  const BkEntry *entry = bk_archive_find(&archive, argv[2]);
  if (!bk_archive_read(&archive, entry, &data, error))
    goto done;
  if (bk_model_decode(data, entry->size, &model, error) != BK_MODEL_OK)
    goto done;
  printf("{\"meshes\":%u,\"submeshes\":%u,\"vertices\":%" PRIu64
         ",\"triangles\":%" PRIu64
         ",\"materials\":%u,\"textures\":%u,\"frames\":%u}\n",
         model->mesh_count, model->submesh_count, model->vertex_count,
         model->triangle_count, model->material_count, model->texture_count,
         model->frame_count);
  result = 0;
done:
  if (result)
    fprintf(stderr, "%s\n", error);
  bk_model_destroy(model);
  free(data);
  bk_archive_close(&archive);
  return result;
}
