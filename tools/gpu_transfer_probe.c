#include "core/matrix.h"
#include "render/renderer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define N 131
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
static double worst;
static unsigned comparisons;
static void transform(BkLitVertex *v, const float *m) {
  float p[4], n[3];
  bk_matrix_point(p, (float[]){v->base.x, v->base.y, v->base.z}, m);
  double delta = (double)p[3] - 1;
  if (delta < -(double)1e-5f || delta > (double)1e-5f)
    for (unsigned i = 0; i < 3; i++)
      p[i] = (float)((double)p[i] / p[3]);
  for (unsigned i = 0; i < 3; i++)
    n[i] =
        (float)((double)v->normal[0] * m[i] + (double)v->normal[1] * m[4 + i] +
                (double)v->normal[2] * m[8 + i]);
  v->base.x = p[0];
  v->base.y = p[1];
  v->base.z = p[2];
  memcpy(v->normal, n, 12);
}
static void copy_vertices(BkLitVertex *src, BkLitVertex *dst,
                          const BkVertexPair *pairs, unsigned count,
                          const float *world) {
  for (unsigned i = 0; i < count; i++) {
    BkLitVertex v = src[pairs[i].source];
    transform(&v, world);
    memcpy(&dst[pairs[i].target].base.x, &v.base.x, 12);
    memcpy(dst[pairs[i].target].normal, v.normal, 12);
  }
}
static int compare(const BkLitVertex *got, const BkLitVertex *expected,
                   char e[256]) {
  for (unsigned i = 0; i < N; i++) {
    float a[22], b[22];
    memcpy(a, got + i, sizeof(a));
    memcpy(b, expected + i, sizeof(b));
    for (unsigned j = 0; j < 22; j++) {
      double delta = fabs((double)a[j] - b[j]) / fmax(1, fabs(b[j]));
      if (delta > worst)
        worst = delta;
      if (!isfinite(delta) || delta > 3e-5 ||
          ((j >= 3 && j < 9) || j >= 12 ? a[j] != b[j] : 0)) {
        snprintf(e, 256, "transfer mismatch vertex%u field%u got%g want%g", i,
                 j, a[j], b[j]);
        return 0;
      }
      comparisons++;
    }
  }
  return 1;
}
static int resume_probe(BkRenderer *r, BkVertexTransfer *transfer,
                        char e[256]) {
  int rc = 0;
  BkGpuMesh *front = NULL, *back = NULL;
  BkTexture *white = NULL;
  uint8_t pixel[] = {255, 255, 255, 255}, output[32 * 32 * 4];
  BkVertex v[4] = {{-1, -1, .2f, 0, 0, 0, 0, 1, 1},
                   {0, -1, .2f, 0, 0, 0, 0, 1, 1},
                   {0, 1, .2f, 0, 0, 0, 0, 1, 1},
                   {-1, 1, .2f, 0, 0, 0, 0, 1, 1}};
  uint16_t indices[] = {0, 1, 2, 0, 2, 3};
  white = bk_texture_create(r, &(BkImage){1, 1, pixel}, e);
  CHECK(white);
  front = bk_mesh_create(r, v, 4, indices, 6, e);
  CHECK(front);
  for (unsigned i = 0; i < 4; i++) {
    if (v[i].x == 0)
      v[i].x = 1;
    v[i].z = .8f;
    v[i].r = 1;
    v[i].b = 0;
  }
  back = bk_mesh_create(r, v, 4, indices, 6, e);
  CHECK(back);
  BkDrawState draw = {BK_BLEND_OPAQUE, 1, BK_CULL_NONE};
  for (unsigned frame = 0; frame < 16; frame++) {
    BkViewport vp = {frame % 4, frame / 4, 24, 24};
    CHECK(bk_renderer_begin(r, e));
    CHECK(bk_renderer_viewport(r, &vp, e));
    CHECK(bk_renderer_draw_mesh(r, white, front, bk_identity, draw, e));
    for (unsigned i = 0; i < 3; i++)
      CHECK(bk_vertex_transfers_apply(r, &transfer, 1, e));
    CHECK(bk_renderer_draw_mesh(r, white, back, bk_identity, draw, e));
    CHECK(bk_renderer_end(r, e));
    CHECK(bk_renderer_readback(r, output, sizeof(output), e));
    for (unsigned y = 0; y < 32; y++)
      for (unsigned x = 0; x < 32; x++) {
        int inside = x >= vp.x && x < vp.x + vp.width && y >= vp.y &&
                     y < vp.y + vp.height;
        const uint8_t *p = output + (y * 32 + x) * 4;
        CHECK(p[0] == (inside && x >= vp.x + vp.width / 2 ? 255 : 0) &&
              p[1] == 0 &&
              p[2] == (inside && x < vp.x + vp.width / 2 ? 255 : 0) &&
              p[3] == 255);
      }
  }
  rc = 1;
done:
  bk_mesh_destroy(r, front);
  bk_mesh_destroy(r, back);
  bk_texture_destroy(r, white);
  return rc;
}
int main(void) {
  char e[256] = {0};
  int rc = 1;
  BkRenderer *r = bk_renderer_create(32, 32, stdout, e);
  BkGpuMesh *meshes[3] = {0};
  BkVertexTransfer *t[4] = {0};
  BkSkinPalette *palette = NULL;
  BkLitVertex input[3][N], expected[3][N], got[N];
  BkVertexPair pairs[4][N];
  unsigned counts[] = {N, 7, 5, 3};
  unsigned from[] = {0, 1, 2, 2}, to[] = {1, 2, 2, 1};
  float world[4][16], bone[16];
  uint32_t offsets[N + 1];
  BkGpuSkinWeight weights[N];
  CHECK(r);
  for (unsigned m = 0; m < 3; m++)
    for (unsigned i = 0; i < N; i++)
      input[m][i] = (BkLitVertex){
          .base = {(float)(i % 7) * .1f, (float)(i % 5) * .2f, .1f * m,
                   .01f * i, .003f * i, .2f + m * .1f, .3f, .4f, .8f},
          .normal = {.1f * i, 1, -.2f},
          .ambient = {.1f, .2f, .3f},
          .emissive = {.3f, .2f, .1f},
          .specular = {.01f, .02f, .03f},
          .power = 7};
  for (unsigned i = 0; i < 3; i++) {
    meshes[i] = bk_lit_mesh_create(r, input[i], N, (uint16_t[]){0, 1, 2}, 3, e);
    CHECK(meshes[i]);
  }
  palette = bk_skin_palette_create(r, 1, e);
  CHECK(palette);
  for (unsigned i = 0; i < N; i++) {
    offsets[i] = i;
    weights[i] = (BkGpuSkinWeight){.bone = 0, .reset = 1, .weight = 1};
    memcpy(weights[i].position, &input[1][i].base.x, 12);
    memcpy(weights[i].normal, input[1][i].normal, 12);
  }
  offsets[N] = N;
  CHECK(bk_lit_mesh_skin(r, meshes[1], palette, offsets, weights, N, e));
  for (unsigned i = 0; i < N; i++)
    pairs[0][i] = (BkVertexPair){(i * 3) % N, i % 17};
  for (unsigned i = 0; i < 7; i++)
    pairs[1][i] = (BkVertexPair){i, i};
  pairs[2][0] = (BkVertexPair){0, 0};
  pairs[2][1] = (BkVertexPair){0, 1};
  pairs[2][2] = (BkVertexPair){1, 0};
  pairs[2][3] = (BkVertexPair){0, 0};
  pairs[2][4] = (BkVertexPair){0, 2};
  for (unsigned i = 0; i < 3; i++)
    pairs[3][i] = (BkVertexPair){i, i};
  for (unsigned i = 0; i < 4; i++) {
    t[i] = bk_vertex_transfer_create(r, meshes[from[i]], meshes[to[i]],
                                     pairs[i], counts[i], e);
    CHECK(t[i]);
  }
  CHECK(!bk_vertex_transfer_create(r, meshes[0], meshes[1],
                                   (BkVertexPair[]){{N, 0}}, 1, e));
  CHECK(resume_probe(r, t[0], e));
  BkRenderStats baseline = bk_renderer_stats(r);
  for (unsigned frame = 0; frame < 64; frame++) {
    memcpy(expected, input, sizeof(expected));
    for (unsigned i = 0; i < 3; i++)
      CHECK(bk_lit_mesh_update(r, meshes[i], input[i], N, e));
    memcpy(bone, bk_identity, 64);
    bone[12] = .125f * (frame % 3);
    bone[0] = 1.2f;
    bone[5] = .9f;
    CHECK(bk_skin_palette_update(r, palette, bone, 1, e));
    for (unsigned i = 0; i < N; i++)
      transform(expected[1] + i, bone);
    for (unsigned i = 0; i < 4; i++) {
      memcpy(world[i], bk_identity, 64);
      world[i][0] = .9f;
      world[i][5] = 1.1f;
      world[i][12] = .015625f * i;
      world[i][13] = -.03125f;
      world[i][15] = (float[]){1, 1.000005f, 1.00002f, .85f}[frame % 4];
      if (frame % 7 == 0)
        world[i][3] = .0001f;
      CHECK(bk_vertex_transfer_world(t[i], world[i], e));
    }
    float invalid[16];
    memcpy(invalid, bk_identity, 64);
    invalid[0] = NAN;
    CHECK(!bk_vertex_transfer_world(t[0], invalid, e));
    CHECK(!bk_vertex_transfers_apply(r, t, 4, e));
    CHECK(bk_renderer_begin(r, e));
    CHECK(!bk_vertex_transfer_world(t[0], world[0], e));
    BkVertexTransfer *bad[] = {t[0], NULL};
    CHECK(!bk_vertex_transfers_apply(r, bad, 2, e));
    for (unsigned repeat = 0; repeat < 2; repeat++) {
      CHECK(bk_vertex_transfers_apply(r, t, 4, e));
      for (unsigned i = 0; i < 4; i++)
        copy_vertices(expected[from[i]], expected[to[i]], pairs[i], counts[i],
                      world[i]);
    }
    CHECK(bk_vertex_transfers_apply(r, NULL, 0, e));
    CHECK(bk_renderer_end(r, e));
    for (unsigned i = 0; i < 3; i++) {
      CHECK(bk_lit_mesh_readback(r, meshes[i], got, N, e));
      CHECK(compare(got, expected[i], e));
    }
    BkRenderStats now = bk_renderer_stats(r);
    CHECK(now.live_allocations == baseline.live_allocations &&
          now.live_bytes == baseline.live_bytes);
  }
  /* Frozen pose, no vertex/palette upload: toggling callback off must restore
   * base ENVL output; worlds still prepare the target's compute each frame. */
  BkRenderStats before_held = bk_renderer_stats(r);
  for (unsigned frame = 0; frame < 16; frame++) {
    memcpy(expected[1], input[1], sizeof(expected[1]));
    for (unsigned i = 0; i < N; i++)
      transform(expected[1] + i, bone);
    CHECK(bk_vertex_transfer_world(t[0], world[0], e));
    CHECK(bk_renderer_begin(r, e));
    if (frame % 2 == 0) {
      CHECK(bk_vertex_transfers_apply(r, t, 1, e));
      copy_vertices(input[0], expected[1], pairs[0], counts[0], world[0]);
    }
    CHECK(bk_renderer_end(r, e));
    CHECK(bk_lit_mesh_readback(r, meshes[1], got, N, e));
    CHECK(compare(got, expected[1], e));
  }
  BkRenderStats after_held = bk_renderer_stats(r);
  CHECK(after_held.uploaded_bytes == before_held.uploaded_bytes &&
        after_held.skin_dispatches - before_held.skin_dispatches == 16);
  /* Reusing a prepared snapshot across presentations still regenerates ENVL;
   * rigid source buffers retain prior writes, including alias chains. */
  for (unsigned redraw = 0; redraw < 3; redraw++) {
    memcpy(expected[1], input[1], sizeof(expected[1]));
    for (unsigned i = 0; i < N; i++)
      transform(expected[1] + i, bone);
    CHECK(bk_renderer_begin(r, e));
    CHECK(bk_vertex_transfers_apply(r, t, 4, e));
    for (unsigned i = 0; i < 4; i++)
      copy_vertices(expected[from[i]], expected[to[i]], pairs[i], counts[i],
                    world[i]);
    CHECK(bk_renderer_end(r, e));
    for (unsigned i = 0; i < 3; i++) {
      CHECK(bk_lit_mesh_readback(r, meshes[i], got, N, e));
      CHECK(compare(got, expected[i], e));
    }
  }
  /* Last callback owner release also refreshes the surviving skinned mesh. */
  CHECK(bk_renderer_begin(r, e));
  CHECK(bk_vertex_transfers_apply(r, t, 1, e));
  CHECK(bk_renderer_end(r, e));
  bk_vertex_transfer_destroy(t[0]);
  t[0] = NULL;
  CHECK(bk_renderer_begin(r, e));
  CHECK(bk_renderer_end(r, e));
  CHECK(bk_lit_mesh_readback(r, meshes[1], got, N, e));
  memcpy(expected[1], input[1], sizeof(expected[1]));
  for (unsigned i = 0; i < N; i++)
    transform(expected[1] + i, bone);
  CHECK(compare(got, expected[1], e));
  t[0] = bk_vertex_transfer_create(r, meshes[0], meshes[1], pairs[0], counts[0],
                                   e);
  CHECK(t[0]);
  /* Program retains source and target allocations after client release. */
  for (unsigned i = 0; i < 3; i++) {
    bk_mesh_destroy(r, meshes[i]);
    meshes[i] = NULL;
  }
  CHECK(bk_renderer_begin(r, e));
  CHECK(bk_vertex_transfers_apply(r, t, 4, e));
  CHECK(bk_renderer_end(r, e));
  for (unsigned i = 0; i < 4; i++) {
    bk_vertex_transfer_destroy(t[i]);
    t[i] = NULL;
  }
  bk_skin_palette_destroy(r, palette);
  palette = NULL;
  printf("PASS GPU transfer:64 ordered +16 held +16 attachment +3 redraw "
         "frames; %u "
         "scalar checks max%g "
         "ordered/duplicate/alias/projective/retained buffers; stable "
         "allocations\n",
         comparisons, worst);
  rc = 0;
done:
  for (unsigned i = 0; i < 4; i++)
    bk_vertex_transfer_destroy(t[i]);
  for (unsigned i = 0; i < 3; i++)
    bk_mesh_destroy(r, meshes[i]);
  bk_skin_palette_destroy(r, palette);
  bk_renderer_destroy(r);
  if (rc)
    fprintf(stderr, "GPU transfer failed:%s\n", e);
  return rc;
}
