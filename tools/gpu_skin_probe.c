#include "model/animation.h"
#include "scene/skin_upload.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static double worst_position, worst_normal;
static uint64_t vertices_checked;
static unsigned models_checked, meshes_checked, frames_checked;
static void convert(BkLitVertex *out, const BkModelVertex *in, unsigned n,
                    unsigned variant) {
  for (unsigned i = 0; i < n; i++) {
    out[i] = (BkLitVertex){.base = {in[i].position[0], in[i].position[1],
                                    in[i].position[2],
                                    in[i].uv[0][0] + variant * .125f,
                                    in[i].uv[0][1], .3f, .4f, .5f, .6f},
                           .ambient = {.3f, .4f, .5f},
                           .power = 3};
    memcpy(out[i].normal, in[i].normal, 12);
  }
}
static int probe(BkRenderer *r, const BkModel *model, const char *name,
                 char error[256]) {
  BkModelSkin *skin = bk_model_skin_create(model, error);
  if (!skin)
    return 0;
  unsigned count = bk_model_skin_count(skin), max = 0;
  if (!count) {
    bk_model_skin_destroy(skin);
    return 1;
  }
  BkSkinMesh **cpu = calloc(count, sizeof(*cpu));
  BkGpuMesh **gpu = calloc(count, sizeof(*gpu));
  BkSkinPalette *palette = bk_skin_palette_create(r, model->frame_count, error);
  BkModelAnimation *anim = bk_model_animation_create(model, error);
  if (!anim && strcmp(error, "ANIM: duplicate target frame")) {
    bk_skin_palette_destroy(r, palette);
    bk_model_skin_destroy(skin);
    free(cpu);
    free(gpu);
    return 0;
  }
  float *world = malloc((size_t)model->frame_count * 64);
  BkLitVertex *input = NULL, *output = NULL, *expected = NULL;
  int ok = 0;
  if (!cpu || !gpu || !palette || !world)
    goto done;
  for (unsigned i = 0; i < count; i++) {
    const BkSkinEntry *e = bk_model_skin_entry(skin, i);
    if (e->vertex_count > max)
      max = e->vertex_count;
  }
  input = malloc((size_t)max * sizeof(*input));
  output = malloc((size_t)max * sizeof(*output));
  expected = malloc((size_t)max * sizeof(*expected));
  if (!input || !output || !expected)
    goto done;
  for (unsigned i = 0; i < count; i++) {
    const BkSkinEntry *e = bk_model_skin_entry(skin, i);
    const BkModelSubmesh *sub = model->submeshes + e->submesh;
    cpu[i] = bk_skin_mesh_create(skin, i, sub, error);
    convert(input, sub->vertices, e->vertex_count, 0);
    gpu[i] = bk_lit_mesh_create(r, input, e->vertex_count,
                                (uint16_t[3]){0, 0, 0}, 3, error);
    if (!cpu[i] || !gpu[i] ||
        !bk_skin_upload(r, gpu[i], palette, skin, i, error))
      goto done;
  }
  if (!anim)
    printf("Static ENVL fixture %s: %s\n", name, error);
  for (unsigned frame = 0; frame < 8; frame++) {
    float time = (float)(frame * .137 * bk_model_animation_duration(anim));
    if (!(anim ? bk_model_animation_sample(anim, time, 1, world,
                                           (size_t)model->frame_count * 16,
                                           error)
               : bk_model_world_matrices(
                     model, world, (size_t)model->frame_count * 16, error)))
      goto done;
    if (frame == 6)
      for (unsigned i = 0; i < model->frame_count; i++) {
        world[i * 16 + 12] += .125f;
        world[i * 16 + 13] -= .25f;
      }
    if (!bk_skin_palette_update(r, palette, world, model->frame_count, error))
      goto done;
    if (frame == 3) {
      float saved = world[0];
      world[0] = NAN;
      int accepted =
          bk_skin_palette_update(r, palette, world, model->frame_count, error);
      world[0] = saved;
      if (accepted || bk_skin_palette_update(r, palette, world,
                                             model->frame_count - 1, error)) {
        snprintf(error, 256, "invalid palette update accepted");
        goto done;
      }
    }
    /* Immutable attributes change twice; source restoration must survive
     * compute. */
    if (frame == 2 || frame == 5)
      for (unsigned i = 0; i < count; i++) {
        const BkSkinEntry *e = bk_model_skin_entry(skin, i);
        const BkModelSubmesh *sub = model->submeshes + e->submesh;
        convert(input, sub->vertices, e->vertex_count, frame == 2);
        if (!bk_lit_mesh_update(r, gpu[i], input, e->vertex_count, error))
          goto done;
      }
    if (!bk_renderer_begin(r, error))
      goto done;
    if (bk_skin_palette_update(r, palette, world, model->frame_count, error)) {
      snprintf(error, 256, "active palette update accepted");
      goto done;
    }
    if (!bk_renderer_end(r, error))
      goto done;
    for (unsigned i = 0; i < count; i++) {
      const BkSkinEntry *e = bk_model_skin_entry(skin, i);
      if (!bk_skin_mesh_apply(cpu[i], world, (size_t)model->frame_count * 16,
                              NULL, 0, error) ||
          !bk_lit_mesh_readback(r, gpu[i], output, e->vertex_count, error))
        goto done;
      convert(expected, bk_skin_mesh_vertices(cpu[i]), e->vertex_count,
              frame >= 2 && frame < 5);
      for (unsigned v = 0; v < e->vertex_count; v++) {
        float a[22], b[22];
        memcpy(a, output + v, sizeof(a));
        memcpy(b, expected + v, sizeof(b));
        for (unsigned c = 0; c < 22; c++) {
          double d = fabs((double)a[c] - b[c]) / fmax(1, fabs(b[c]));
          double *worst = c < 3 ? &worst_position : &worst_normal;
          if ((c < 3 || (c >= 9 && c < 12)) && d > *worst)
            *worst = d;
          if (!isfinite(d) || d > ((c < 3 || (c >= 9 && c < 12)) ? 5e-5 : 0)) {
            snprintf(error, 256,
                     "GPU skin %s frame%u mesh%u vertex%u component%u: "
                     "%.9g/%.9g rel%.9g",
                     name, frame, i, v, c, a[c], b[c], d);
            goto done;
          }
        }
        vertices_checked++;
      }
    }
    frames_checked++;
  }
  /* An unchanged palette must not dispatch again. */
  uint64_t dispatched = bk_renderer_stats(r).skin_dispatches;
  if (!bk_skin_palette_update(r, palette, world, model->frame_count, error) ||
      !bk_renderer_begin(r, error) || !bk_renderer_end(r, error))
    goto done;
  if (bk_renderer_stats(r).skin_dispatches != dispatched) {
    snprintf(error, 256, "unchanged palette dispatched");
    goto done;
  }
  /* Queue changed work then release palette ownership before meshes. */
  world[12] += .25f;
  if (!bk_skin_palette_update(r, palette, world, model->frame_count, error))
    goto done;
  bk_skin_palette_destroy(r, palette);
  palette = NULL;
  models_checked++;
  meshes_checked += count;
  ok = 1;
done:
  for (unsigned i = 0; i < count; i++) {
    if (gpu)
      bk_mesh_destroy(r, gpu[i]);
    if (cpu)
      bk_skin_mesh_destroy(cpu[i]);
  }
  bk_skin_palette_destroy(r, palette);
  bk_model_animation_destroy(anim);
  bk_model_skin_destroy(skin);
  free(cpu);
  free(gpu);
  free(input);
  free(output);
  free(expected);
  free(world);
  return ok;
}
static void word(uint8_t *p, uint32_t value) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(value >> (i * 8));
}
static void number(uint8_t *p, float value) {
  uint32_t bits;
  memcpy(&bits, &value, 4);
  word(p, bits);
}
static int boundary_probe(BkRenderer *r, char error[256]) {
  for (unsigned test = 0; test < 100; test++) {
    uint8_t raw[72 + 80 + 2 * (8 + 4 * 32)] = {0};
    word(raw + 68, 1);
    word(raw + 140, 20);
    word(raw + 144, 100);
    word(raw + 148, 2);
    float weights[2][4] = {{.001f, .6f, .999f, .2f}, {.5f, .3995f, .2f, .3f}};
    if (test & 1)
      weights[0][0] = nextafterf(.001f, INFINITY);
    if (test & 2)
      weights[0][1] = .59949994f;
    if (test & 4)
      weights[1][2] = -.25f;
    BkModelFrame frames[2] = {
        {.id = 20, .parent_index = BK_MODEL_NONE, .mesh_index = BK_MODEL_NONE},
        {.id = 21, .parent_index = BK_MODEL_NONE, .mesh_index = BK_MODEL_NONE}};
    for (unsigned b = 0; b < 2; b++) {
      for (unsigned k = 0; k < 4; k++)
        frames[b].local[k * 5] = 1;
      frames[b].local[0] = .3f + test * .033f;
      frames[b].local[5] = b ? 3 : 1;
      frames[b].local[4] = test * .01f;
      frames[b].local[12] = b ? 10 : -.37f;
      uint8_t *p = raw + 152 + b * 136;
      word(p, 20 + b);
      word(p + 4, 4);
      p += 8;
      for (unsigned i = 0; i < 4; i++) {
        number(p + i * 12, b ? 3 : 1);
        number(p + 48 + i * 12 + 4, 2);
        word(p + 96 + i * 4, (test & 8) && i == 3 ? 1 : i);
        number(p + 112 + i * 4, weights[b][i]);
      }
    }
    BkModelVertex vertices[5] = {0};
    vertices[4].position[0] = 999;
    vertices[4].beta = .7f;
    BkModelSubmesh sub = {.id = 100, .vertex_count = 5, .vertices = vertices};
    BkModelChunk chunk = {.tag = "ENVL", .size = sizeof(raw)};
    BkModel model = {.source = raw,
                     .source_size = sizeof(raw),
                     .chunks = &chunk,
                     .chunk_count = 1,
                     .frames = frames,
                     .frame_count = 2,
                     .submeshes = &sub,
                     .submesh_count = 1};
    if (!probe(r, &model, "synthetic ordered-weight boundary", error))
      return 0;
  }
  return 1;
}
int main(int argc, char **argv) {
  if (argc < 2)
    return 2;
  char error[256] = {0};
  BkRenderer *r = bk_renderer_create(16, 16, stderr, error);
  int rc = 1;
  if (!r)
    goto done;
  for (int arg = 1; arg < argc; arg++) {
    BkArchive archive = {0};
    if (!bk_archive_open(&archive, argv[arg], error))
      goto done;
    for (uint32_t i = 0; i < archive.count; i++) {
      const BkEntry *e = archive.entries + i;
      size_t n = strlen(e->name);
      if (n < 2 || strcmp(e->name + n - 2, ".x"))
        continue;
      uint8_t *bytes = NULL;
      BkModel *model = NULL;
      if (!bk_archive_read(&archive, e, &bytes, error)) {
        bk_archive_close(&archive);
        goto done;
      }
      if (e->size >= 4 && !memcmp(bytes, "xof ", 4)) {
        free(bytes);
        continue;
      }
      if (bk_model_decode(bytes, e->size, &model, error) != BK_MODEL_OK) {
        free(bytes);
        bk_archive_close(&archive);
        goto done;
      }
      free(bytes);
      if (bk_model_chunk(model, "ENVL") && !probe(r, model, e->name, error)) {
        bk_model_destroy(model);
        bk_archive_close(&archive);
        goto done;
      }
      bk_model_destroy(model);
    }
    bk_archive_close(&archive);
  }
  if (!boundary_probe(r, error) || !bk_renderer_begin(r, error) ||
      !bk_renderer_end(r, error))
    goto done;
  printf("PASS GPU skin: models%u meshes%u frames%u vertices%llu "
         "position_error%.9g normal_error%.9g\n",
         models_checked, meshes_checked, frames_checked,
         (unsigned long long)vertices_checked, worst_position, worst_normal);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s\n", error);
  bk_renderer_destroy(r);
  return rc;
}
