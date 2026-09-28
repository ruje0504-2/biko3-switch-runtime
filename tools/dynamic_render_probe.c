/* Vertex updates must wait for the prior submission, not overwrite its draw. */
#include "render/renderer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void vertices(BkVertex plain[4], BkLitVertex lit[4], unsigned frame) {
  const float xy[4][2] = {{-1, -1}, {0, -1}, {0, 1}, {-1, 1}};
  float shift = (float)(frame & 1);
  for (unsigned i = 0; i < 4; i++) {
    plain[i] = (BkVertex){
        xy[i][0] + shift,  xy[i][1], .5f, .5f, .5f, frame & 2 ? 0 : 1, 0,
        frame & 2 ? 1 : 0, 1};
    lit[i] = (BkLitVertex){
        .base = plain[i],
        .normal = {0, 0, 1},
        .ambient = {frame & 2 ? 1 : 0, frame & 2 ? 0 : 1, frame & 2 ? 1 : 0}};
  }
}
static int check(BkRenderer *r, unsigned frame, char error[256]) {
  uint8_t pixels[16 * 16 * 4];
  if (!bk_renderer_readback(r, pixels, sizeof(pixels), error))
    return 0;
  for (unsigned side = 0; side < 2; side++) {
    unsigned offset = 4 * (8 * 16 + 4 + 8 * side);
    const int occupied = side == (frame & 1), lit = (frame / 4) & 1;
    const int want[4] = {occupied && (lit ? !!(frame & 2) : !(frame & 2)) ? 255
                                                                          : 0,
                         occupied && lit && !(frame & 2) ? 255 : 0,
                         occupied && (frame & 2) ? 255 : 0, 255};
    for (unsigned j = 0; j < 4; j++)
      if (abs((int)pixels[offset + j] - want[j]) > 1) {
        snprintf(error, 256, "frame %u side %u channel %u: %u != %d", frame,
                 side, j, pixels[offset + j], want[j]);
        return 0;
      }
  }
  return 1;
}
int main(void) {
  char error[256] = {0};
  int ok = 0;
  BkRenderer *r = bk_renderer_create(16, 16, stderr, error), *other = NULL;
  BkTexture *texture = NULL;
  BkLightSet *lights = NULL;
  BkGpuMesh *plain_mesh = NULL, *lit_mesh = NULL;
  BkVertex plain[4];
  BkLitVertex lit[4];
  const uint16_t indices[] = {0, 1, 2, 0, 2, 3};
  uint8_t white[] = {255, 255, 255, 255};
  BkImage image = {1, 1, white};
  BkLighting ambient = {.ambient = {1, 1, 1}};
  if (!r)
    goto done;
  texture = bk_texture_create(r, &image, error);
  lights = bk_light_set_create(r, &ambient, error);
  vertices(plain, lit, 0);
  plain_mesh = bk_mesh_create(r, plain, 4, indices, 6, error);
  lit_mesh = bk_lit_mesh_create(r, lit, 4, indices, 6, error);
  other = bk_renderer_create(1, 1, stderr, error);
  if (!texture || !lights || !plain_mesh || !lit_mesh || !other)
    goto done;
  if (bk_mesh_update(other, plain_mesh, plain, 4, error) ||
      bk_mesh_update(NULL, plain_mesh, plain, 4, error) ||
      bk_mesh_update(r, NULL, plain, 4, error) ||
      bk_light_set_view(other, lights, bk_identity, error) ||
      bk_light_set_view(r, lights, NULL, error))
    goto done;
  for (unsigned frame = 0; frame < 16; frame++) {
    /* Last vertex rejects after validating all preceding vertices; neither
     * mesh may be partially changed by rejected data/count/layout calls. */
    BkVertex invalid[4];
    BkLitVertex invalid_lit[4];
    memcpy(invalid, plain, sizeof(invalid));
    memcpy(invalid_lit, lit, sizeof(invalid_lit));
    invalid[0].x = 99;
    invalid[3].a = NAN;
    invalid_lit[0].base.x = 99;
    invalid_lit[3].power = NAN;
    if (bk_mesh_update(r, plain_mesh, invalid, 4, error) ||
        bk_lit_mesh_update(r, lit_mesh, invalid_lit, 4, error) ||
        bk_mesh_update(r, plain_mesh, plain, 3, error) ||
        bk_lit_mesh_update(r, lit_mesh, lit, 5, error) ||
        bk_mesh_update(r, plain_mesh, NULL, 4, error) ||
        bk_mesh_update(r, lit_mesh, plain, 4, error) ||
        bk_lit_mesh_update(r, plain_mesh, lit, 4, error))
      goto done;
    if (!bk_renderer_begin(r, error))
      goto done;
    if (bk_mesh_update(r, plain_mesh, plain, 4, error) ||
        bk_lit_mesh_update(r, lit_mesh, lit, 4, error) ||
        bk_light_set_view(r, lights, bk_identity, error))
      goto done;
    BkDrawState state = {BK_BLEND_OPAQUE, 1, BK_CULL_NONE};
    int drawn =
        ((frame / 4) & 1)
            ? bk_renderer_draw_lit_mesh(r, texture, lit_mesh, lights,
                                        bk_identity, bk_identity, state, error)
            : bk_renderer_draw_mesh(r, texture, plain_mesh, bk_identity, state,
                                    error);
    if (!drawn || !bk_renderer_end(r, error))
      goto done;
    vertices(plain, lit, frame + 1);
    /* No readback/explicit wait before the update: this is the synchronization
     * exercised by the probe. Readback must retain the previous frame. */
    if (!bk_mesh_update(r, plain_mesh, plain, 4, error) ||
        !bk_lit_mesh_update(r, lit_mesh, lit, 4, error) ||
        !check(r, frame, error))
      goto done;
  }
  fprintf(stderr,
          "PASS: 16 Vulkan frames, mutable lit/unlit geometry and colors, "
          "in-flight synchronization, atomic invalid data, owner/count/"
          "layout/active-frame guards\n");
  ok = 1;
done:
  if (!ok)
    fprintf(stderr, "dynamic render probe FAILED: %s\n", error);
  bk_renderer_destroy(other);
  bk_mesh_destroy(r, lit_mesh);
  bk_mesh_destroy(r, plain_mesh);
  bk_light_set_destroy(r, lights);
  bk_texture_destroy(r, texture);
  bk_renderer_destroy(r);
  return ok ? 0 : 1;
}
