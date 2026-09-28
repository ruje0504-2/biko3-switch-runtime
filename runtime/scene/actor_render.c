#include "scene/actor_render.h"
#include "core/matrix.h"
#include "model/draw_order.h"
#include "model/skin.h"
#include "model/static_model.h"
#include "model/triangles.h"
#include "scene/skin_upload.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct BkActorRender {
  BkRenderer *renderer;
  const BkModel *model;
  BkStaticModel *layout;
  BkModelSkin *skin;
  BkSkinPalette *palette;
  uint32_t *skin_entries;
  BkSkinMesh **skin_meshes;
  BkGpuMesh **meshes;
  BkTexture **textures, *white;
  uint32_t *texture_refs;
  BkTexture *eye_texture;
  BkTexture *surface_override;
  uint32_t surface_target;
  const BkEyeAssets *eyes;
  uint32_t eye_target, eye_selected;
  int eye_alpha;
  BkLitVertex *vertices;
  BkLitVertex *material_signatures;
  uint8_t *rigid_uploaded;
  int *alpha;
  uint32_t *hidden, *order, *traversal, *frame_begin, *frame_size;
  BkDrawKey *keys, *frame_keys;
  float *scratch, vp[16];
  uint64_t generation;
  BkActorRenderStats stats;
  int ready, order_ready;
  BkActorMeshCallback callback;
  void *callback_context;
};
void bk_actor_render_destroy(BkActorRender *a) {
  if (!a)
    return;
  if (a->model) {
    for (uint32_t i = 0; i < a->model->submesh_count; i++) {
      if (a->meshes)
        bk_mesh_destroy(a->renderer, a->meshes[i]);
      if (a->skin_meshes)
        bk_skin_mesh_destroy(a->skin_meshes[i]);
    }
    if (a->texture_refs && *a->texture_refs == 1 && a->textures)
      for (uint32_t i = 0; i < a->model->texture_count; i++)
        bk_texture_destroy(a->renderer, a->textures[i]);
  }
  if (a->texture_refs && !--*a->texture_refs) {
    bk_texture_destroy(a->renderer, a->white);
    bk_texture_destroy(a->renderer, a->eye_texture);
    free(a->textures);
    free(a->alpha);
    free(a->texture_refs);
  }
  bk_skin_palette_destroy(a->renderer, a->palette);
  free(a->skin_entries);
  bk_model_skin_destroy(a->skin);
  bk_static_model_destroy(a->layout);
  free(a->meshes);
  free(a->skin_meshes);
  free(a->vertices);
  free(a->material_signatures);
  free(a->rigid_uploaded);
  free(a->hidden);
  free(a->frame_begin);
  free(a->frame_size);
  free(a->order);
  free(a->traversal);
  free(a->keys);
  free(a->frame_keys);
  free(a->scratch);
  free(a);
}
static void convert(BkLitVertex *out, const BkModelVertex *v, uint32_t count,
                    const BkModelMaterial *m, const BkMaterialState *state) {
  for (uint32_t i = 0; i < count; i++) {
    out[i] = (BkLitVertex){
        .base = {v[i].position[0], v[i].position[1], v[i].position[2],
                 v[i].uv[0][0], v[i].uv[0][1], state->diffuse[0],
                 state->diffuse[1], state->diffuse[2], state->diffuse[3]},
        .power = m->power};
    memcpy(out[i].normal, v[i].normal, sizeof(out[i].normal));
    memcpy(out[i].ambient, m->ambient, sizeof(out[i].ambient));
    memcpy(out[i].emissive, m->emissive, sizeof(out[i].emissive));
    memcpy(out[i].specular, m->specular, sizeof(out[i].specular));
  }
}
static BkActorRender *create(BkRenderer *renderer, BkResourceStore *resources,
                             const char *pack, const BkModel *m,
                             const BkEyeAssets *eyes,
                             const BkActorRender *shared, char error[256]) {
  if (!renderer || (!shared && (!resources || !pack)) || !m ||
      (shared &&
       (!shared->texture_refs || *shared->texture_refs == UINT32_MAX))) {
    snprintf(error, 256, "invalid actor render inputs");
    return NULL;
  }
  BkActorRender *a = calloc(1, sizeof(*a));
  if (!a)
    goto oom;
  a->renderer = renderer;
  a->model = m;
  a->eyes = eyes;
  if (shared) {
    a->texture_refs = shared->texture_refs;
    ++*a->texture_refs;
    a->textures = shared->textures;
    a->alpha = shared->alpha;
    a->white = shared->white;
    a->eye_texture = shared->eye_texture;
    a->eye_alpha = shared->eye_alpha;
    a->surface_target = shared->surface_target;
    a->surface_override = shared->surface_override;
  } else {
    a->texture_refs = malloc(sizeof(*a->texture_refs));
    if (!a->texture_refs)
      goto oom;
    *a->texture_refs = 1;
  }
  a->eye_target =
      eyes ? bk_eye_assets_binding(eyes)->target_submesh : BK_MODEL_NONE;
  if (a->eye_target != BK_MODEL_NONE && a->eye_target >= m->submesh_count) {
    snprintf(error, 256, "eye texture target does not belong to actor model");
    goto fail;
  }
  const BkImage *eye_image = bk_eye_assets_image(eyes, 1, &a->eye_alpha);
  if (eye_image && !shared) {
    a->eye_texture =
        bk_texture_create_sampled(renderer, eye_image, BK_WRAP_REPEAT, error);
    if (!a->eye_texture)
      goto fail;
  }
  if (!shared) {
    a->textures =
        calloc(m->texture_count ? m->texture_count : 1, sizeof(*a->textures));
    a->alpha =
        calloc(m->texture_count ? m->texture_count : 1, sizeof(*a->alpha));
  }
  a->meshes = calloc(m->submesh_count, sizeof(*a->meshes));
  a->skin_entries = malloc((size_t)m->submesh_count * sizeof(*a->skin_entries));
  if (!a->skin_entries)
    goto oom;
  for (uint32_t i = 0; i < m->submesh_count; i++)
    a->skin_entries[i] = BK_MODEL_NONE;
  a->skin_meshes = calloc(m->submesh_count, sizeof(*a->skin_meshes));
  a->material_signatures =
      calloc(m->submesh_count, sizeof(*a->material_signatures));
  a->rigid_uploaded = calloc(m->submesh_count, sizeof(*a->rigid_uploaded));
  a->hidden = calloc(m->frame_count, sizeof(*a->hidden));
  a->frame_begin = calloc(m->frame_count, sizeof(*a->frame_begin));
  a->frame_size = calloc(m->frame_count, sizeof(*a->frame_size));
  if (!a->textures || !a->alpha || !a->meshes || !a->skin_meshes ||
      !a->hidden || !a->frame_begin || !a->frame_size ||
      !a->material_signatures || !a->rigid_uploaded)
    goto oom;
  for (uint32_t i = 0; !shared && i < m->texture_count; i++) {
    BkModelTextureImage t;
    if (!bk_model_texture_load(m, i, resources, pack, &t, error))
      goto fail;
    a->alpha[i] = t.alpha_hint;
    a->textures[i] =
        bk_texture_create_sampled(renderer, &t.image, BK_WRAP_REPEAT, error);
    bk_image_free(&t.image);
    if (!a->textures[i])
      goto fail;
  }
  a->layout = bk_static_model_create(m, a->alpha, error);
  if (!a->layout)
    goto fail;
  uint32_t count = a->layout->instance_count, max_vertices = 0;
  a->order = malloc((size_t)count * sizeof(*a->order));
  a->traversal = malloc((size_t)count * sizeof(*a->traversal));
  a->keys = malloc((size_t)count * sizeof(*a->keys));
  a->frame_keys = malloc((size_t)count * sizeof(*a->frame_keys));
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t f = a->layout->instances[i].frame;
    if (!a->frame_size[f]++)
      a->frame_begin[f] = i;
  }
  a->scratch = malloc((size_t)count * sizeof(*a->scratch));
  for (uint32_t i = 0; i < m->submesh_count; i++)
    if (m->submeshes[i].vertex_count > max_vertices)
      max_vertices = m->submeshes[i].vertex_count;
  a->vertices = malloc((size_t)max_vertices * sizeof(*a->vertices));
  if (!a->order || !a->traversal || !a->keys || !a->frame_keys || !a->scratch ||
      !a->vertices)
    goto oom;
  if (bk_model_chunk(m, "ENVL")) {
    a->skin = bk_model_skin_create(m, error);
    if (!a->skin)
      goto fail;
    if (bk_model_skin_count(a->skin) && !getenv("BK_CPU_SKINNING")) {
      a->palette = bk_skin_palette_create(renderer, m->frame_count, error);
      if (!a->palette)
        goto fail;
    }
    for (uint32_t i = 0; i < bk_model_skin_count(a->skin); i++) {
      const BkSkinEntry *entry = bk_model_skin_entry(a->skin, i);
      a->skin_entries[entry->submesh] = i;
      if (!a->palette) {
        a->skin_meshes[entry->submesh] = bk_skin_mesh_create(
            a->skin, i, &m->submeshes[entry->submesh], error);
        if (!a->skin_meshes[entry->submesh])
          goto fail;
      }
    }
  }
  for (uint32_t i = 0; i < m->submesh_count; i++) {
    const BkModelSubmesh *sub = &m->submeshes[i];
    convert(a->vertices, sub->vertices, sub->vertex_count,
            &m->materials[sub->material_index], &a->layout->parts[i].material);
    BkModelTriangles triangles;
    if (!bk_model_triangles(m, i, &triangles, error))
      goto fail;
    a->meshes[i] =
        bk_lit_mesh_create(renderer, a->vertices, sub->vertex_count,
                           triangles.indices, triangles.count, error);
    bk_model_triangles_free(&triangles);
    if (!a->meshes[i])
      goto fail;
    if (a->palette && a->skin_entries[i] != BK_MODEL_NONE &&
        !bk_skin_upload(renderer, a->meshes[i], a->palette, a->skin,
                        a->skin_entries[i], error))
      goto fail;
    convert(&a->material_signatures[i], &(BkModelVertex){0}, 1,
            &m->materials[sub->material_index], &a->layout->parts[i].material);
    a->rigid_uploaded[i] = 1;
  }
  uint8_t white[] = {255, 255, 255, 255};
  BkImage image = {1, 1, white};
  if (!shared)
    a->white = bk_texture_create(renderer, &image, error);
  if (!a->white)
    goto fail;
  return a;
