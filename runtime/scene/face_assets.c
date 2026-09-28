#include "scene/face_assets.h"
#include "model/clip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkFaceAssets {
  BkFaceConfig config;
  BkModelMorph *morph;
  BkMorphBinding *bindings[2][4];
  BkMorphMesh *meshes[8];
  uint32_t submeshes[8], mesh_count;
};
static int fail(char *error, const char *reason) {
  snprintf(error, 256, "face assets: %s", reason);
  return 0;
}
void bk_face_assets_destroy(BkFaceAssets *f) {
  if (!f)
    return;
  for (unsigned g = 0; g < 2; g++)
    for (unsigned i = 0; i < 4; i++)
      bk_morph_binding_destroy(f->bindings[g][i]);
  for (unsigned i = 0; i < f->mesh_count; i++)
    bk_morph_mesh_destroy(f->meshes[i]);
  bk_model_morph_destroy(f->morph);
  free(f);
}
static BkFaceAssets *create(BkResourceStore *resources, const char *config_pack,
                            const char *config_name, const char *model_pack,
                            const BkModel *target, const char *target_filename,
                            unsigned eye_limit, char error[256]) {
  if (!resources || !target || !target_filename) {
    fail(error, "missing resource/target");
    return NULL;
  }
  BkFaceAssets *f = calloc(1, sizeof(*f));
  if (!f) {
    fail(error, "allocation failed");
    return NULL;
  }
  BkBlob data = {0};
  BkClipSet *clips = NULL;
  BkModel *source = NULL;
  if (bk_resources_read(resources, config_pack, config_name, &data, error) !=
          BK_RESOURCE_OK ||
      !bk_face_config_decode(data.data, data.size, &f->config, error))
    goto bad;
  bk_blob_free(&data);
  if (f->config.counts[0] > eye_limit || f->config.counts[1] > 3) {
    fail(error, "face slot count unsupported by selected native builder");
    goto bad;
  }
  if (bk_resources_read(resources, model_pack, f->config.source_clip, &data,
                        error) != BK_RESOURCE_OK)
    goto bad;
  clips = bk_clip_set_decode(data.data, data.size, error);
  bk_blob_free(&data);
  if (!clips ||
      bk_resources_read(resources, model_pack, bk_clip_model_name(clips), &data,
                        error) != BK_RESOURCE_OK)
    goto bad;
  if (bk_model_decode(data.data, data.size, &source, error) != BK_MODEL_OK)
    goto bad;
  bk_blob_free(&data);
  f->morph = bk_model_morph_create(source, error);
  if (!f->morph)
    goto bad;
  for (unsigned g = 0; g < 2; g++)
    for (unsigned i = 0; i < f->config.counts[g]; i++) {
      const BkFaceSlot *slot = &f->config.slots[g][i];
      uint32_t from, to;
      if (!bk_model_find_submesh(target, target_filename, slot->target, &to,
                                 error) ||
          !bk_model_find_submesh(source, bk_clip_model_name(clips),
                                 slot->source, &from, error))
        goto bad;
      uint32_t track = 0;
      while (track < bk_model_morph_count(f->morph) &&
             bk_model_morph_track(f->morph, track)->submesh != from)
        track++;
      if (track == bk_model_morph_count(f->morph)) {
        fail(error, "source submesh has no MORP track");
        goto bad;
      }
      unsigned mesh = 0;
      while (mesh < f->mesh_count && f->submeshes[mesh] != to)
        mesh++;
      if (mesh == f->mesh_count) {
        f->meshes[mesh] = bk_morph_mesh_create(&target->submeshes[to], error);
        if (!f->meshes[mesh])
          goto bad;
        f->submeshes[mesh] = to;
        f->mesh_count++;
      }
      f->bindings[g][i] =
          bk_morph_binding_create(f->morph, track, f->meshes[mesh], error);
      if (!f->bindings[g][i])
        goto bad;
      if (*slot->selection) {
        BkResourceResult result = bk_resources_read(
            resources, model_pack, slot->selection, &data, error);
        if (result == BK_RESOURCE_ERROR ||
            (result == BK_RESOURCE_OK &&
             !bk_morph_binding_vix(f->bindings[g][i], data.data, data.size,
                                   error)))
          goto bad;
        bk_blob_free(&data);
      }
    }
  bk_clip_set_destroy(clips);
  bk_model_destroy(source);
  error[0] = 0;
  return f;
bad:
  bk_blob_free(&data);
  bk_clip_set_destroy(clips);
  bk_model_destroy(source);
  bk_face_assets_destroy(f);
  return NULL;
}
BkFaceAssets *
bk_face_assets_create(BkResourceStore *resources, const char *config_pack,
                      const char *config_name, const char *model_pack,
                      const BkModel *target, const char *target_filename,
                      char error[256]) {
  return create(resources, config_pack, config_name, model_pack, target,
                target_filename, 3, error);
}
BkFaceAssets *
bk_face_assets_create_ending(BkResourceStore *resources,
                             const char *config_pack, const char *config_name,
                             const char *model_pack, const BkModel *target,
                             const char *target_filename, char error[256]) {
  return create(resources, config_pack, config_name, model_pack, target,
                target_filename, 4, error);
}
const BkFaceConfig *bk_face_assets_config(const BkFaceAssets *f) {
  return f ? &f->config : NULL;
}
const BkMorphMesh *bk_face_assets_mesh(const BkFaceAssets *f,
                                       uint32_t submesh) {
  if (f)
    for (unsigned i = 0; i < f->mesh_count; i++)
      if (f->submeshes[i] == submesh)
        return f->meshes[i];
  return NULL;
}
static int apply(BkFaceAssets *f, const BkFaceCommands *sets,
                 unsigned set_count, char *error) {
  if (!f || !sets || set_count > 3)
    return fail(error, "invalid commands");
  BkMorphBinding *bindings[48];
  BkMorphSample samples[48];
  size_t count = 0;
  for (unsigned s = 0; s < set_count; s++) {
    if (sets[s].count > 16)
      return fail(error, "invalid command count");
    for (unsigned i = 0; i < sets[s].count; i++) {
      const BkFaceCommand *c = &sets[s].commands[i];
      if (c->group >= 2 || c->index >= f->config.counts[c->group])
        return fail(error, "command outside face binding");
      bindings[count] = f->bindings[c->group][c->index];
      samples[count++] = (BkMorphSample){c->blend, c->from, c->to, c->weight};
    }
  }
  return bk_morph_bindings_apply(bindings, samples, count, error);
}
int bk_face_assets_apply(BkFaceAssets *f, const BkFaceCommands *commands,
                         char error[256]) {
  return apply(f, commands, 1, error);
}
int bk_face_assets_initialize(BkFaceAssets *f, BkFaceState *state,
                              const uint32_t clock_ms[4],
                              uint32_t *random_state, char error[256]) {
  if (!f || !state || !clock_ms || !random_state)
    return fail(error, "missing initialization state");
  BkFaceState next;
  uint32_t random = *random_state;
  BkFaceCommands commands[3];
  if (!bk_face_init(&next, f->config.counts[0], f->config.counts[1], error) ||
      !bk_face_eye_range(&next, 0, 9, &random, error) ||
      !bk_face_mouth_range(&next, 0, 9, error) ||
      !bk_face_transition_seconds(&next, .5f, error) ||
      !bk_face_request(&next, 0, clock_ms[0], error) ||
      !bk_face_blink(&next, 1000, clock_ms[1], &random, &commands[0], error))
    return 0;
  next.mouth = 1;
  if (!bk_face_mouth(&next, 100, clock_ms[2], &commands[1], error) ||
      !bk_face_mouth(&next, 0, clock_ms[3], &commands[2], error) ||
      !apply(f, commands, 3, error))
    return 0;
  *state = next;
  *random_state = random;
  return 1;
}
int bk_face_assets_step(BkFaceAssets *f, BkFaceState *state, int32_t expression,
                        float mouth_level, uint32_t timestamp_ms,
                        uint32_t request_clock_ms, uint32_t mouth_clock_ms,
                        uint32_t blink_clock_ms, uint32_t *random_state,
                        char error[256]) {
  if (!f || !state || !random_state ||
      state->eye_count != f->config.counts[0] ||
      state->mouth_count != f->config.counts[1])
    return fail(error, "controller counts/bindings disagree");
  BkFaceState next = *state;
  uint32_t random = *random_state;
  BkFaceCommands commands[2];
  if (!bk_face_request(&next, expression, request_clock_ms, error) ||
      !bk_face_mouth(&next, mouth_level, mouth_clock_ms, &commands[0], error) ||
      !bk_face_blink(&next, timestamp_ms, blink_clock_ms, &random, &commands[1],
                     error) ||
      !apply(f, commands, 2, error))
    return 0;
  *state = next;
  *random_state = random;
  return 1;
}
