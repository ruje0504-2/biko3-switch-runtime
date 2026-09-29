/* Independent double-precision reference for vertex lighting + GPU readback. */
#include "render/renderer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static double clamp(double v) { return fmax(0, fmin(1, v)); }
static int expected_vertex(const BkLitVertex *v, const float world[16],
                           const BkLighting *lights, const float viewer[3],
                           double out[4], double highlight[3]) {
  double position[3] = {world[12], world[13], world[14]}, normal[3], a[3][4];
  float xyz[] = {v->base.x, v->base.y, v->base.z};
  for (unsigned j = 0; j < 3; j++) {
    for (unsigned k = 0; k < 3; k++) {
      position[j] += xyz[k] * (double)world[k * 4 + j];
      a[j][k] = world[j * 4 + k];
    }
    a[j][3] = v->normal[j];
    /* Fixed-function eye-space XYZ: world homogeneous position then view.
     * Rotate back into world axes; this probe's viewer has identity axes. */
    position[j] += (double)viewer[j] * (1.0 - world[15]);
  }
  /* Solve M*n_world=n_local rather than duplicating GLSL's inverse operation.
   */
  for (unsigned k = 0; k < 3; k++) {
    unsigned pivot = k;
    for (unsigned j = k + 1; j < 3; j++)
      if (fabs(a[j][k]) > fabs(a[pivot][k]))
        pivot = j;
    for (unsigned j = 0; j < 4; j++) {
      double t = a[k][j];
      a[k][j] = a[pivot][j];
      a[pivot][j] = t;
    }
    double scale = a[k][k];
    if (scale == 0)
      return 0;
    for (unsigned j = k; j < 4; j++)
      a[k][j] /= scale;
    for (unsigned i = 0; i < 3; i++)
      if (i != k) {
        double f = a[i][k];
        for (unsigned j = k; j < 4; j++)
          a[i][j] -= f * a[k][j];
      }
  }
  double len = 0;
  for (unsigned j = 0; j < 3; j++) {
    normal[j] = a[j][3];
    len += normal[j] * normal[j];
  }
  len = sqrt(len);
  for (unsigned j = 0; j < 3; j++)
    normal[j] = len > 0 ? normal[j] / len : 0;
  float diffuse[] = {v->base.r, v->base.g, v->base.b};
  for (unsigned j = 0; j < 3; j++)
    out[j] = v->emissive[j] + v->ambient[j] * (double)lights->ambient[j];
  double eye[3], eye_length = 0;
  for (unsigned j = 0; j < 3; j++) {
    highlight[j] = 0;
    eye[j] = viewer[j] - position[j];
    eye_length += eye[j] * eye[j];
  }
  eye_length = sqrt(eye_length);
  for (unsigned i = 0; i < lights->point_count + lights->spot_count; i++) {
    const BkSpotLight *spot = i < lights->point_count
                                  ? NULL
                                  : &lights->spots[i - lights->point_count];
    const BkPointLight *p = spot ? &spot->point : &lights->points[i];
    double delta[3], squared = 0, dot = 0;
    for (unsigned j = 0; j < 3; j++) {
      delta[j] = p->position[j] - position[j];
      squared += delta[j] * delta[j];
      dot += normal[j] * delta[j];
    }
    double d = sqrt(squared);
    if (d > p->range)
      continue;
    double denominator =
        p->attenuation0 + p->attenuation1 * d + p->attenuation2 * squared;
    if (denominator <= 0)
      continue;
    double cone = 1;
    if (spot) {
      if (d == 0)
        continue;
      double sd = 0, sn = 0;
      for (unsigned j = 0; j < 3; ++j) {
        sd -= spot->direction[j] * delta[j];
        sn += (double)spot->direction[j] * spot->direction[j];
      }
      double rho = sd / (sqrt(sn) * d), outer = cos(spot->phi / 2.0),
             inner = cos(spot->theta / 2.0);
      if (rho <= outer)
        continue;
      if (rho < inner)
        cone = pow((rho - outer) / (inner - outer), spot->falloff);
    }
    double incidence = d > 0 ? fmax(0, dot / d) : 0;
    for (unsigned j = 0; j < 3; j++)
      out[j] += (v->ambient[j] * (double)p->ambient[j] +
                 diffuse[j] * (double)p->diffuse[j] * incidence) *
                cone / denominator;
    if (v->power >= .01f && dot > 0 && eye_length > 0) {
      double half[3], length = 0, nh = 0;
      for (unsigned j = 0; j < 3; j++) {
        half[j] = eye[j] / eye_length + delta[j] / d;
        length += half[j] * half[j];
        nh += normal[j] * half[j];
      }
      double factor =
          pow(length > 0 ? fmax(0, nh / sqrt(length)) : 0, v->power);
      for (unsigned j = 0; j < 3; j++)
        highlight[j] += v->specular[j] * (double)p->specular[j] * factor *
                        cone / denominator;
    }
  }
  for (unsigned j = 0; j < 3; j++) {
    out[j] = clamp(out[j]);
    highlight[j] = clamp(highlight[j]);
  }
  out[3] = v->base.a;
  return 1;
}
static int check(BkRenderer *r, const double color[4], unsigned test,
                 char error[256]) {
  uint8_t pixel[4];
  if (!bk_renderer_readback(r, pixel, 4, error))
    return 0;
  for (unsigned j = 0; j < 4; j++) {
    int want = (int)lround(255 * clamp(color[j]));
    if (abs(pixel[j] - want) > 2) {
      snprintf(error, 256, "lighting test %u channel %u: got %u expected %d",
               test, j, pixel[j], want);
      return 0;
    }
  }
  return 1;
}
int main(void) {
  char error[256] = {0};
  int ok = 0;
  BkRenderer *r = bk_renderer_create(1, 1, stderr, error);
  BkLightSet *set = NULL, *alternate = NULL;
  BkGpuMesh *mesh = NULL;
  BkTexture *texture = NULL, *white = NULL, *black = NULL;
  if (!r)
    goto done;
  uint8_t pixel[] = {102, 204, 153, 128}, solid[] = {255, 255, 255, 255};
  BkImage image = {1, 1, pixel};
  texture = bk_texture_create(r, &image, error);
  image.rgba = solid;
  white = bk_texture_create(r, &image, error);
  uint8_t dark[] = {0, 0, 0, 128};
  image.rgba = dark;
  black = bk_texture_create(r, &image, error);
  if (!texture || !white || !black)
    goto done;
  const uint16_t indices[] = {0, 1, 2};
  for (unsigned test = 0; test < 72; test++) {
    BkLitVertex v[3] = {0};
    const float xy[3][2] = {{-1, -1}, {3, -1}, {-1, 3}};
    for (unsigned i = 0; i < 3; i++) {
      v[i].base =
          (BkVertex){xy[i][0], xy[i][1], .5f, .5f, .5f, .8f, .6f, .4f, .7f};
      v[i].normal[2] = test == 6 ? 9 : 1;
      for (unsigned j = 0; j < 3; j++) {
        v[i].ambient[j] = .5f;
        v[i].emissive[j] = .025f * (i + 1);
      }
    }
    BkLighting lighting = {.ambient = {.2f, .3f, .4f},
                           .point_count = test ? 1 : 0};
    lighting.points[0] = (BkPointLight){.position = {0, 0, 4},
                                        .range = 20,
                                        .diffuse = {.3f, .4f, .5f},
                                        .attenuation0 = 1,
                                        .ambient = {.02f, .03f, .04f}};
    if (test == 2)
      lighting.points[0].position[2] = -4;
    if (test == 3)
      lighting.points[0].range = .1f;
    if (test == 4) {
      lighting.points[0].attenuation0 = .25f;
      lighting.points[0].attenuation1 = .2f;
      lighting.points[0].attenuation2 = .05f;
    }
    float world[16];
    float viewer[] = {0, 0, 6};
    memcpy(world, bk_identity, sizeof(world));
    if (test == 5 || test == 16) {
      world[0] = 0;
      world[1] = 2;
      world[4] = 3;
      world[5] = 0;
      world[10] = -.5f;
      world[12] = .5f;
      world[13] = -.75f;
      world[14] = 1;
      for (unsigned i = 0; i < 3; i++) {
        v[i].normal[0] = 1;
        v[i].normal[1] = 2;
        v[i].normal[2] = 3;
      }
    }
    if (test == 7) {
      lighting.points[0].diffuse[0] = 9;
      lighting.points[0].diffuse[1] = 4;
      lighting.points[0].diffuse[2] = 8;
    }
    if (test == 8 || test == 9) {
      lighting.point_count = test == 8 ? 2 : 8;
      for (unsigned i = 1; i < lighting.point_count; i++) {
        lighting.points[i] = lighting.points[0];
        lighting.points[i].position[0] = (float)i * 3;
        lighting.points[i].diffuse[0] = .03f * i;
        lighting.points[i].diffuse[1] = .05f;
        lighting.points[i].diffuse[2] = .07f;
      }
    }
    if (test >= 10) {
      if (test == 10)
        world[15] = .999777f;
      if (test == 16)
        world[15] = .8f;
      lighting.points[0].specular[0] = 1;
      lighting.points[0].specular[1] = .5f;
      lighting.points[0].specular[2] = .8f;
      for (unsigned i = 0; i < 3; i++) {
        v[i].power = test == 10   ? 1
                     : test == 17 ? .009999f
                     : test == 18 ? .01f
                     : test == 19 ? 300
                                  : 20;
        v[i].specular[0] = .7f;
        v[i].specular[1] = .4f;
        v[i].specular[2] = .2f;
        if (test == 21)
          memset(v[i].normal, 0, sizeof(v[i].normal));
      }
      if (test == 12)
        viewer[0] = 5;
      if (test == 13)
        lighting.points[0].range = .1f;
      if (test == 14)
        lighting.points[0].position[2] = -4;
      if (test == 19) {
        viewer[2] = 1000;
        lighting.points[0].position[2] = 1000;
        lighting.points[0].range = 2000;
      }
      if (test == 15)
        for (unsigned j = 0; j < 3; j++)
          lighting.points[0].specular[j] = 20;
    }
    if (test >= 22 && test < 26) {
      float s = test == 22   ? 3.05519108678709e-7f
                : test == 23 ? 1e-20f
                : test == 24 ? 1e20f
                             : -3e-7f;
      world[0] = s;
      world[5] = 2 * s;
      world[10] = 3 * s;
      for (unsigned i = 0; i < 3; ++i) {
        v[i].normal[0] = 1;
        v[i].normal[1] = 2;
        v[i].normal[2] = 3;
      }
    }
    if (test >= 26 && test < 66) {
      lighting.spot_count = 1;
      lighting.spots[0] = (BkSpotLight){.point = lighting.points[0],
                                        .direction = {0, 0, -1},
                                        .falloff = 1,
                                        .theta = .1f,
                                        .phi = 2.2f};
      lighting.point_count = 0;
      BkSpotLight *s = &lighting.spots[0];
      if (test == 26) {
        s->theta = 2.8f;
        s->phi = 3.0f;
      }
      if (test == 27)
        s->direction[2] = 1;
      if (test == 29)
        s->theta = s->phi = 1.2f;
      if (test == 30)
        s->falloff = 0;
      if (test == 31)
        s->falloff = 8;
      if (test == 34)
        s->point.range = 1;
      if (test == 35) {
        s->point.attenuation0 = .25f;
        s->point.attenuation1 = .2f;
        s->point.attenuation2 = .05f;
      }
      if (test == 36) {
        s->point.position[0] = -1;
        s->point.position[1] = -1;
        s->point.position[2] = .5f;
      }
      if (test == 37)
        s->theta = s->phi = 0;
      if (test == 38)
        s->direction[2] = -17;
      if (test == 39)
        s->direction[2] = -1e-4f;
      if (test == 40) {
        s->direction[0] = 8e37f;
        s->direction[2] = -8e37f;
      }
      if (test == 32 || test == 33) {
        lighting.spot_count = test == 32 ? 8 : 4;
        lighting.point_count = test == 33 ? 4 : 0;
        for (unsigned k = 1; k < lighting.point_count; ++k)
          lighting.points[k] = lighting.points[0];
        for (unsigned k = 1; k < lighting.spot_count; ++k) {
          lighting.spots[k] = *s;
          lighting.spots[k].point.diffuse[0] = .03f * k;
          lighting.spots[k].direction[0] = .07f * k;
          lighting.spots[k].point.position[1] = .5f * k;
        }
      }
      if (test > 40) {
        s->direction[0] = .8f * sinf((float)test);
        s->direction[1] = .3f * cosf((float)test);
        s->falloff = (test - 40) * .125f;
        s->theta = (test % 5) * .2f;
        s->phi = s->theta + 1.5f;
        s->point.ambient[0] = .7f;
        s->point.diffuse[1] = 1.08f;
        s->point.attenuation1 = (test % 3) * .1f;
        s->point.attenuation2 = .03f;
      }
    }
    if (test >= 66) {
      /* Signed colors from h03_10/Light_PNT_(6), including light-order
       * cancellation and lower saturation. The independent double CPU
       * formula must agree without clamping each light separately. */
      lighting.point_count = test == 66 || test == 69 ? 1 : 2;
      lighting.points[0].diffuse[0] = -.5f;
      lighting.points[0].diffuse[1] = -.5f;
      lighting.points[0].diffuse[2] = -.4f;
      for (unsigned j = 0; j < 3; ++j) {
        lighting.points[0].ambient[j] = test == 69 ? -.3f : .1f;
        lighting.points[0].specular[j] = test >= 69 ? -.8f : 0;
      }
      if (lighting.point_count == 2) {
        lighting.points[1] = lighting.points[0];
        for (unsigned j = 0; j < 3; ++j) {
          lighting.points[1].diffuse[j] = .9f;
          lighting.points[1].ambient[j] = .15f;
          lighting.points[1].specular[j] = 1;
        }
        if (test == 68 || test == 71) {
          BkPointLight swap = lighting.points[0];
          lighting.points[0] = lighting.points[1];
          lighting.points[1] = swap;
        }
      }
      if (test >= 70) {
        lighting.spot_count = lighting.point_count;
        for (unsigned j = 0; j < lighting.spot_count; ++j)
          lighting.spots[j] = (BkSpotLight){.point = lighting.points[j],
              .direction = {0,0,-1}, .falloff = 1, .theta = 2.8f, .phi = 3};
        lighting.point_count = 0;
      }
    }
    set = bk_light_set_create(r, &(BkLighting){0}, error);
    mesh = bk_lit_mesh_create(r, v, 3, indices, 3, error);
    if (!set || !mesh)
      goto done;
    if (!bk_light_set_view(r, set, viewer, error) ||
        !bk_light_set_update(r, set, &lighting, error))
      goto done;
    BkLighting bad = lighting;
    bad.point_count = BK_MAX_POINT_LIGHTS + 1;
    if (bk_light_set_update(r, set, &bad, error))
      goto done;
    if (lighting.spot_count) {
      const float bad_values[] = {-1, NAN, INFINITY};
      for (unsigned k = 0; k < 3; ++k) {
        bad = lighting;
        bad.spots[0].falloff = bad_values[k];
        if (bk_light_set_update(r, set, &bad, error))
          goto done;
      }
      bad = lighting;
      memset(bad.spots[0].direction, 0, 12);
      if (bk_light_set_update(r, set, &bad, error))
        goto done;
      bad = lighting;
      bad.spots[0].theta = bad.spots[0].phi + 1;
      if (bk_light_set_update(r, set, &bad, error))
        goto done;
    }
    float invalid_eye[] = {0, 1, NAN};
    if (bk_light_set_view(r, set, invalid_eye, error))
      goto done;
    double expected[4] = {0};
    const double weights[] = {.5, .25, .25};
    for (unsigned i = 0; i < 3; i++) {
      double color[4], highlight[3];
      if (!expected_vertex(&v[i], world, &lighting, viewer, color, highlight))
        goto done;
      for (unsigned j = 0; j < 4; j++)
        expected[j] +=
            weights[i] * (color[j] * (test == 20 ? dark[j] : pixel[j]) / 255.0 +
                          (j < 3 ? highlight[j] : 0));
    }
    if (!bk_renderer_begin(r, error) ||
        !bk_renderer_draw_lit_mesh(
            r, test == 20 ? black : texture, mesh, set, bk_identity, world,
            (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE}, error) ||
        !bk_renderer_end(r, error))
      goto done;
    /* Mutate viewer before readback: prior submitted frame must be preserved.
     */
    float next_eye[] = {-100, 30, -40};
    if (!bk_light_set_view(r, set, next_eye, error) ||
        !check(r, expected, test, error))
      goto done;
    bk_mesh_destroy(r, mesh);
    mesh = NULL;
    bk_light_set_destroy(r, set);
    set = NULL;
  }
  /* Same frame: light descriptor isolation, lit/unlit format guard and UI
   * restoration. */
  BkLighting ambient = {.ambient = {1, 0, 0}};
  set = bk_light_set_create(r, &ambient, error);
  if (!set)
    goto done;
  ambient.ambient[0] = 0;
  ambient.ambient[2] = 1;
  alternate = bk_light_set_create(r, &ambient, error);
  if (!alternate)
    goto done;
  BkLitVertex v[3] = {0};
  const float xy[3][2] = {{-1, -1}, {3, -1}, {-1, 3}};
  for (unsigned i = 0; i < 3; i++) {
    v[i].base = (BkVertex){xy[i][0], xy[i][1], .5f, .5f, .5f, 1, 1, 1, .5f};
    for (unsigned j = 0; j < 3; j++)
      v[i].ambient[j] = 1;
  }
  mesh = bk_lit_mesh_create(r, v, 3, indices, 3, error);
  if (!mesh)
    goto done;
  double mixed[] = {.5, 0, .5, .5};
  if (!bk_renderer_begin(r, error) ||
      !bk_renderer_draw_lit_mesh(
          r, white, mesh, set, bk_identity, bk_identity,
          (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE}, error) ||
      !bk_renderer_draw_lit_mesh(
          r, white, mesh, alternate, bk_identity, bk_identity,
          (BkDrawState){BK_BLEND_ALPHA, 1, BK_CULL_NONE}, error) ||
      !bk_renderer_end(r, error) || !check(r, mixed, 10, error))
    goto done;
  if (!bk_renderer_begin(r, error))
    goto done;
  if (bk_light_set_update(r, set, &ambient, error))
    goto done;
  if (bk_renderer_draw_mesh(r, white, mesh, bk_identity,
                            (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE},
                            error))
    goto done;
  float singular[16] = {0};
  if (bk_renderer_draw_lit_mesh(r, white, mesh, set, bk_identity, singular,
                                (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE},
                                error))
    goto done;
  float collapsed[16] = {0};
  collapsed[15] = 1;
  BkRenderStats before = bk_renderer_stats(r);
  if (!bk_renderer_draw_lit_mesh(
          r, white, mesh, set, collapsed, collapsed,
          (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE}, error) ||
      bk_renderer_stats(r).draws != before.draws)
    goto done;
  if (bk_renderer_draw_lit_mesh(r, white, mesh, set, bk_identity, collapsed,
                                (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE},
                                error))
    goto done; /* Inconsistent transform must not be silently culled. */
  float flat[16];
  memcpy(flat, bk_identity, sizeof(flat));
  flat[10] = 0;
  if (bk_renderer_draw_lit_mesh(r, white, mesh, set, flat, flat,
                                (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE},
                                error))
    goto done; /* Nonzero-area singular geometry still needs valid normals. */
  if (!bk_renderer_draw_lit_mesh(
          r, white, mesh, set, bk_identity, bk_identity,
          (BkDrawState){BK_BLEND_OPAQUE, 1, BK_CULL_NONE}, error))
    goto done;
  BkVertex ui[3];
  for (unsigned i = 0; i < 3; i++)
    ui[i] = (BkVertex){xy[i][0], xy[i][1], 0, .5f, .5f, 0, 0, 1, 1};
  double blue[] = {0, 0, 1, 1};
  if (!bk_renderer_draw(r, white, ui, 3, bk_identity, error) ||
      !bk_renderer_end(r, error) || !check(r, blue, 11, error))
    goto done;
  fprintf(stderr, "PASS: 72 point/spot lighting/specular cases vs independent "
                  "vertex/interpolation "
                  "reference <=2/255; light descriptor isolation, lit "
                  "format/singular/zero-scale guards and UI restoration\n");
  ok = 1;
done:
  if (!ok)
    fprintf(stderr, "lighting render probe FAILED: %s\n", error);
  bk_mesh_destroy(r, mesh);
  bk_light_set_destroy(r, set);
  bk_light_set_destroy(r, alternate);
  bk_texture_destroy(r, white);
  bk_texture_destroy(r, black);
  bk_texture_destroy(r, texture);
  bk_renderer_destroy(r);
  return ok ? 0 : 1;
}
