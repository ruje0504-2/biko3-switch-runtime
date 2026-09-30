/* Actual background glow/wall geometry, isolated with constant colors.
 * CPU double ray intersections establish visibility independently of GPU
 * depth. Moving camera, decaying motion after release, then an exact hold.
 * First outdoor area, near-native viewport and two release positions.
 * --legacy is an expected-failure comparison of composed-MVP depth. */
#include "core/matrix.h"
#include "model/model.h"
#include "model/triangles.h"
#include "render/renderer.h"
#include "resource/store.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* Odd dimensions put a pixel center exactly on the CPU reference ray. */
#define W 961
#define H 721
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "light surface line%d (%s): %s\n", __LINE__, #x, e);     \
      goto done;                                                               \
    }                                                                          \
  } while (0)
typedef struct {
  float local[3][3], world[16];
  double point[3][3];
} Triangle;
static void subtract(double out[3], const double a[3], const double b[3]) {
  for (unsigned i = 0; i < 3; ++i)
    out[i] = a[i] - b[i];
}
static double dot(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
static void cross(double out[3], const double a[3], const double b[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}
static void point(double out[3], const float p[3], const float m[16]) {
  for (unsigned c = 0; c < 3; ++c)
    out[c] = (double)p[0] * m[c] + (double)p[1] * m[4 + c] +
             (double)p[2] * m[8 + c] + m[12 + c];
}
static int hit(const Triangle *t, const double origin[3],
               const double direction[3], double *distance) {
  double a[3], b[3], h[3], s[3], q[3];
  subtract(a, t->point[1], t->point[0]);
  subtract(b, t->point[2], t->point[0]);
  cross(h, direction, b);
  double d = dot(a, h);
  if (fabs(d) < 1e-9)
    return 0;
  subtract(s, origin, t->point[0]);
  double u = dot(s, h) / d;
  cross(q, s, a);
  double v = dot(direction, q) / d;
  /* Avoid edges: the center ray remains safely inside the selected wall. */
  if (u < .03 || v < .03 || u + v > .97)
    return 0;
  *distance = dot(b, q) / d;
  return 1;
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--legacy")))
    return 2;
  int legacy = argc == 3, rc = 1;
  char e[256] = {0}, path[2048];
  BkRenderer *r = NULL;
  BkResourceStore *store = NULL;
  BkModel *model = NULL;
  BkTexture *texture = NULL;
  BkLightSet *lights = NULL;
  BkGpuMesh *mesh[2] = {0};
  BkBlob blob = {0};
  float *world = NULL;
  Triangle *triangles = NULL;
  uint8_t white[4] = {255, 255, 255, 255}, pixels[W * H * 4];
  unsigned cases = 0, frames = 0, wrong[3] = {0}, changes[3] = {0};
  CHECK(store = bk_resources_create(e));
  snprintf(path, sizeof(path), "%s/bk3_03.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_03", path, e));
  CHECK(r = bk_renderer_create(W, H, stderr, e));
  CHECK(texture = bk_texture_create(r, &(BkImage){1, 1, white}, e));
  CHECK(lights = bk_light_set_create(r, &(BkLighting){0}, e));
  for (unsigned area = 0; area < 1; ++area) {
    char name[32];
    snprintf(name, sizeof(name), "m00_%02u.x", 10 + area);
    CHECK(bk_resources_read(store, "bk3_03", name, &blob, e) == BK_RESOURCE_OK);
    CHECK(bk_model_decode(blob.data, blob.size, &model, e) == BK_MODEL_OK);
    bk_blob_free(&blob);
    CHECK(world = malloc((size_t)model->frame_count * 16 * sizeof(float)));
    CHECK(bk_model_world_matrices(model, world, (size_t)model->frame_count * 16,
                                  e));
    size_t capacity = model->triangle_count * 2, count = 0;
    CHECK(triangles = calloc(capacity, sizeof(*triangles)));
    for (uint32_t f = 0; f < model->frame_count; ++f) {
      if (model->frames[f].mesh_index == BK_MODEL_NONE)
        continue;
      const BkModelMesh *m = &model->meshes[model->frames[f].mesh_index];
      for (uint32_t j = m->first_submesh;
           j < m->first_submesh + m->submesh_count; ++j) {
        const BkModelSubmesh *s = &model->submeshes[j];
        float alpha = model->materials[s->material_index].diffuse[3];
        if (!(alpha > .999999f && alpha < 1.000001f))
          continue;
        BkModelTriangles list = {0};
        CHECK(bk_model_triangles(model, j, &list, e));
        for (uint32_t t = 0; t < list.count; t += 3) {
          CHECK(count < capacity);
          Triangle *p = &triangles[count++];
          memcpy(p->world, world + f * 16, 64);
          for (unsigned v = 0; v < 3; ++v) {
            memcpy(p->local[v], s->vertices[list.indices[t + v]].position, 12);
            point(p->point[v], p->local[v], p->world);
          }
        }
        bk_model_triangles_free(&list);
      }
    }
    for (uint32_t f = 0; f < model->frame_count; ++f) {
      if (model->frames[f].mesh_index == BK_MODEL_NONE)
        continue;
      const BkModelMesh *m = &model->meshes[model->frames[f].mesh_index];
      for (uint32_t j = m->first_submesh;
           j < m->first_submesh + m->submesh_count; ++j) {
        const BkModelSubmesh *s = &model->submeshes[j];
        if (!strstr(s->name, "akarimore") || s->vertex_count % 4)
          continue;
        for (uint32_t q = 0; q < s->vertex_count; q += 4) {
          double center[3] = {0}, vertices[4][3], a[3], b[3], normal[3];
          for (unsigned v = 0; v < 4; ++v) {
            point(vertices[v], s->vertices[q + v].position, world + f * 16);
            for (unsigned c = 0; c < 3; ++c)
              center[c] += vertices[v][c] * .25;
          }
          subtract(a, vertices[1], vertices[0]);
          subtract(b, vertices[2], vertices[0]);
          cross(normal, a, b);
          double length = sqrt(dot(normal, normal));
          CHECK(length > 0);
          for (unsigned c = 0; c < 3; ++c)
            normal[c] /= length;
          size_t selected = SIZE_MAX;
          double gap = 1;
          for (size_t t = 0; t < count; ++t) {
            double distance;
            if (hit(&triangles[t], center, normal, &distance) &&
                fabs(distance) < fabs(gap))
              selected = t, gap = distance;
          }
          if (selected == SIZE_MAX || fabs(gap) > .025 || fabs(gap) < .001)
            continue;
          const Triangle *wall = &triangles[selected];
          BkLitVertex wall_v[3] = {0}, glow_v[4] = {0};
          for (unsigned v = 0; v < 3; ++v) {
            memcpy(&wall_v[v].base.x, wall->local[v], 12);
            wall_v[v].base.a = 1;
            wall_v[v].emissive[0] = .2f;
          }
          for (unsigned v = 0; v < 4; ++v) {
            memcpy(&glow_v[v].base.x, s->vertices[q + v].position, 12);
            glow_v[v].base.a = 1;
            glow_v[v].emissive[1] = .5f;
          }
          CHECK(mesh[0] = bk_lit_mesh_create(r, wall_v, 3,
                                             (uint16_t[]){0, 1, 2}, 3, e));
          CHECK(mesh[1] = bk_lit_mesh_create(
                    r, glow_v, 4, (uint16_t[]){0, 1, 2, 0, 2, 3}, 6, e));
          for (unsigned release = 0; release < 2; ++release) {
            int previous = -1;
            for (unsigned frame = 0; frame < 360; ++frame) {
              unsigned phase = frame < 120 ? 0 : frame < 300 ? 1 : 2;
              double motion =
                  phase == 0   ? sin(((double)frame - release) * .13) * 2
                  : phase == 1 ? sin((119. - release) * .13) * 2 *
                                     exp(-(double)(frame - 119) / 60)
                               : 0;
              float camera[16], view[16], projection[16], vp[16], target[3];
              memcpy(camera, bk_identity, 64);
              for (unsigned c = 0; c < 3; ++c) {
                target[c] = (float)center[c];
                camera[12 + c] = (float)(center[c] + normal[c] * 200);
              }
              const double offset = 170;
              camera[12] += (float)((motion + offset) * normal[2]);
              camera[14] -= (float)((motion + offset) * normal[0]);
              camera[13] += (float)(motion * .3 + offset * .7);
              CHECK(bk_camera_aim(camera, camera, target) &&
                    bk_camera_view(view, camera) &&
                    bk_camera_projection(
                        projection, &(BkCameraLens){1, .75f, .5f, 126384}));
              bk_matrix_multiply(vp, view, projection);
              BkDepthTransform depth;
              CHECK(bk_renderer_depth_transform(&depth, view, projection, e));
              CHECK(bk_renderer_begin(r, e));
              for (unsigned k = 0; k < 2; ++k) {
                const float *w = k ? world + f * 16 : wall->world;
                float mvp[16];
                bk_matrix_multiply(mvp, w, vp);
                BkDrawState state = {k ? BK_BLEND_ADDITIVE : BK_BLEND_OPAQUE,
                                     !k, BK_CULL_NONE};
                CHECK(legacy
                          ? bk_renderer_draw_lit_mesh(r, texture, mesh[k],
                                                      lights, mvp, w, state, e)
                          : bk_renderer_draw_lit_mesh_projected(
                                r, texture, mesh[k], lights, mvp, w, &depth,
                                state, e));
              }
              CHECK(bk_renderer_end(r, e) &&
                    bk_renderer_readback(r, pixels, sizeof(pixels), e));
              uint8_t *pixel = pixels + ((H / 2) * W + W / 2) * 4;
              CHECK(abs(pixel[0] - 51) <= 1 && pixel[2] == 0);
              int visible = pixel[1] > 64;
              if (visible != (gap < 0))
                printf("MISMATCH part%u quad%u release%u frame%u gap%.9g "
                       "visible%d\n",
                       j, q, release, frame, gap, visible);
              wrong[phase] += visible != (gap < 0);
              changes[phase] += previous >= 0 && visible != previous;
              previous = visible;
              ++frames;
            }
            ++cases;
          }
          for (unsigned k = 0; k < 2; ++k) {
            bk_mesh_destroy(r, mesh[k]);
            mesh[k] = NULL;
          }
        }
      }
    }
    free(triangles);
    triangles = NULL;
    free(world);
    world = NULL;
    bk_model_destroy(model);
    model = NULL;
  }
  printf("%s light surfaces cases%u frames%u wrong%u/%u/%u changes%u/%u/%u\n",
         wrong[0] + wrong[1] + wrong[2] ? "FAIL" : "PASS", cases, frames,
         wrong[0], wrong[1], wrong[2], changes[0], changes[1], changes[2]);
  CHECK(cases == 22 && !wrong[0] && !wrong[1] && !wrong[2]);
  rc = 0;
done:
  for (unsigned k = 0; k < 2; ++k)
    bk_mesh_destroy(r, mesh[k]);
  free(triangles);
  free(world);
  bk_model_destroy(model);
  bk_blob_free(&blob);
  bk_light_set_destroy(r, lights);
  bk_texture_destroy(r, texture);
  bk_renderer_destroy(r);
  bk_resources_destroy(store);
  return rc;
}
