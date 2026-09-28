#include "model/morph_group.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
struct BkMorphGroup {
  BkModelMorph *morph;
  BkMorphMesh **meshes;
  BkMorphBinding *bindings[64];
  uint32_t count, mesh_count;
  float time;
};
static int fail(char *error, const char *why) {
  snprintf(error, 256, "MORP group: %s", why);
  return 0;
}
void bk_morph_group_destroy(BkMorphGroup *g) {
  if (!g)
    return;
  for (uint32_t i = 0; i < g->count; i++)
    bk_morph_binding_destroy(g->bindings[i]);
  if (g->meshes)
    for (uint32_t i = 0; i < g->mesh_count; i++)
      bk_morph_mesh_destroy(g->meshes[i]);
  free(g->meshes);
  bk_model_morph_destroy(g->morph);
  free(g);
}
BkMorphGroup *bk_morph_group_create(const BkModel *model, char error[256]) {
  if (!model || !model->submeshes || !model->submesh_count) {
    fail(error, "missing target model");
    return NULL;
  }
  BkMorphGroup *g = calloc(1, sizeof(*g));
  if (!g) {
    fail(error, "allocation failed");
    return NULL;
  }
  g->morph = bk_model_morph_create(model, error);
  if (!g->morph)
    goto bad;
  uint32_t count = bk_model_morph_count(g->morph);
  if (count > 64) {
    fail(error, "more than 64 ordered tracks unsupported");
    goto bad;
  }
  g->count = count;
  g->mesh_count = model->submesh_count;
  g->meshes = calloc(g->mesh_count, sizeof(*g->meshes));
  if (!g->meshes) {
    fail(error, "mesh registry allocation failed");
    goto bad;
  }
  for (uint32_t i = 0; i < g->count; i++) {
    const BkMorphTrack *track = bk_model_morph_track(g->morph, i);
    if (!track || track->submesh >= g->mesh_count) {
      fail(error, "invalid target");
      goto bad;
    }
    uint32_t target = track->submesh;
    if (!g->meshes[target])
      g->meshes[target] =
          bk_morph_mesh_create(model->submeshes + target, error);
    if (!g->meshes[target])
      goto bad;
    g->bindings[i] =
        bk_morph_binding_create(g->morph, i, g->meshes[target], error);
    if (!g->bindings[i])
      goto bad;
  }
  return g;
bad:
  bk_morph_group_destroy(g);
  return NULL;
}
int bk_morph_group_sample(BkMorphGroup *g, float time, const uint32_t *mask,
                          size_t mask_count, char error[256]) {
  if (!g || !isfinite(time) || time < 0 || time >= 2147483648.f ||
      (mask ? mask_count != g->count : mask_count != 0))
    return fail(error, "invalid time/mask");
  if (time == g->time)
    return 1;
  BkMorphBinding *bindings[64];
  BkMorphSample samples[64];
  size_t count = 0;
  for (uint32_t i = 0; i < g->count; i++)
    if (!mask || mask[i]) {
      bindings[count] = g->bindings[i];
      samples[count++] = (BkMorphSample){0, time, time, 0};
    }
  if (!bk_morph_bindings_apply(bindings, samples, count, error))
    return 0;
  g->time = time;
  return 1;
}
const BkMorphMesh *bk_morph_group_mesh(const BkMorphGroup *g,
                                       uint32_t submesh) {
  return g && submesh < g->mesh_count ? g->meshes[submesh] : NULL;
}
int bk_morph_group_blend(BkMorphGroup *g, float from, float to, float weight,
                         const uint32_t *mask, size_t mask_count,
                         char error[256]) {
  if (!g || !isfinite(from) || from < 0 || from >= 2147483648.f ||
      !isfinite(to) || to < 0 || to >= 2147483648.f || !isfinite(weight) ||
      (mask ? mask_count != g->count : mask_count != 0))
    return fail(error, "invalid blend/mask");
  BkMorphBinding *bindings[64];
  BkMorphSample samples[64];
  size_t count = 0;
  for (uint32_t i = 0; i < g->count; i++)
    if (!mask || mask[i]) {
      bindings[count] = g->bindings[i];
      samples[count++] = (BkMorphSample){1, from, to, weight};
    }
  return bk_morph_bindings_apply(bindings, samples, count, error);
}
uint32_t bk_morph_group_tracks(const BkMorphGroup *g) {
  return g ? g->count : 0;
}
float bk_morph_group_time(const BkMorphGroup *g) { return g ? g->time : 0; }