oom:
  snprintf(error, 256, "actor render allocation failed");
fail:
  bk_actor_render_destroy(a);
  return NULL;
}
BkActorRender *bk_actor_render_create(BkRenderer *renderer,
                                      BkResourceStore *resources,
                                      const char *pack, const BkModel *model,
                                      const BkEyeAssets *eyes,
                                      char error[256]) {
  return create(renderer, resources, pack, model, eyes, NULL, error);
}
BkActorRender *bk_actor_render_create_view(const BkActorRender *source,
                                           char error[256]) {
  if (!source) {
    snprintf(error, 256, "missing actor view source");
    return NULL;
  }
  return create(source->renderer, NULL, NULL, source->model, source->eyes,
                source, error);
}
static int prepare(BkActorRender *a, const BkActorPose *pose,
                   const BkMaterialPose *materials, const BkFaceAssets *face,
                   const BkMorphGroup *group, const float view[16],
                   const float projection[16], char error[256]) {
  if (!a)
    goto invalid;
  a->ready = a->order_ready = 0;
  ++a->generation;
  memset(&a->stats, 0, sizeof(a->stats));
  if (!pose || bk_actor_pose_model(pose) != a->model || !view || !projection)
    goto invalid;
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(view[i]) || !isfinite(projection[i]))
      goto invalid;
  bk_matrix_multiply(a->vp, view, projection);
  const BkModel *m = a->model;
  a->eye_selected = bk_eye_assets_selected(a->eyes);
  for (uint32_t i = 0; i < m->frame_count; i++) {
    const float *world = bk_actor_pose_frame(pose, i);
    if (!world || !bk_actor_pose_hidden(pose, i, &a->hidden[i]))
      goto invalid;
    memcpy(a->layout->world + i * 16, world, 16 * sizeof(float));
  }
  if (a->palette &&
      !bk_skin_palette_update(a->renderer, a->palette, a->layout->world,
                              m->frame_count, error))
    return 0;
  for (uint32_t i = 0; i < m->submesh_count; i++) {
    a->stats.skinned_parts += a->skin_entries[i] != BK_MODEL_NONE;
    const BkModelSubmesh *sub = &m->submeshes[i];
    const BkModelMaterial *mat =
        materials ? bk_material_pose_material(materials, sub->material_index)
                  : &m->materials[sub->material_index];
    BkStaticPart *part = &a->layout->parts[i];
    int alpha = sub->texture_count ? a->alpha[sub->texture_indices[0]] : 0;
    if (i == a->eye_target && a->eye_selected == 1)
      alpha = a->eye_alpha;
    if (!bk_material_state(mat, alpha, &part->material, error))
      return 0;
    float opacity = part->material.encoded_alpha;
    int translucent = !(opacity > .999999f && opacity < 1.000001f);
    part->sorted = sub->texture_count && (translucent || alpha);
    part->depth_write = !(sub->texture_count && translucent);
    const BkMorphMesh *morph =
        face ? bk_face_assets_mesh(face, i) : bk_morph_group_mesh(group, i);
    if (morph && bk_morph_mesh_count(morph) != sub->vertex_count)
      goto invalid;
    BkLitVertex signature;
    convert(&signature, &(BkModelVertex){0}, 1, mat, &part->material);
    /* Rigid geometry stays in local space: changes to the node, camera,
     * light, hidden state or sort key do not change its vertex buffer.
     * Material edits must still upload. Dropping a prior MORP also uploads
     * once to restore the authored geometry. ENVL remains world-space. */
    if (!morph && !a->skin_meshes[i] && a->rigid_uploaded[i] &&
        !memcmp(&signature, &a->material_signatures[i], sizeof(signature)))
      continue;
    const BkModelVertex *vertices =
        morph ? bk_morph_mesh_vertices(morph) : sub->vertices;
    if (a->skin_meshes[i]) {
      if (!bk_skin_mesh_apply(a->skin_meshes[i], a->layout->world,
                              (size_t)m->frame_count * 16, vertices,
                              sub->vertex_count, error))
        return 0;
      vertices = bk_skin_mesh_vertices(a->skin_meshes[i]);
    }
    a->stats.morph_parts += morph != NULL;
    convert(a->vertices, vertices, sub->vertex_count, mat, &part->material);
    if (!bk_lit_mesh_update(a->renderer, a->meshes[i], a->vertices,
                            sub->vertex_count, error))
      return 0;
    a->stats.uploaded_vertices += sub->vertex_count;
    a->material_signatures[i] = signature;
    a->rigid_uploaded[i] = !morph && !a->skin_meshes[i];
  }
  /* Retain raw depth-first insertion order; a later cross-model flush must
   * not concatenate already sorted per-model queues. Alpha-zero entries
   * remain queued, as in native material dispatch. */
  for (uint32_t i = 0; i < a->layout->instance_count; ++i) {
    const BkStaticInstance *inst = &a->layout->instances[i];
    const BkStaticPart *p = &a->layout->parts[inst->submesh];
    if (a->hidden[inst->frame])
      continue;
    const BkModelSubmesh *sub = &m->submeshes[inst->submesh];
    BkTexture *texture =
        sub->texture_count ? a->textures[sub->texture_indices[0]] : NULL;
    if (inst->submesh == a->eye_target && a->eye_selected == 1)
      texture = a->eye_texture;
    BkDrawKey key = {.sorted = p->sorted,
                     .priority = p->priority,
                     .texture_key = bk_texture_sort_key(texture)};
    if (!bk_draw_distance(&key.distance, a->layout->world + inst->frame * 16,
                          view, p->sort_bias))
      goto invalid;
    a->frame_keys[i] = key;
    /* Global visits carry their own ancestry after reparenting. Only legacy
     * whole-model draws still apply the immutable asset ancestor chain. */
    uint32_t node = m->frames[inst->frame].parent_index;
    while (node != BK_MODEL_NONE && !a->hidden[node])
      node = m->frames[node].parent_index;
    if (node != BK_MODEL_NONE)
      continue;
    uint32_t k = a->stats.submitted_instances++;
    a->traversal[k] = i;
    a->keys[k] = key;
    a->stats.visible_instances += p->material.visible != 0;
  }
  a->ready = 1;
  return 1;
