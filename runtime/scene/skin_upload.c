#include "scene/skin_upload.h"
#include <stdlib.h>
#include <string.h>
int bk_skin_upload(BkRenderer *r, BkGpuMesh *mesh, BkSkinPalette *palette,
                   const BkModelSkin *skin, unsigned index, char error[256]) {
  const BkSkinEntry *entry = bk_model_skin_entry(skin, index);
  if (!entry || !entry->vertex_count) {
    snprintf(error, 256, "invalid skin upload entry");
    return 0;
  }
  uint32_t count = entry->vertex_count;
  uint32_t *offsets = calloc((size_t)count + 1, 4), *cursor = calloc(count, 4);
  float *beta = calloc(count, sizeof(float));
  BkGpuSkinWeight *weights = NULL;
  int ok = 0;
  if (!offsets || !cursor || !beta)
    goto oom;
  uint64_t total = 0;
  for (uint32_t j = 0; j < entry->bone_count; j++) {
    const BkSkinBone *b = bk_model_skin_bone(skin, index, j);
    total += b->count;
    if (total > 2000000) {
      snprintf(error, 256, "skin influence capacity exceeded");
      goto done;
    }
    for (uint32_t k = 0; k < b->count; k++)
      offsets[b->influences[k].index + 1]++;
  }
  /* A zero-influence ENVL mesh retains authored/source vertices while still
   * drawing with the native world-space identity. No compute is necessary. */
  if (!total) {
    ok = 1;
    goto done;
  }
  for (uint32_t i = 0; i < count; i++)
    offsets[i + 1] += offsets[i];
  memcpy(cursor, offsets, count * 4);
  weights = malloc((size_t)total * sizeof(*weights));
  if (!weights)
    goto oom;
  for (uint32_t j = 0; j < entry->bone_count; j++) {
    const BkSkinBone *b = bk_model_skin_bone(skin, index, j);
    for (uint32_t k = 0; k < b->count; k++) {
      const BkSkinInfluence *in = b->influences + k;
      float previous = beta[in->index], weight = in->weight;
      int first = previous <= .001f;
      int finish = !first && (double)previous + weight >= (double).999f;
      if (finish)
        weight = (float)(1.0 - previous);
      beta[in->index] = first    ? weight
                        : finish ? 1
                                 : (float)((double)previous + weight);
      BkGpuSkinWeight *w = weights + cursor[in->index]++;
      *w = (BkGpuSkinWeight){
          .bone = b->frame, .reset = (uint32_t)first, .weight = weight};
      memcpy(w->position, in->position, sizeof(w->position));
      memcpy(w->normal, in->normal, sizeof(w->normal));
    }
  }
  ok = bk_lit_mesh_skin(r, mesh, palette, offsets, weights, (unsigned)total,
                        error);
  goto done;
oom:
  snprintf(error, 256, "skin upload allocation failed");
done:
  free(offsets);
  free(cursor);
  free(beta);
  free(weights);
  return ok;
}
