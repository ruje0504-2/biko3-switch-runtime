/* Real4D00FA owners with explicit draw/input fixtures, not a story entry.
 * Compare completed GPU geometry against the already native-verified CPU
 * ENVL/material/effect components. No image export or production readback. */
#include "model/material.h"
#include "model/skin.h"
#include "scene/ending_normal_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
  fprintf(stderr, "secondary render group%u background%u line%d: %s: %s\n", \
          group, variant, __LINE__, #x, e); goto done; } } while (0)

typedef struct {
  const BkModel *model;
  BkActorPose *pose;
  BkModelSkin *skin;
  BkSkinMesh **meshes;
  uint8_t *seen;
} Reference;
static unsigned profiles, frames, dual_frames, empty_frames, hidden_frames;
static unsigned redraws, queue_items, mixed_profiles, rejected;
static uint64_t vertices, model_vertices, face_vertices;
static double worst;

static int submit(void *p, const int16_t *pcm, size_t n, char e[256]) {
  (void)pcm; (void)e;
  *(uint64_t *)p += n;
  return 1;
}
static int poll(void *p, uint64_t *out, char e[256]) {
  (void)e;
  *out = *(uint64_t *)p;
  return 1;
}
static void release_reference(Reference *r) {
  if (r->meshes)
    for (uint32_t i = 0; i < r->model->submesh_count; ++i)
      bk_skin_mesh_destroy(r->meshes[i]);
  bk_model_skin_destroy(r->skin);
  free(r->meshes);
  free(r->seen);
}
static int reference(Reference *r, BkActorPose *pose, char e[256]) {
  r->pose = pose;
  r->model = bk_actor_pose_model(pose);
  if (!r->model || !r->model->submesh_count)
    return 0;
  r->meshes = calloc(r->model->submesh_count, sizeof(*r->meshes));
  r->seen = calloc((size_t)r->model->submesh_count * 2, 1);
  if (!r->meshes || !r->seen)
    return 0;
  if (!bk_model_chunk(r->model, "ENVL"))
    return 1;
  r->skin = bk_model_skin_create(r->model, e);
  if (!r->skin)
    return 0;
  for (uint32_t i = 0; i < bk_model_skin_count(r->skin); ++i) {
    const BkSkinEntry *entry = bk_model_skin_entry(r->skin, i);
    if (!entry || entry->submesh >= r->model->submesh_count ||
        r->meshes[entry->submesh])
      return 0;
    r->meshes[entry->submesh] = bk_skin_mesh_create(r->skin, i,
        r->model->submeshes + entry->submesh, e);
    if (!r->meshes[entry->submesh])
      return 0;
  }
  return 1;
}
static int equal_geometry(const BkLitVertex *got, const BkModelVertex *want,
                           uint32_t count, const BkModelMaterial *material,
                           char e[256]) {
  BkMaterialState state;
  if (!bk_material_state(material, 0, &state, e))
    return 0;
  for (uint32_t i = 0; i < count; ++i) {
    const float position[] = {got[i].base.x, got[i].base.y, got[i].base.z};
    for (unsigned j = 0; j < 6; ++j) {
      float a = j < 3 ? position[j] : got[i].normal[j - 3];
      float b = j < 3 ? want[i].position[j] : want[i].normal[j - 3];
      double d = fabs((double)a - b) / fmax(1, fabs(b));
      if (d > worst) worst = d;
      if (!isfinite(d) || d > 3e-5) {
        snprintf(e, 256, "vertex%u field%u got%.9g want%.9g", i, j, a, b);
        return 0;
      }
    }
    const float diffuse[] = {got[i].base.r, got[i].base.g,
                              got[i].base.b, got[i].base.a};
    if (memcmp(diffuse, state.diffuse, sizeof(diffuse)) ||
        memcmp(got[i].ambient, material->ambient, sizeof(got[i].ambient)) ||
        memcmp(got[i].emissive, material->emissive, sizeof(got[i].emissive)) ||
        memcmp(got[i].specular, material->specular, sizeof(got[i].specular)) ||
        got[i].power != material->power || got[i].base.u != want[i].uv[0][0] ||
        got[i].base.v != want[i].uv[0][1]) {
      snprintf(e, 256, "vertex%u lost live material or UV fields", i);
      return 0;
    }
    ++vertices;
  }
  return 1;
}
static int draw(BkRenderer *renderer, BkEndingNormalRender *render,
                 const BkViewport *viewport, char e[256]) {
  return bk_renderer_begin(renderer, e) &&
         bk_renderer_viewport(renderer, viewport, e) &&
         bk_ending_normal_render_draw(render, e) &&
         bk_renderer_end(renderer, e);
}
static int profile(BkRenderer *renderer, BkResourceStore *store,
                     BkResourceStore *missing_movie, unsigned group,
                     unsigned variant, int retained, char e[256]) {
  int ok = 0;
  BkEndingSecondaryAssets *assets = NULL;
  BkEndingNormalAssets *previous = NULL;
  BkEndingNormalRender *render = NULL;
  BkAudio *mixer = NULL;
  BkEndingAudio *audio = NULL;
  BkLitVertex *readback = NULL;
  BkEndingSpecialMaterial *materials = NULL;
  Reference ref[2] = {0};
  uint32_t random = 123, clocks[] = {100, 110, 120, 130};
  BkMenuCamera camera;
  BkEndingCameraPresets presets;
  CHECK(bk_menu_camera_dialogue(&camera));
  if (retained) {
    previous = bk_ending_normal_assets_create(store, group, variant, clocks,
        &random, &camera, &presets, e);
    CHECK(previous && bk_ending_normal_assets_load_background(previous, store, e));
    assets = bk_ending_secondary_assets_create_reloaded(store, group, variant,
        bk_ending_normal_assets_background(previous), clocks, &random,
        &camera, &presets, e);
  } else
    assets = bk_ending_secondary_assets_create(store, group, variant, clocks,
        &random, &camera, &presets, e);
  CHECK(assets && camera.fov == .2f);
  bk_ending_normal_assets_destroy(previous);
  previous = NULL;
  CHECK(bk_eye_assets_selected(bk_ending_secondary_assets_eyes(assets)) == 1);
  CHECK(bk_ending_secondary_assets_load_background(assets, store, e));
  BkActorForest *forest = bk_ending_secondary_assets_forest(assets);
  CHECK(reference(ref, bk_ending_secondary_assets_pose(assets, 0), e));
  CHECK(reference(ref + 1, bk_ending_secondary_assets_pose(assets, 3), e));
  uint32_t max_vertices = 0;
  size_t material_count = 0;
  for (unsigned a = 0; a < 2; ++a) {
    material_count += ref[a].model->material_count;
    for (uint32_t m = 0; m < ref[a].model->submesh_count; ++m)
      if (ref[a].model->submeshes[m].vertex_count > max_vertices)
        max_vertices = ref[a].model->submeshes[m].vertex_count;
  }
  readback = malloc((size_t)max_vertices * sizeof(*readback));
  materials = calloc(material_count, sizeof(*materials));
  CHECK(readback && materials);
  size_t entry = 0;
  for (unsigned a = 0; a < 2; ++a)
    for (uint32_t m = 0; m < ref[a].model->material_count; ++m)
      materials[entry++] = (BkEndingSpecialMaterial){
          bk_ending_secondary_assets_materials(assets, a ? 3 : 0), m};
  int model_owner = 0, face_owner = 0;
  for (uint32_t m = 0; m < ref[0].model->submesh_count; ++m) {
    const BkMorphMesh *model = bk_morph_group_mesh(
        bk_ending_secondary_assets_morph(assets, 0), m);
    const BkMorphMesh *face = bk_face_assets_mesh(
        bk_ending_secondary_assets_face(assets), m);
    CHECK(!(model && face));
    model_owner |= model != NULL;
    face_owner |= face != NULL;
  }
  mixed_profiles += model_owner && face_owner;
  BkRenderStats before = bk_renderer_stats(renderer);
  render = bk_ending_secondary_render_create(renderer, missing_movie, assets,
                                               1000, e);
  CHECK(!render);
  ++rejected;
  BkRenderStats after = bk_renderer_stats(renderer);
  CHECK(before.live_allocations == after.live_allocations &&
        before.live_bytes == after.live_bytes);
  render = bk_ending_secondary_render_create(renderer, store, assets, 1000, e);
  CHECK(render);
  CHECK(!bk_ending_normal_render_mesh(render, 1, 0) &&
        !bk_ending_normal_render_mesh(render, 2, 0) &&
        !bk_ending_normal_render_mesh(render, 4, 0));
  CHECK(bk_ending_normal_render_mesh(render, 0, 0) !=
        bk_ending_normal_render_view_mesh(render, 1, 0, 0));
  CHECK(!bk_ending_normal_render_view_mesh(render, 1, 3, 0));
  uint64_t submitted = 0;
  BkAudioSink sink = {&submitted, 48000, 240, 960, submit, poll};
  CHECK(mixer = bk_audio_create(&sink, e));
  CHECK(audio = bk_ending_audio_create(store, mixer, 0, e));
  CHECK(bk_ending_audio_bind(audio, 0, "bk3_02", "se002.wav", e));
  CHECK(bk_ending_audio_bind(audio, 1, "bk3_02", "se002.wav", e));
  BkEndingFrameState frame = {.phase = 2, .group = (uint8_t)group};
  int32_t action = (int32_t)variant, index = 0, offset = 0, duck = 0;
  uint8_t camera_variant = 1, restore_hidden = 0;
  uint32_t primary = bk_ending_secondary_assets_root(assets, 0), auxiliary = 0;
  uint32_t background = bk_ending_secondary_assets_root(assets, 3);
  uint32_t node = bk_actor_forest_node(forest, 0,
      bk_ending_secondary_assets_node(assets, 0));
  float cameras[BK_ENDING_SPECIAL_CAMERAS][4];
  CHECK(bk_ending_special_cameras(cameras, group));
  BkEndingSpecialBindings bindings = {&frame, &action, &camera_variant,
      &restore_hidden, &index, &offset, cameras,
      bk_actor_forest_world(forest, node) + 12, &primary, &auxiliary};
  BkViewport viewport = {8, 6, 48, 36};
  BkFog fog = {0};
  BkRenderStats stable = {0};
  for (unsigned step = 0; step < 48; ++step) {
    unsigned hidden_background = (step % 8) >= 4;
    unsigned hidden_primary = step % 12 == 9;
    CHECK(bk_actor_forest_visibility(forest, background, hidden_background, e));
    CHECK(bk_actor_forest_visibility(forest, primary, hidden_primary, e));
    CHECK(bk_ending_secondary_assets_advance(assets, 0, .125f, e));
    CHECK(bk_ending_secondary_assets_advance(assets, 3, .125f, e));
    const BkEndingSecondaryConfig *config = bk_ending_secondary_assets_config(assets);
    CHECK(bk_face_assets_step(bk_ending_secondary_assets_face(assets),
        bk_ending_secondary_assets_face_state(assets), config->expression_a,
        (float)((step * 17) % 100), 1100 + step * 125, 1101 + step * 125,
        1102 + step * 125, 1103 + step * 125, &random, e));
    CHECK(bk_eye_assets_select(bk_ending_secondary_assets_eyes(assets), step % 2, e));
    frame.phase = step % 12 == 11 ? 7 : step % 12 == 10 ? 9 : 2;
    index = (int32_t)((step * 7) % BK_ENDING_SPECIAL_CAMERAS);
    int playing;
    BkEndingAudioCall voice = {.operation = step % 2 ? BK_ENDING_AUDIO_RESTART
                                                      : BK_ENDING_AUDIO_PAUSE,
                               .slot = 1, .flags = 1, .volume = -1700};
    CHECK(bk_ending_audio_call(audio, group, action, 0, &voice, &playing, e));
    CHECK(bk_audio_playing(mixer, 1, &playing) && playing == (int)(step % 2));
    uint32_t old_movie = bk_ending_normal_render_movie_frame(render);
    BkEndingEventRenderInput event = {
        .roots = {.flow = 16, .group = (int32_t)group, .event_state = frame.phase,
                    .root_721b28 = primary, .root_721b34 = background},
        .event_mode = (int32_t)(step % 3),
        .scene = {.bindings = &bindings, .camera = &camera, .fog = &fog,
                    .primary_materials = bk_ending_secondary_assets_materials(assets, 0),
                    .materials = materials, .material_count = material_count,
                    .main_viewport = viewport, .special_viewport = {24, 6, 32, 24}},
        .audio = audio, .duck_transition = &duck, .voice_master = -1000,
        .seconds = .125f, .movie_clock = (int32_t)(1100 + step * 125),
        .movie_restart_clock = (int32_t)(1101 + step * 125)};
    if (!step) {
      BkEndingEventRenderInput invalid = event;
      invalid.roots.root_721b28 = 1;
      CHECK(!bk_ending_normal_render_prepare_event(render, &invalid, e));
      CHECK(!bk_ending_normal_render_pass_count(render));
      ++rejected;
      invalid = event;
      invalid.event_mode = 1;
      invalid.scene.special_viewport.width = 0;
      CHECK(!bk_ending_normal_render_prepare_event(render, &invalid, e));
      CHECK(!bk_ending_normal_render_pass_count(render));
      ++rejected;
    }
    CHECK(bk_ending_normal_render_prepare_event(render, &event, e));
    int dual = frame.phase != 7 &&
        (event.event_mode == 1 || (event.event_mode == 0 && playing));
    unsigned passes = bk_ending_normal_render_pass_count(render);
    CHECK(passes == (frame.phase == 7 ? 0u : 2u + (unsigned)dual));
    if (frame.phase == 7) CHECK(bk_ending_normal_render_movie_frame(render) == old_movie);
    if (passes && hidden_background) {
      CHECK(!bk_ending_normal_render_queue_count(render, 0));
      ++hidden_frames;
    }
    CHECK(draw(renderer, render, &viewport, e));
    for (unsigned a = 0; a < 2; ++a)
      memset(ref[a].seen, 0, (size_t)ref[a].model->submesh_count * 2);
    for (unsigned p = 0; p < passes; ++p) {
      unsigned view = bk_ending_normal_render_pass_view(render, p);
      CHECK(view == (p >= 2));
      for (uint32_t i = 0; i < bk_ending_normal_render_queue_count(render, p); ++i) {
        uint32_t actor, mesh_frame, mesh;
        CHECK(bk_ending_normal_render_queue_item(render, p, i, &actor, &mesh_frame, &mesh));
        CHECK(actor == 0 || (actor == 3 && view == 0 && !hidden_background));
        Reference *r = ref + (actor == 3);
        CHECK(mesh < r->model->submesh_count && mesh_frame < r->model->frame_count);
        ++queue_items;
        if (r->seen[view * r->model->submesh_count + mesh]) continue;
        r->seen[view * r->model->submesh_count + mesh] = 1;
        const BkModelSubmesh *sub = r->model->submeshes + mesh;
        const BkMorphMesh *morph = bk_ending_secondary_assets_mesh(assets, actor, mesh);
        const BkModelVertex *want = morph ? bk_morph_mesh_vertices(morph) : sub->vertices;
        if (bk_morph_group_mesh(bk_ending_secondary_assets_morph(assets, actor), mesh))
          model_vertices += sub->vertex_count;
        if (!actor && bk_face_assets_mesh(bk_ending_secondary_assets_face(assets), mesh))
          face_vertices += sub->vertex_count;
        if (r->meshes[mesh]) {
          size_t floats;
          const float *world = bk_actor_pose_world(r->pose, &floats);
          CHECK(bk_skin_mesh_apply(r->meshes[mesh], world, floats, want, sub->vertex_count, e));
          want = bk_skin_mesh_vertices(r->meshes[mesh]);
        }
        CHECK(bk_lit_mesh_readback(renderer,
            bk_ending_normal_render_view_mesh(render, view, actor, mesh),
            readback, sub->vertex_count, e));
        const BkModelMaterial *mat = bk_material_pose_material(
            bk_ending_secondary_assets_materials(assets, actor), sub->material_index);
        CHECK(equal_geometry(readback, want, sub->vertex_count, mat, e));
      }
    }
    if (step % 6 == 0 || frame.phase == 7) {
      uint8_t first[64 * 48 * 4], again[sizeof(first)];
      CHECK(bk_renderer_readback(renderer, first, sizeof(first), e));
      BkMenuCamera saved = camera;
      uint32_t rng = random, movie = bk_ending_normal_render_movie_frame(render);
      BkRenderStats held = bk_renderer_stats(renderer);
      CHECK(draw(renderer, render, &viewport, e));
      CHECK(bk_renderer_readback(renderer, again, sizeof(again), e));
      CHECK(!memcmp(first, again, sizeof(first)) && !memcmp(&saved, &camera, sizeof(saved)));
      CHECK(random == rng && movie == bk_ending_normal_render_movie_frame(render));
      CHECK(held.skin_dispatches == bk_renderer_stats(renderer).skin_dispatches);
      ++redraws;
    }
    BkRenderStats current = bk_renderer_stats(renderer);
    if (!step) stable = current;
    else CHECK(current.live_allocations == stable.live_allocations &&
               current.live_bytes == stable.live_bytes);
    dual_frames += dual;
    empty_frames += passes == 0;
    ++frames;
  }
  ++profiles;
  printf("secondary GPU group%u background%u:48 frames\n", group, variant);
  ok = 1;
done:
  bk_ending_normal_render_destroy(render);
  bk_ending_audio_destroy(audio);
  bk_audio_destroy(mixer);
  for (unsigned a = 0; a < 2; ++a) release_reference(ref + a);
  free(materials);
  free(readback);
  bk_ending_secondary_assets_destroy(assets);
  bk_ending_normal_assets_destroy(previous);
  return ok;
}
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--retained"))) return 2;
  int retained = argc == 3;
  char e[256] = {0}, path[1024];
  BkRenderer *renderer = NULL;
  BkResourceStore *store = NULL, *missing_movie = NULL;
  int rc = 1;
  renderer = bk_renderer_create(64, 48, stdout, e);
  store = bk_resources_create(e);
  missing_movie = bk_resources_create(e);
  if (!renderer || !store || !missing_movie) goto done;
  const char *packs[] = {"bk3_02", "bk3_03", "bk3_04", "bk3_09", "fambom", "bk3_18"};
  for (unsigned i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
    if (snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]) >= (int)sizeof(path) ||
        !bk_resources_mount(store, packs[i], path, e) ||
        (i + 1 < sizeof(packs) / sizeof(*packs) &&
         !bk_resources_mount(missing_movie, packs[i], path, e))) goto done;
  }
  if (retained) {
    if (snprintf(path, sizeof(path), "%s/bk3_08.pp", argv[1]) >= (int)sizeof(path) ||
        !bk_resources_mount(store, "bk3_08", path, e) ||
        !bk_resources_mount(missing_movie, "bk3_08", path, e)) goto done;
  }
  BkRenderStats baseline = bk_renderer_stats(renderer);
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned v = 0; v < 2; ++v) {
      if (!profile(renderer, store, missing_movie, g, v, retained, e)) goto done;
      BkRenderStats now = bk_renderer_stats(renderer);
      if (now.live_allocations != baseline.live_allocations || now.live_bytes != baseline.live_bytes) {
        snprintf(e, sizeof(e), "GPU resources remain after secondary owner retirement");
        goto done;
      }
    }
  if (profiles != 10 || !mixed_profiles || !model_vertices || !face_vertices ||
      !hidden_frames || !dual_frames || !bk_renderer_stats(renderer).skin_dispatches) {
    snprintf(e, sizeof(e), "missing secondary coverage");
    goto done;
  }
  printf("PASS secondary GPU retained=%d profiles=%u frames=%u dual=%u empty=%u hidden_background=%u "
         "mixed_profiles=%u queues=%u vertices=%llu model_morp_vertices=%llu face_vertices=%llu "
         "max_relative=%.9g redraws=%u rejected=%u; stable allocations\n",
      retained, profiles, frames, dual_frames, empty_frames, hidden_frames, mixed_profiles,
      queue_items, (unsigned long long)vertices, (unsigned long long)model_vertices,
      (unsigned long long)face_vertices, worst, redraws, rejected);
  rc = 0;
done:
  if (rc) fprintf(stderr, "%s\n", e);
  bk_resources_destroy(missing_movie);
  bk_resources_destroy(store);
  bk_renderer_destroy(renderer);
  return rc;
}