invalid:
  snprintf(error, 256, "invalid actor render snapshot/geometry");
  return 0;
}
int bk_actor_render_prepare(BkActorRender *a, const BkActorPose *pose,
                            const BkMaterialPose *materials,
                            const BkFaceAssets *face, const float view[16],
                            const float projection[16], char error[256]) {
  return prepare(a, pose, materials, face, NULL, view, projection, error);
}
int bk_actor_render_prepare_morph(BkActorRender *a, const BkActorPose *pose,
                                  const BkMaterialPose *materials,
                                  const BkMorphGroup *morph,
                                  const float view[16],
                                  const float projection[16], char error[256]) {
  return prepare(a, pose, materials, NULL, morph, view, projection, error);
}
BkGpuMesh *bk_actor_render_mesh(BkActorRender *a, const BkModel *model,
                                uint32_t submesh) {
  return a && model == a->model && submesh < model->submesh_count
             ? a->meshes[submesh]
             : NULL;
}
uint64_t bk_actor_render_revision(const BkActorRender *a) {
  return a && a->ready ? a->generation : 0;
}
int bk_actor_render_mesh_callback(BkActorRender *a,
                                  BkActorMeshCallback callback, void *context,
                                  char error[256]) {
  if (!a || !context || (a->callback && a->callback_context != context)) {
    snprintf(error, 256, "invalid or occupied actor callback owner");
    return 0;
  }
  a->callback = callback;
  a->callback_context = callback ? context : NULL;
  a->ready = 0;
  ++a->generation;
  return 1;
}
int bk_actor_render_texture_surface(BkActorRender *a, uint32_t index,
                                    BkTexture *texture, char error[256]) {
  if (!a ||
      (texture && (index >= a->model->texture_count ||
                   !bk_texture_owned_by(texture, a->renderer))) ||
      (!texture && index != BK_MODEL_NONE)) {
    snprintf(error, 256, "invalid actor borrowed texture surface");
    return 0;
  }
  a->ready = 0;
  ++a->generation;
  a->surface_target = index;
  a->surface_override = texture;
  return 1;
}
static int draw_instance(BkActorRender *a, uint32_t index, BkLightSet *lights,
                         char error[256]) {
  const BkBlend blends[] = {BK_BLEND_ALPHA, BK_BLEND_ADDITIVE,
                            BK_BLEND_INVERSE_COLOR};
  const BkStaticInstance *inst = &a->layout->instances[index];
  const BkStaticPart *p = &a->layout->parts[inst->submesh];
  if (a->callback && !a->callback(a->callback_context, inst->submesh, error))
    return 0;
  if (!p->material.visible)
    return 1;
  const BkModelSubmesh *sub = &a->model->submeshes[inst->submesh];
  BkTexture *texture =
      sub->texture_count ? a->textures[sub->texture_indices[0]] : a->white;
  if (sub->texture_count && a->surface_override &&
      sub->texture_indices[0] == a->surface_target)
    texture = a->surface_override;
  if (inst->submesh == a->eye_target && a->eye_selected == 1)
    texture = a->eye_texture;
  const float *world = (a->skin_entries[inst->submesh] != BK_MODEL_NONE)
                           ? bk_identity
                           : a->layout->world + inst->frame * 16;
  float mvp[16];
  bk_matrix_multiply(mvp, world, a->vp);
  if (!bk_renderer_draw_lit_mesh(
          a->renderer, texture, a->meshes[inst->submesh], lights, mvp, world,
          (BkDrawState){blends[p->material.blend], p->depth_write,
                        BK_CULL_COUNTER_CLOCKWISE},
          error)) {
    char cause[256];
    memcpy(cause, error, sizeof(cause));
    snprintf(error, 256, "actor frame %u part %u: %.185s", inst->frame,
             inst->submesh, cause);
    return 0;
  }
  return 1;
}
int bk_actor_render_draw(BkActorRender *a, BkLightSet *lights,
                         char error[256]) {
  if (!a || !a->ready || !lights) {
    snprintf(error, 256, "actor render has no complete prepared frame/lights");
    return 0;
  }
  if (!a->order_ready) {
    if (!bk_draw_order(a->keys, a->stats.submitted_instances, a->order,
                       a->scratch))
      return 0;
    a->order_ready = 1;
  }
  for (uint32_t i = 0; i < a->stats.submitted_instances; ++i)
    if (!draw_instance(a, a->traversal[a->order[i]], lights, error))
      return 0;
  return 1;
}
int bk_actor_render_stats(const BkActorRender *a, BkActorRenderStats *out) {
  if (!a || !a->ready || !out)
    return 0;
  *out = a->stats;
  return 1;
}
typedef struct {
  BkActorRender *actor;
  uint64_t generation;
  uint32_t instance, source;
} BatchEntry;
struct BkActorRenderBatch {
  BkRenderer *renderer;
  BatchEntry *entries;
  BkDrawOrderCache *order_cache;
  BkDrawKey *keys;
  uint32_t *order, capacity, count;
  float *scratch;
  int ready;
};
void bk_actor_render_batch_destroy(BkActorRenderBatch *b) {
  if (!b)
    return;
  bk_draw_order_cache_destroy(b->order_cache);
  free(b->entries);
  free(b->keys);
  free(b->order);
  free(b->scratch);
  free(b);
}
BkActorRenderBatch *bk_actor_render_batch_create(BkRenderer *r,
                                                 uint32_t capacity,
                                                 char error[256]) {
  if (!r || !capacity || capacity > BK_DRAW_QUEUE_LIMIT) {
    snprintf(error, 256, "invalid actor batch capacity/renderer");
    return NULL;
  }
  BkActorRenderBatch *b = calloc(1, sizeof(*b));
  if (b) {
    b->renderer = r;
    b->capacity = capacity;
    b->order_cache = bk_draw_order_cache_create(capacity);
    b->entries = malloc(capacity * sizeof(*b->entries));
    b->keys = malloc(capacity * sizeof(*b->keys));
    b->order = malloc(capacity * sizeof(*b->order));
    b->scratch = malloc(capacity * sizeof(*b->scratch));
    if (b->order_cache && b->entries && b->keys && b->order && b->scratch)
      return b;
  }
  bk_actor_render_batch_destroy(b);
  snprintf(error, 256, "actor batch allocation failed");
  return NULL;
}
int bk_actor_render_batch_prepare(BkActorRenderBatch *b,
                                  BkActorRender *const *actors, uint32_t count,
                                  char error[256]) {
  if (!b)
    goto invalid;
  b->ready = 0;
  b->count = 0;
  if (count && !actors)
    goto invalid;
  for (uint32_t i = 0; i < count; ++i) {
    BkActorRender *a = actors[i];
    if (!a || !a->ready || a->renderer != b->renderer ||
        a->stats.submitted_instances > b->capacity - b->count)
      goto invalid;
    for (uint32_t k = 0; k < a->stats.submitted_instances; ++k) {
      uint32_t j = b->count++;
      b->entries[j] = (BatchEntry){a, a->generation, a->traversal[k], i};
      b->keys[j] = a->keys[k];
    }
  }
  if (!bk_draw_order_cached(b->order_cache, b->keys, b->count, b->order,
                            b->scratch))
    goto invalid;
  b->ready = 1;
  return 1;
invalid:
  snprintf(error, 256,
           "invalid/unprepared actors or actor batch capacity exceeded");
  return 0;
}
int bk_actor_render_batch_prepare_visits(BkActorRenderBatch *b,
                                         const BkActorRenderVisit *visits,
                                         uint32_t count, char error[256]) {
  if (!b)
    goto invalid;
  b->ready = 0;
  b->count = 0;
  if (count && !visits)
    goto invalid;
  for (uint32_t i = 0; i < count; ++i) {
    BkActorRender *a = visits[i].actor;
    uint32_t f = visits[i].frame;
    if (!a || !a->ready || a->renderer != b->renderer ||
        f >= a->model->frame_count)
      goto invalid;
    if (a->hidden[f])
      continue;
    if (a->frame_size[f] > b->capacity - b->count)
      goto invalid;
    for (uint32_t n = 0; n < a->frame_size[f]; ++n) {
      uint32_t k = a->frame_begin[f] + n, j = b->count++;
      b->entries[j] = (BatchEntry){a, a->generation, k, i};
      b->keys[j] = a->frame_keys[k];
    }
  }
  if (!bk_draw_order_cached(b->order_cache, b->keys, b->count, b->order,
                            b->scratch))
    goto invalid;
  b->ready = 1;
  return 1;
invalid:
  snprintf(error, 256,
           "invalid/unprepared frame visits or actor batch capacity exceeded");
  return 0;
}
static int batch_valid(const BkActorRenderBatch *b) {
  if (!b || !b->ready)
    return 0;
  for (uint32_t i = 0; i < b->count; ++i) {
    const BatchEntry *e = &b->entries[i];
    if (!e->actor->ready || e->actor->generation != e->generation)
      return 0;
  }
  return 1;
}
int bk_actor_render_batch_draw(BkActorRenderBatch *b, BkLightSet *lights,
                               char error[256]) {
  if (!lights || !batch_valid(b)) {
    snprintf(error, 256,
             "actor batch has missing or stale prepared snapshots/lights");
    return 0;
  }
  for (uint32_t i = 0; i < b->count; ++i) {
    const BatchEntry *e = &b->entries[b->order[i]];
    if (!draw_instance(e->actor, e->instance, lights, error))
      return 0;
  }
  return 1;
}
uint32_t bk_actor_render_batch_count(const BkActorRenderBatch *b) {
  return batch_valid(b) ? b->count : 0;
}
int bk_actor_render_batch_item(const BkActorRenderBatch *b, uint32_t i,
                               uint32_t *source, uint32_t *frame,
                               uint32_t *submesh) {
  if (!source || !frame || !submesh || !batch_valid(b) || i >= b->count)
    return 0;
  const BatchEntry *e = &b->entries[b->order[i]];
  const BkStaticInstance *inst = &e->actor->layout->instances[e->instance];
  *source = e->source;
  *frame = inst->frame;
  *submesh = inst->submesh;
  return 1;
}
