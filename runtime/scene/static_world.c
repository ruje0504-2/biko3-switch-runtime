#include "core/matrix.h"
#include "model/environment.h"
#include "model/playback.h"
#include "model/static_model.h"
#include "model/triangles.h"
#include "scene/inspection_camera.h"
#include "scene/scene_internal.h"
#include "ui/debug_overlay.h"
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkScene base;
  BkRenderer *renderer;
  BkModel *model;
  BkStaticModel *static_model;
  BkModelEnvironment *environment;
  BkLightSet *lights;
  BkTexture **textures, *white, *overlay;
  BkGpuMesh **meshes;
  uint32_t *order, *texture_keys;
  float *distances;
  BkInspectionCamera camera;
  BkModel *camera_model;
  BkModelPlayback *playback;
  uint32_t camera_node;
  BkClipSet *clips;
} StaticScene;
static void destroy(BkScene *base) {
  StaticScene *s = (StaticScene *)base;
  if (s->model) {
    if (s->meshes)
      for (uint32_t i = 0; i < s->model->submesh_count; i++)
        bk_mesh_destroy(s->renderer, s->meshes[i]);
    if (s->textures)
      for (uint32_t i = 0; i < s->model->texture_count; i++)
        bk_texture_destroy(s->renderer, s->textures[i]);
  }
  bk_texture_destroy(s->renderer, s->overlay);
  bk_texture_destroy(s->renderer, s->white);
  bk_light_set_destroy(s->renderer, s->lights);
  bk_model_environment_destroy(s->environment);
  bk_static_model_destroy(s->static_model);
  bk_model_destroy(s->model);
  bk_model_playback_destroy(s->playback);
  bk_clip_set_destroy(s->clips);
  bk_model_destroy(s->camera_model);
  free(s->textures);
  free(s->meshes);
  free(s->order);
  free(s->texture_keys);
  free(s->distances);
  free(s);
}
static int sample_clip(StaticScene *s, float seconds, char error[256]) {
  return bk_model_playback_advance(s->playback, seconds, NULL, error);
}
static int step(BkScene *base, double seconds, const BkInput *input,
                char error[256]) {
  StaticScene *s = (StaticScene *)base;
  if (!s->playback)
    return bk_inspection_step(&s->camera, seconds, input, error);
  if (!isfinite(seconds) || seconds <= 0 || seconds > 1) {
    snprintf(error, 256, "invalid camera track step");
    return 0;
  }
  /* 0x4bdc12 passes half the simulation seconds to the clip scheduler.
   * Actor anchoring, aiming and gameplay dispatch are still separate work. */
  if (input->pressed & BK_BUTTON_CONFIRM) {
    if (!bk_model_playback_select(s->playback, 0, 1, error))
      return 0;
    return sample_clip(s, 0, error);
  }
  return sample_clip(s, (float)(seconds * .5), error);
}
static int draw(BkScene *base, const BkSceneFrame *frame, char error[256]) {
  (void)frame;
  StaticScene *s = (StaticScene *)base;
  float camera_world[16], view[16], projection[16], vp[16], mvp[16];
  /* Main-loop setup 0x4662dd..0306 overrides earlier device defaults:
   * FOV 1, H/W .75, clip .5..126384. */
  const BkCameraLens lens = {1, .75f, .5f, 126384};
  unsigned width, height;
  BkViewport viewport;
  bk_renderer_extent(s->renderer, &width, &height);
  if (s->playback)
    memcpy(camera_world, bk_model_playback_frame(s->playback, s->camera_node),
           64);
  else if (!bk_inspection_world(&s->camera, camera_world)) {
    snprintf(error, 256, "invalid inspection camera");
    return 0;
  }
  if (!bk_camera_view(view, camera_world) ||
      !bk_camera_projection(projection, &lens) ||
      !bk_camera_fit(&viewport, width, height, 4, 3)) {
    snprintf(error, 256, "invalid camera matrix");
    return 0;
  }
  if (!bk_renderer_viewport(s->renderer, &viewport, error))
    return 0;
  bk_matrix_multiply(vp, view, projection);
  BkDepthTransform depth;
  if (!bk_renderer_depth_transform(&depth, view, projection, error))
    return 0;
  if (!bk_static_model_order_textures(s->static_model, view, s->texture_keys,
                                      s->order, s->distances, error))
    return 0;
  const BkBlend blends[] = {BK_BLEND_ALPHA, BK_BLEND_ADDITIVE,
                            BK_BLEND_INVERSE_COLOR};
  for (uint32_t i = 0; i < s->static_model->instance_count; i++) {
    BkStaticInstance instance = s->static_model->instances[s->order[i]];
    BkStaticPart *part = &s->static_model->parts[instance.submesh];
    if (!part->material.visible)
      continue;
    BkModelSubmesh *sub = &s->model->submeshes[instance.submesh];
    BkTexture *texture =
        sub->texture_count ? s->textures[sub->texture_indices[0]] : s->white;
    bk_matrix_multiply(mvp, s->static_model->world + instance.frame * 16, vp);
    if (!bk_renderer_draw_lit_mesh_projected(
            s->renderer, texture, s->meshes[instance.submesh], s->lights, mvp,
            s->static_model->world + instance.frame * 16, &depth,
            (BkDrawState){blends[part->material.blend], part->depth_write,
                          BK_CULL_COUNTER_CLOCKWISE},
            error))
      return 0;
  }
  /* Constant clip-space overlay; unaffected by camera/depth of the scene. */
  if (!bk_renderer_viewport(s->renderer, NULL, error))
    return 0;
  const float y = 656.0f / 360 - 1;
  BkVertex v[6] = {{-1, y, 0, 0, 0, 1, 1, 1, 1}, {1, y, 0, 1, 0, 1, 1, 1, 1},
                   {1, 1, 0, 1, 1, 1, 1, 1, 1},  {-1, y, 0, 0, 0, 1, 1, 1, 1},
                   {1, 1, 0, 1, 1, 1, 1, 1, 1},  {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  return bk_renderer_draw(s->renderer, s->overlay, v, 6, bk_identity, error);
}
static BkScene *create(const BkSceneServices *services, int clip_camera,
                       char error[256]) {
  StaticScene *s = calloc(1, sizeof(*s));
  BkBlob blob = {0};
  BkImage image = {0};
  int *alpha = NULL;
  BkLitVertex *vertices = NULL;
  if (!s) {
    snprintf(error, 256, "static scene allocation failed");
    return NULL;
  }
  s->base = (BkScene){step, draw, destroy};
  s->renderer = services->renderer;
  if (bk_resources_read(services->resources, "bk3_03", "m01_04.x", &blob,
                        error) != BK_RESOURCE_OK ||
      bk_model_decode(blob.data, blob.size, &s->model, error) != BK_MODEL_OK)
    goto fail;
  bk_blob_free(&blob);
  if (clip_camera) {
    if (bk_resources_read(services->resources, "bk3_04", "cam00_02.xan", &blob,
                          error) != BK_RESOURCE_OK)
      goto fail;
    s->clips = bk_clip_set_decode(blob.data, blob.size, error);
    bk_blob_free(&blob);
    if (!s->clips)
      goto fail;
    if (bk_resources_read(services->resources, "bk3_04",
                          bk_clip_model_name(s->clips), &blob,
                          error) != BK_RESOURCE_OK ||
        bk_model_decode(blob.data, blob.size, &s->camera_model, error) !=
            BK_MODEL_OK)
      goto fail;
    bk_blob_free(&blob);
    s->playback = bk_model_playback_create(s->camera_model, s->clips, 0, error);
    if (!s->playback || !bk_model_playback_select(s->playback, 0, 1, error))
      goto fail;
    if (!bk_model_find_frame(s->camera_model, "Cam_AUTO", &s->camera_node,
                             error))
      goto fail;
    if (!sample_clip(s, 0, error))
      goto fail;
  }
  BkModel *m = s->model;
  s->textures = calloc(m->texture_count, sizeof(*s->textures));
  s->texture_keys = calloc(m->texture_count, sizeof(*s->texture_keys));
  s->meshes = calloc(m->submesh_count, sizeof(*s->meshes));
  alpha = calloc(m->texture_count, sizeof(*alpha));
  if (!s->textures || !s->texture_keys || !s->meshes || !alpha)
    goto oom;
  uint64_t texture_bytes = 0, uploaded_vertices = 0, uploaded_indices = 0;
  for (uint32_t i = 0; i < m->texture_count; i++) {
    BkModelTextureImage t;
    if (!bk_model_texture_load(m, i, services->resources, "bk3_03", &t, error))
      goto fail;
    image = t.image;
    alpha[i] = t.alpha_hint;
    texture_bytes += (uint64_t)image.width * image.height * 4;
    s->textures[i] =
        bk_texture_create_sampled(s->renderer, &image, BK_WRAP_REPEAT, error);
    bk_image_free(&image);
    if (!s->textures[i])
      goto fail;
    s->texture_keys[i] = bk_texture_sort_key(s->textures[i]);
  }
  s->static_model = bk_static_model_create(m, alpha, error);
  free(alpha);
  alpha = NULL;
  if (!s->static_model)
    goto fail;
  s->environment =
      bk_model_environment_create(m, s->static_model->world, error);
  if (!s->environment)
    goto fail;
  s->lights =
      bk_light_set_create(s->renderer, &s->environment->lighting, error);
  if (!s->lights)
    goto fail;
  uint32_t count = s->static_model->instance_count;
  s->order = malloc((size_t)count * sizeof(*s->order));
  s->distances = malloc((size_t)count * sizeof(*s->distances));
  if (!s->order || !s->distances)
    goto oom;
  unsigned uploaded = 0, hidden = 0;
  for (uint32_t i = 0; i < m->submesh_count; i++) {
    BkModelSubmesh *sub = &m->submeshes[i];
    BkMaterialState *mat = &s->static_model->parts[i].material;
    if (!mat->visible) {
      hidden++;
      continue;
    }
    vertices = malloc((size_t)sub->vertex_count * sizeof(*vertices));
    if (!vertices)
      goto oom;
    for (uint32_t j = 0; j < sub->vertex_count; j++) {
      BkModelVertex *v = &sub->vertices[j];
      vertices[j] = (BkLitVertex){0};
      vertices[j].base =
          (BkVertex){v->position[0],  v->position[1],  v->position[2],
                     v->uv[0][0],     v->uv[0][1],     mat->diffuse[0],
                     mat->diffuse[1], mat->diffuse[2], mat->diffuse[3]};
      const BkModelMaterial *material = &m->materials[sub->material_index];
      for (unsigned k = 0; k < 3; k++) {
        vertices[j].normal[k] = v->normal[k];
        vertices[j].ambient[k] = material->ambient[k];
        vertices[j].emissive[k] = material->emissive[k];
      }
    }
    BkModelTriangles triangles;
    if (!bk_model_triangles(m, i, &triangles, error))
      goto fail;
    s->meshes[i] =
        bk_lit_mesh_create(s->renderer, vertices, sub->vertex_count,
                           triangles.indices, triangles.count, error);
    uploaded_indices += triangles.count;
    bk_model_triangles_free(&triangles);
    free(vertices);
    vertices = NULL;
    if (!s->meshes[i])
      goto fail;
    uploaded++;
    uploaded_vertices += sub->vertex_count;
  }
  uint8_t white[4] = {255, 255, 255, 255};
  BkImage solid = {1, 1, white};
  s->white = bk_texture_create(s->renderer, &solid, error);
  if (!s->white)
    goto fail;
  if (!bk_debug_banner_lines(
          &image,
          clip_camera ? "BIKO3 - XAN CAMERA CLIP - ACTOR BINDING PENDING"
                      : "BIKO3 - ORIGINAL LENS 4:3 - INSPECTION CAMERA",
          clip_camera
              ? "A RESTART  X INSPECTION  Y ACTOR  B TITLE - ASSET DIAGNOSTIC"
              : "L MOVE  R LOOK  A RESET  X CAMERA CLIP  Y ACTOR  B TITLE - NO "
                "GAMEPLAY",
          error))
    goto fail;
  s->overlay = bk_texture_create(s->renderer, &image, error);
  bk_image_free(&image);
  if (!s->overlay)
    goto fail;
  bk_inspection_reset(&s->camera);
  FILE *log = services->log ? services->log : stderr;
  fprintf(log,
          "Static office: %u frames, %u instances, %u first / %u sorted; %u "
          "GPU parts, %u hidden parts\n",
          m->frame_count, count, s->static_model->opaque_count,
          count - s->static_model->opaque_count, uploaded, hidden);
  fprintf(log,
          "Static uploads: %u textures, %" PRIu64 " texture bytes; %" PRIu64
          " vertices, %" PRIu64 " indices\n",
          m->texture_count, texture_bytes, uploaded_vertices, uploaded_indices);
  fprintf(log, "Lighting: %u ambient, %u points; fog disabled by model data.\n",
          s->environment->ambient_count, s->environment->lighting.point_count);
  fprintf(log,
          "Original lens: FOV 1 rad, H/W .75, clip .5..126384; fitted 4:3.\n");
  fprintf(log,
          "Office uses static geometry and authored zero-specular materials; "
          "enabled fog and gameplay are not implemented.\n");
  if (clip_camera)
    fprintf(log, "XAN camera clip: bk3_04/cam00_02.xan slot 0, Cam_AUTO. "
                 "Clip 151..166 over 400 timeline ticks; "
                 "actor anchor, aim and gameplay dispatch NOT applied.\n");
  fflush(log);
  return &s->base;
oom:
  snprintf(error, 256, "static scene allocation failed");
fail:
  free(alpha);
  free(vertices);
  bk_blob_free(&blob);
  bk_image_free(&image);
  destroy(&s->base);
  return NULL;
}

BkScene *bk_static_world_create(const BkSceneServices *services,
                                char error[256]) {
  return create(services, 0, error);
}
BkScene *bk_camera_track_create(const BkSceneServices *services,
                                char error[256]) {
  return create(services, 1, error);
}
