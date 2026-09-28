/* Real MATA values on diagnostic lit quads: no game/ending camera claim. */
#include "model/material_animation.h"
#include "scene/actor_render.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 16
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
static double clamp(double x) { return fmin(1, fmax(0, x)); }
static uint64_t digest(uint64_t h, const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i++)
    h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  int rc = 1;
  char e[256] = {0}, path[1024], name[32];
  BkRenderer *r = NULL;
  BkLightSet *light = NULL;
  BkResourceStore *store = bk_resources_create(e);
  BkModel *source = NULL;
  BkBlob raw = {0};
  BkMaterialAnimation *animation = NULL;
  BkMaterialPose *materials = NULL;
  BkActorPose *pose = NULL;
  BkActorRender *render = NULL;
  BkClipSet *clips = NULL;
  uint8_t pixels[W * W * 4], previous[sizeof(pixels)];
  uint64_t hash = UINT64_C(14695981039346656037);
  unsigned frames = 0, changed = 0, checks = 0, max_error = 0;
  CHECK(store);
  for (unsigned p = 0; p < 2; p++) {
    snprintf(name, sizeof(name), "bk3_%02u", p ? 8 : 3);
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], name);
    CHECK(bk_resources_mount(store, name, path, e));
  }
  unsigned char xan[0x5190] = {0};
  memcpy(xan, "fixture.x", 10);
  memcpy(xan + 256, "fixture.x", 10);
  clips = bk_clip_set_decode(xan, sizeof(xan), e);
  CHECK(clips);
  r = bk_renderer_create(W, W, stderr, e);
  CHECK(r);
  BkLighting lighting = {.ambient = {.2f, .3f, .4f},
                         .point_count = 1,
                         .points = {{.position = {0, 0, 2},
                                     .range = 100,
                                     .diffuse = {.4f, .3f, .2f},
                                     .attenuation0 = 1,
                                     .ambient = {.1f, .05f, .025f},
                                     .specular = {.1f, .2f, .3f}}}};
  light = bk_light_set_create(r, &lighting, e);
  CHECK(light);
  CHECK(bk_light_set_view(r, light, (float[3]){0, 0, 2}, e));
  for (unsigned profile = 0; profile < 7; profile++) {
    const char *pack = profile < 5 ? "bk3_08" : "bk3_03";
    if (profile < 5)
      snprintf(name, sizeof(name), "h%02u_01.x", profile + 1);
    else
      snprintf(name, sizeof(name), "m00_%02u.x", profile == 5 ? 10 : 12);
    CHECK(bk_resources_read(store, pack, name, &raw, e) == BK_RESOURCE_OK);
    CHECK(bk_model_decode(raw.data, raw.size, &source, e) == BK_MODEL_OK);
    bk_blob_free(&raw);
    animation = bk_material_animation_create(source, e);
    CHECK(animation);
    const BkMaterialTrack *track = bk_material_animation_track(animation, 0);
    CHECK(track);
    uint8_t header[332] = {0};
    uint16_t indices[] = {0, 1, 2, 0, 2, 3};
    BkModelVertex vertices[4] = {
        {.position = {-.8f, -.8f, .5f}, .normal = {0, 0, 1}},
        {.position = {.8f, -.8f, .5f}, .normal = {0, 0, 1}},
        {.position = {.8f, .8f, .5f}, .normal = {0, 0, 1}},
        {.position = {-.8f, .8f, .5f}, .normal = {0, 0, 1}}};
    BkModelFrame frame = {
        .id = 1, .parent_index = BK_MODEL_NONE, .mesh_index = 0};
    memcpy(frame.local, bk_identity, 64);
    BkModelMesh mesh = {.id = 2, .submesh_count = 1};
    BkModelSubmesh sub = {.id = 3,
                          .material_index = track->material_index,
                          .material_id = track->material_id,
                          .vertex_count = 4,
                          .index_count = 6,
                          .vertices = vertices,
                          .indices = indices};
    BkModel model = {.source = header,
                     .source_size = sizeof(header),
                     .materials = source->materials,
                     .material_count = source->material_count,
                     .frames = &frame,
                     .frame_count = 1,
                     .meshes = &mesh,
                     .mesh_count = 1,
                     .submeshes = &sub,
                     .submesh_count = 1,
                     .vertex_count = 4,
                     .triangle_count = 2};
    materials = bk_material_pose_create(&model, e);
    pose = bk_actor_pose_create_loaded(&model, clips, 0, (float[3]){0}, 0, e);
    render = bk_actor_render_create(r, store, pack, &model, NULL, e);
    CHECK(materials && pose && render);
    bk_actor_pose_publish(pose);
    const BkRenderStats initial = bk_renderer_stats(r);
    for (unsigned step = 0; step < 60; step++) {
      const BkMaterialKey *key = bk_material_animation_key(
          animation, 0, (step / 4) % track->key_count);
      float time = step % 3 == 2 ? bk_material_animation_time(animation)
                                 : key->time + (step % 3 == 1 ? .375f : 0);
      CHECK(bk_material_animation_sample(animation, time, materials, e));
      CHECK(bk_actor_render_prepare(render, pose, materials, NULL, bk_identity,
                                    bk_identity, e));
      BkActorRenderStats stats;
      CHECK(bk_actor_render_stats(render, &stats));
      if (step % 3 == 2)
        CHECK(stats.uploaded_vertices == 0);
      CHECK(bk_renderer_begin(r, e) && bk_actor_render_draw(render, light, e) &&
            bk_renderer_end(r, e));
      CHECK(bk_renderer_readback(r, pixels, sizeof(pixels), e));
      const BkModelMaterial *m =
          bk_material_pose_material(materials, track->material_index);
      BkMaterialState state;
      CHECK(bk_material_state(m, 0, &state, e));
      /* Symmetric vertices give equal incidence/half-angle at every corner,
       * making independent constant-color pixel comparison possible. */
      double dot = 1.5 / sqrt(1.5 * 1.5 + 2 * (double).8f * .8f),
             alpha = clamp(state.diffuse[3]);
      for (unsigned y = 4; y < 12; y++)
        for (unsigned x = 4; x < 12; x++)
          for (unsigned c = 0; c < 3; c++) {
            double base =
                m->emissive[c] +
                m->ambient[c] * (double)(lighting.ambient[c] +
                                         lighting.points[0].ambient[c]) +
                m->diffuse[c] * lighting.points[0].diffuse[c] * dot;
            double spec = m->power >= .01f
                              ? m->specular[c] *
                                    lighting.points[0].specular[c] *
                                    pow(dot, m->power)
                              : 0;
            double color = clamp(clamp(base) + clamp(spec));
            double expected =
                !state.visible || state.blend == BK_MATERIAL_INVERSE_COLOR
                    ? 0
                    : color * alpha;
            int want = (int)lround(clamp(expected) * 255),
                actual = pixels[(y * W + x) * 4 + c];
            unsigned delta = (unsigned)abs(actual - want);
            if (delta > max_error)
              max_error = delta;
            if (delta > 2) {
              snprintf(e, sizeof(e), "profile%u step%u c%u: %d != %d", profile,
                       step, c, actual, want);
              goto done;
            }
            checks++;
          }
      if (step && memcmp(previous, pixels, sizeof(pixels)))
        changed++;
      memcpy(previous, pixels, sizeof(pixels));
      hash = digest(hash, pixels, sizeof(pixels));
      frames++;
      BkRenderStats now = bk_renderer_stats(r);
      CHECK(now.live_allocations == initial.live_allocations &&
            now.live_bytes == initial.live_bytes);
    }
    bk_actor_render_destroy(render);
    render = NULL;
    bk_actor_pose_destroy(pose);
    pose = NULL;
    bk_material_pose_destroy(materials);
    materials = NULL;
    bk_material_animation_destroy(animation);
    animation = NULL;
    bk_model_destroy(source);
    source = NULL;
  }
  CHECK(changed > 20);
  printf("PASS MATA GPU:7 material fixtures %u frames %u RGB checks max%u/255 "
         "%u changes FNV%016llx; cached uploads and stable allocations\n",
         frames, checks, max_error, changed, (unsigned long long)hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "MATA GPU: %s\n", e);
  bk_actor_render_destroy(render);
  bk_actor_pose_destroy(pose);
  bk_material_pose_destroy(materials);
  bk_material_animation_destroy(animation);
  bk_model_destroy(source);
  bk_clip_set_destroy(clips);
  bk_blob_free(&raw);
  bk_light_set_destroy(r, light);
  bk_renderer_destroy(r);
  bk_resources_destroy(store);
  return rc;
}
