#include "scene/special_audio.h"
#include "scene/special_render.h"
#include "model/skin.h"
#include "core/matrix.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Actual resources/services, with explicit offline clock/input/fog fixtures.
 * This does not implement the remaining flow48 UI or application lifecycle. */
#define WIDTH 320
#define HEIGHT 240
#define BYTES (WIDTH * HEIGHT * 4)
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "line%d: %s\n", __LINE__, error); goto done; } } while (0)
static uint64_t hash(uint64_t h, const void *data, size_t n) {
  const uint8_t *p = data;
  while (n--) h = (h ^ *p++) * UINT64_C(1099511628211);
  return h;
}
typedef struct {
  BkSpecialAudio *audio;
  BkSpecialRender *render;
  BkVoiceEnvelope envelope;
  uint64_t submitted, consumed, samples, nonzero, pcm;
  uint32_t now, query;
  unsigned movie_calls, loads, directs;
  int movie;
} Context;
static int submit(void *p, const int16_t *pcm, size_t frames, char e[256]) {
  (void)e; Context *c = p;
  c->pcm = hash(c->pcm, pcm, frames * 4);
  for (size_t i = 0; i < frames * 2; ++i) c->nonzero += pcm[i] != 0;
  c->submitted += frames; c->samples += frames * 2; return 1;
}
static int poll(void *p, uint64_t *out, char e[256]) { (void)e; *out = ((Context *)p)->consumed; return 1; }
static int key(void *p, uint32_t keycode, uint8_t *out, char e[256]) {
  (void)p; (void)keycode; (void)e; *out = 0; return 1; /* No held keys. */
}
static int present(void *p, BkSpecialEventObject object, int *out, char e[256]) {
  Context *c = p;
  if (object == BK_SPECIAL_MOVIE) { *out = c->movie; return 1; }
  if (object >= BK_SPECIAL_EFFECT0 && object <= BK_SPECIAL_EFFECT3)
    return bk_special_audio_present(c->audio, object - BK_SPECIAL_EFFECT0, out);
  snprintf(e, 256, "unexpected media query"); return 0;
}
static int audio_call(void *p, const BkSpecialEventAudioCall *call, char e[256]) {
  Context *c = p;
  c->loads += call->operation == BK_SPECIAL_EFFECT_LOAD;
  c->directs += call->operation == BK_SPECIAL_EFFECT_DIRECT_PLAY;
  return bk_special_audio_call(c->audio, call, -900, -600, -500, e);
}
static int movie(void *p, char e[256]) {
  Context *c = p; ++c->movie_calls;
  return bk_special_render_movie_step(c->render, (int32_t)c->now, (int32_t)c->now, e);
}
static int clock_read(void *p, int timer, uint32_t *out, char e[256]) {
  (void)timer; (void)e; Context *c = p; *out = c->now + c->query++; return 1;
}
static int level(void *p, unsigned effect, float seconds, float *out, char e[256]) {
  Context *c = p; return bk_special_audio_level(c->audio, effect, &c->envelope, seconds, out, e);
}
static int draw(BkRenderer *gpu, BkSpecialRender *r, uint8_t *pixels, char e[256]) {
  return bk_renderer_begin(gpu, e) && bk_special_render_draw(r, e) &&
    bk_renderer_end(gpu, e) && bk_renderer_readback(gpu, pixels, BYTES, e);
}
static int skin_check(BkRenderer *gpu, BkSpecialRender *r, BkSpecialWorld *w,
    uint64_t *vertices, double *worst, char e[256]) {
  BkActorPose *pose = bk_special_world_pose(w, BK_SPECIAL_WORLD_BODY);
  const BkModel *m = bk_actor_pose_model(pose);
  BkModelSkin *skin = bk_model_skin_create(m, e);
  if (!skin) return 0;
  int ok = 1;
  for (uint32_t i = 0; ok && i < bk_model_skin_count(skin); ++i) {
    const BkSkinEntry *entry = bk_model_skin_entry(skin, i);
    BkSkinMesh *cpu = bk_skin_mesh_create(skin, i, m->submeshes + entry->submesh, e);
    BkLitVertex *out = malloc(sizeof(*out) * entry->vertex_count);
    const BkMorphMesh *morph = bk_face_assets_mesh(bk_special_world_face(w), entry->submesh);
    const BkModelVertex *source = morph ? bk_morph_mesh_vertices(morph) : NULL;
    BkGpuMesh *mesh = bk_actor_render_mesh(bk_special_render_actor(r), m, entry->submesh);
    ok = cpu && out && mesh && bk_skin_mesh_apply(cpu, bk_actor_pose_frame(pose, 0),
      (size_t)m->frame_count * 16, source, source ? entry->vertex_count : 0, e) &&
      bk_lit_mesh_readback(gpu, mesh, out, entry->vertex_count, e);
    if (ok) {
      const BkModelVertex *want = bk_skin_mesh_vertices(cpu);
      for (uint32_t v = 0; ok && v < entry->vertex_count; ++v) {
        const float a[6] = {out[v].base.x, out[v].base.y, out[v].base.z,
                            out[v].normal[0], out[v].normal[1], out[v].normal[2]};
        for (unsigned k = 0; k < 6; ++k) {
          float b = k < 3 ? want[v].position[k] : want[v].normal[k - 3];
          double d = fabs((double)a[k] - b) / fmax(1, fabs(b));
          if (d > *worst) *worst = d;
          if (!isfinite(d) || d > 5e-5) {
            snprintf(e, 256, "special GPU skin entry%u vertex%u channel%u %.9g/%.9g", i, v, k, a[k], b);
            ok = 0; break;
          }
        }
        ++*vertices;
      }
    }
    free(out); bk_skin_mesh_destroy(cpu);
  }
  bk_model_skin_destroy(skin); return ok;
}
static int movie_bounds(BkRenderer *gpu, BkSpecialRender *r, BkSpecialWorld *w,
                         float center[3], float *radius, char e[256]) {
  BkActorPose *pose = bk_special_world_pose(w, BK_SPECIAL_WORLD_BODY);
  const BkModel *m = bk_actor_pose_model(pose);
  BkModelSkin *skin = bk_model_skin_create(m, e);
  if (!skin) return 0;
  float low[3] = {INFINITY, INFINITY, INFINITY}, high[3] = {-INFINITY, -INFINITY, -INFINITY};
  int ok = 1; unsigned matches = 0;
  for (uint32_t i = 0; ok && i < m->submesh_count; ++i) {
    const BkModelSubmesh *part = m->submeshes + i;
    if (!part->texture_count || strcmp(m->textures[part->texture_indices[0]].filename, "D_moza.bmp")) continue;
    int skinned = 0;
    for (uint32_t s = 0; s < bk_model_skin_count(skin); ++s)
      skinned |= bk_model_skin_entry(skin, s)->submesh == i;
    BkLitVertex *out = malloc(sizeof(*out) * part->vertex_count);
    ok = out && bk_lit_mesh_readback(gpu, bk_actor_render_mesh(bk_special_render_actor(r), m, i), out, part->vertex_count, e);
    for (uint32_t f = 0; ok && f < m->frame_count; ++f) {
      if (m->frames[f].mesh_index != part->mesh_index) continue;
      for (uint32_t v = 0; ok && v < part->vertex_count; ++v) {
        float p[3] = {out[v].base.x, out[v].base.y, out[v].base.z};
        if (!skinned) ok = bk_matrix_transform_coord(p, p, bk_actor_pose_frame(pose, f));
        for (unsigned k = 0; k < 3; ++k) { low[k] = fminf(low[k], p[k]); high[k] = fmaxf(high[k], p[k]); }
        ++matches;
      }
    }
    free(out);
  }
  bk_model_skin_destroy(skin);
  if (!ok || !matches) return 0;
  *radius = 0;
  for (unsigned k = 0; k < 3; ++k) {
    center[k] = (low[k] + high[k]) * .5f; *radius = fmaxf(*radius, high[k] - low[k]);
  }
  fprintf(stderr, "movie center %.3f %.3f %.3f radius %.3f\n", center[0], center[1], center[2], *radius);
  return *radius > 0;
}
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  char error[256] = {0}, path[1024]; int rc = 1;
  BkResourceStore *store = bk_resources_create(error), *incomplete = bk_resources_create(error);
  BkRenderer *gpu = NULL;
  BkSpecialWorld *world = NULL, *old_world = NULL;
  BkSpecialRender *render = NULL, *old_render = NULL;
  BkSpecialAudio *sound = NULL, *old_sound = NULL;
  Context c = {.pcm = UINT64_C(1469598103934665603)};
  BkAudio *mixer = bk_audio_create(&(BkAudioSink){&c, 48000, 480, 1920, submit, poll}, error);
  uint8_t *pixels = malloc(BYTES), *previous = malloc(BYTES);
  uint64_t images = UINT64_C(1469598103934665603), vertices = 0;
  unsigned frames = 0, draws = 0, redraws = 0, phase2 = 0, movie_changed = 0,
           movie_groups = 0, retirements = 0, rejected = 0;
  double worst = 0;
  BkMenuCamera camera = {0}; BkEndingCameraTransitions transitions = {0};
  BkSpecialEventState state = bk_special_event_initial();
  uint32_t random = 98765; uint8_t latches[426] = {0}, loops[4] = {9, 8, 7, 6};
  CHECK(store && incomplete && mixer && pixels && previous);
  const char *packs[] = {"bk3_01", "bk3_02", "bk3_04", "bk3_14", "bk3_18"};
  for (unsigned i = 0; i < 5; ++i) {
    snprintf(path, sizeof path, "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(store, packs[i], path, error));
    if (i != 4) CHECK(bk_resources_mount(incomplete, packs[i], path, error));
  }
  CHECK(bk_resources_mount_directory(store, "faces", argv[1], 20480, error));
  gpu = bk_renderer_create(WIDTH, HEIGHT, stderr, error); CHECK(gpu);
  for (int32_t group = 0; group < 5; ++group) {
    /* Explicit load fixture: shared process timers/RNG/loops stay retained;
     * phase/sequence are set at their documented outer-loader boundary. */
    memcpy(camera.pose.world, bk_identity, 64); memcpy(camera.matrix, bk_identity, 64);
    camera.pose.world[13] = 20;
    int32_t phase = 0, clip = 0, mode = 0, effect_volume = -500;
    float seconds = .25f, listener[4] = {0}, positions[4][4] = {{0}};
    int8_t paused = 0, wanted = 1, packed = 1; uint8_t visibility = 0;
    uint32_t clocks[4] = {1000, 1001, 1002, 1003};
    c.now = 1000; state.sequence = 0;
    world = bk_special_world_create(store, (unsigned)group, seconds, &camera,
      &transitions, clocks, &random, latches, sizeof latches, error); CHECK(world);
    c.movie = bk_special_world_needs_movie(world);
    if (c.movie) {
      uint64_t allocations = bk_renderer_stats(gpu).live_allocations;
      assert(!bk_special_render_create(gpu, incomplete, world, 1000, error));
      assert(bk_renderer_stats(gpu).live_allocations == allocations); ++rejected;
    }
    render = bk_special_render_create(gpu, store, world, 1000, error); CHECK(render);
    if (old_sound) CHECK(bk_special_audio_stop(old_sound, error));
    sound = bk_special_audio_create(store, mixer, 12, group, effect_volume, loops, error); CHECK(sound);
    c.audio = sound; c.render = render;
    if (old_render) {
      CHECK(draw(gpu, old_render, pixels, error)); assert(!memcmp(pixels, previous, BYTES));
      ++redraws; ++retirements;
      bk_special_render_destroy(old_render); old_render = NULL;
      bk_special_world_destroy(old_world); old_world = NULL;
      CHECK(bk_special_audio_stop(old_sound, error));
      bk_special_audio_destroy(old_sound); old_sound = NULL;
      int playing = 0; CHECK(bk_audio_playing(mixer, 16, &playing)); assert(playing);
    }
    BkSpecialEventBindings b = {&state, &group, &phase, &clip, &mode, &seconds,
      &paused, &wanted, &packed, &visibility, loops, &effect_volume, listener,
      {positions[0], positions[1], positions[2], positions[3]}, &c.envelope};
    BkSpecialWorldServices services = {&c, key, present, audio_call, movie, clock_read, level};
    BkFog fog = {0}; /* Retained device fog is an explicit test fixture. */
    for (unsigned f = 0; f < 360; ++f) {
      c.now = 1000 + f * 250; c.query = 0;
      paused = f % 37 == 0; visibility = (f / 24) % 2;
      if (phase == 2 && group != 1) { mode = (f / 60) % 3; clip = (f / 120) % 3; }
      c.consumed = c.submitted; CHECK(bk_audio_poll(mixer, error));
      CHECK(bk_special_world_step(world, &b, (float[2]){.2f, -.1f}, f % 4, &services, error));
      CHECK(bk_audio_fill(mixer, error));
      ++frames; phase2 += phase == 2;
      if (f % 12 == 0 || f == 359) {
        CHECK(bk_special_render_prepare(render, &camera, &fog, error));
        assert(bk_special_render_submissions(render) > 0);
        CHECK(draw(gpu, render, pixels, error)); ++draws;
        images = hash(images, pixels, BYTES); memcpy(previous, pixels, BYTES);
        if (f % 120 == 0) CHECK(skin_check(gpu, render, world, &vertices, &worst, error));
        uint32_t movie_frame = bk_special_render_movie_frame(render);
        BkRenderStats before = bk_renderer_stats(gpu);
        CHECK(draw(gpu, render, pixels, error)); ++redraws;
        assert(!memcmp(pixels, previous, BYTES));
        assert(bk_special_render_movie_frame(render) == movie_frame);
        assert(bk_renderer_stats(gpu).skin_dispatches == before.skin_dispatches);
      }
    }
    assert(phase == 2);
    if (group == 1) assert(state.sequence == 2);
    if (c.movie) {
      /* Actual automatic close-ups may omit the movie surface. Diagnostic
       * views hold geometry/lights while only that borrowed texture changes. */
      CHECK(bk_actor_forest_draw_disable(bk_special_world_forest(world),
        bk_special_world_root(world, BK_SPECIAL_WORLD_BODY), 0, error));
      unsigned changed_group = 0;
      float center[3], radius;
      CHECK(movie_bounds(gpu, render, world, center, &radius, error));
      const float directions[6][3] = {{0,0,-1},{1,0,0},{0,0,1},{-1,0,0},{.1f,1,0},{.1f,-1,0}};
      for (unsigned side = 0; side < 6; ++side) {
        BkMenuCamera inspection = camera;
        memcpy(inspection.pose.world, bk_identity, 64);
        for (unsigned k = 0; k < 3; ++k) inspection.pose.world[12+k] = center[k] + directions[side][k] * radius * 1.5f;
        inspection.fov = 1.2f;
        CHECK(bk_camera_aim(inspection.pose.world, inspection.pose.world, center));
        CHECK(bk_actor_forest_anchor(bk_special_world_forest(world), 1, inspection.pose.world, 0, error));
        CHECK(bk_special_render_prepare(render, &inspection, &fog, error));
        CHECK(draw(gpu, render, previous, error)); ++draws;
        CHECK(bk_special_render_movie_step(render, (int32_t)c.now + 137 + side * 137,
          (int32_t)c.now + 137 + side * 137, error));
        CHECK(draw(gpu, render, pixels, error)); ++draws;
        for (unsigned p = 0; p < WIDTH * HEIGHT; ++p)
          changed_group += memcmp(pixels + p * 4, previous + p * 4, 4) != 0;
        memcpy(previous, pixels, BYTES);
      }
      assert(changed_group); movie_changed += changed_group; movie_groups |= 1u << group;
    }
    old_render = render; render = NULL; old_world = world; world = NULL;
    old_sound = sound; sound = NULL;
    fprintf(stderr, "PASS special media group%d\n", group);
  }
  CHECK(bk_special_audio_stop(old_sound, error));
  assert(c.nonzero && phase2 && c.loads && c.directs == 2 && movie_groups == 18);
  printf("PASS special media: frames=%u draws=%u redraws=%u retirements=%u rejected=%u phase2=%u loads=%u direct=%u movie_calls=%u movie_changed=%u vertices=%llu worst=%.9g image=%016llx pcm=%016llx samples=%llu\n",
    frames, draws, redraws, retirements, rejected, phase2, c.loads, c.directs, c.movie_calls,
    movie_changed, (unsigned long long)vertices, worst, (unsigned long long)images,
    (unsigned long long)c.pcm, (unsigned long long)c.samples);
  rc = 0;
done:
  bk_special_render_destroy(render); bk_special_render_destroy(old_render);
  bk_special_world_destroy(world); bk_special_world_destroy(old_world);
  bk_special_audio_destroy(sound); bk_special_audio_destroy(old_sound);
  bk_audio_destroy(mixer); bk_renderer_destroy(gpu);
  bk_resources_destroy(store); bk_resources_destroy(incomplete);
  free(pixels); free(previous); return rc;
}
