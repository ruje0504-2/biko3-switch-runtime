/* Two independent copies of the actual4d2320 assets. The CPU observer walks
 * its own forest at each original4d9898 draw boundary and keeps both BOM
 * sources there. It never obtains expected worlds/vertices from the GPU
 * renderer. Queue membership is inspected separately; this is a geometry,
 * material and lifetime check, not a Windows image or application oracle. */
#include "scene/ending_normal_render.h"
#include "model/material.h"
#include "model/skin.h"
#include "core/matrix.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
  fprintf(stderr, "third render group%u variant%u retained%u line%d: %s: %s\n", \
          group, variant, retained, __LINE__, #x, e); goto done; } } while (0)
static const unsigned roles[] = {0, 1, 2, 5};
typedef struct {
  BkModelVertex *vertices[2];
  BkModelMaterial material[2];
  BkSkinMesh *skin;
  uint8_t seen[2];
  uint8_t replay[2];
} Mesh;
typedef struct {
  const BkModel *model;
  BkActorPose *pose;
  BkModelSkin *skin;
  Mesh *meshes;
  uint32_t registry;
  uint8_t captured[2];
} Actor;
typedef struct {
  BkEndingTertiaryAssets *assets;
  BkEndingSpecialMaterial *materials;
  size_t material_count;
  Actor actors[4];
  BkModelVertex *source[2][5];
  float source_world[2][5][16];
  BkBomDeformBinding plans[5];
  const uint32_t *mapping[5];
  int32_t groups[5];
  uint32_t count;
  unsigned calls, events;
} Reference;
static uint64_t vertices, transfers, upper_transfers, lower_transfers;
static unsigned profiles, frames, dual_frames, redraws, empty_frames, rejected;
static unsigned hidden_backgrounds, model_morphs, face_morphs;
static unsigned redraw_skin_replays;
static double worst;

static int fail(char e[256], const char *why) {
  snprintf(e, 256, "third render reference: %s", why);
  return 0;
}
static int actor_index(const Reference *r, uint32_t registry) {
  for (unsigned i = 0; i < 4; ++i)
    if (r->actors[i].model && r->actors[i].registry == registry) return (int)i;
  return -1;
}
static void release_reference(Reference *r) {
  for (unsigned a = 0; a < 4; ++a) {
    Actor *actor = r->actors + a;
    if (actor->meshes)
      for (uint32_t m = 0; m < actor->model->submesh_count; ++m) {
        bk_skin_mesh_destroy(actor->meshes[m].skin);
        for (unsigned v = 0; v < 2; ++v) free(actor->meshes[m].vertices[v]);
      }
    free(actor->meshes);
    bk_model_skin_destroy(actor->skin);
  }
  for (unsigned v = 0; v < 2; ++v)
    for (unsigned i = 0; i < 5; ++i) free(r->source[v][i]);
  free(r->materials);
}
static BkEndingSpecialMaterial *material_registry(BkEndingTertiaryAssets *a,
                                                  unsigned retained,
                                                  size_t *count) {
  const unsigned fresh[] = {0, 1, 2, 5}, held[] = {5, 0, 1, 2};
  const unsigned *order = retained ? held : fresh;
  *count = 0;
  for (unsigned i = 0; i < 4; ++i) {
    const BkModel *model = bk_actor_pose_model(bk_ending_tertiary_assets_pose(a, order[i]));
    if (model) *count += model->material_count;
  }
  BkEndingSpecialMaterial *out = calloc(*count ? *count : 1, sizeof(*out));
  if (!out) return NULL;
  size_t n = 0;
  for (unsigned i = 0; i < 4; ++i) {
    const BkModel *model = bk_actor_pose_model(bk_ending_tertiary_assets_pose(a, order[i]));
    if (model)
      for (uint32_t m = 0; m < model->material_count; ++m)
        out[n++] = (BkEndingSpecialMaterial){bk_ending_tertiary_assets_materials(a, order[i]), m};
  }
  return out;
}
static int initialize_reference(Reference *r, BkEndingTertiaryAssets *assets,
                                unsigned retained, char e[256]) {
  r->assets = assets;
  r->materials = material_registry(assets, retained, &r->material_count);
  if (!r->materials) return fail(e, "material allocation failed");
  for (unsigned a = 0; a < 4; ++a) {
    Actor *actor = r->actors + a;
    actor->pose = bk_ending_tertiary_assets_pose(assets, roles[a]);
    if (!actor->pose) continue;
    actor->model = bk_actor_pose_model(actor->pose);
    actor->registry = bk_ending_tertiary_assets_registry(assets, roles[a]);
    actor->meshes = calloc(actor->model->submesh_count, sizeof(*actor->meshes));
    if (!actor->meshes) return fail(e, "mesh allocation failed");
    if (bk_model_chunk(actor->model, "ENVL")) {
      actor->skin = bk_model_skin_create(actor->model, e);
      if (!actor->skin) return 0;
    }
    for (uint32_t m = 0; m < actor->model->submesh_count; ++m) {
      const BkModelSubmesh *sub = actor->model->submeshes + m;
      for (unsigned v = 0; v < 2; ++v) {
        actor->meshes[m].vertices[v] = malloc((size_t)sub->vertex_count * sizeof(BkModelVertex));
        if (!actor->meshes[m].vertices[v]) return fail(e, "vertex allocation failed");
      }
    }
    for (uint32_t i = 0; i < bk_model_skin_count(actor->skin); ++i) {
      uint32_t m = bk_model_skin_entry(actor->skin, i)->submesh;
      if (m >= actor->model->submesh_count || actor->meshes[m].skin)
        return fail(e, "invalid skin registry");
      actor->meshes[m].skin = bk_skin_mesh_create(actor->skin, i, actor->model->submeshes + m, e);
      if (!actor->meshes[m].skin) return 0;
    }
  }
  BkBomDualAssets *bom = bk_ending_tertiary_assets_bom(assets);
  r->count = bk_bom_dual_assets_count(bom);
  if (r->count > 5) return fail(e, "unexpected binding count");
  for (unsigned i = 0; i < r->count; ++i) {
    size_t count;
    if (!bk_bom_dual_assets_plan(bom, i, r->plans + i) ||
        !bk_bom_dual_assets_mapping(bom, i, r->groups + i, r->mapping + i, &count) ||
        count != r->plans[i].count) return fail(e, "invalid owned mapping");
    const BkBomAssetMesh *source = bk_bom_dual_assets_mesh(bom, r->plans[i].source);
    if (!source || source->actor < 1 || source->actor > 2)
      return fail(e, "invalid source actor");
    uint32_t size = r->actors[source->actor].model->submeshes[source->submesh].vertex_count;
    for (unsigned v = 0; v < 2; ++v) {
      r->source[v][i] = malloc((size_t)size * sizeof(BkModelVertex));
      if (!r->source[v][i]) return fail(e, "source allocation failed");
    }
  }
  return 1;
}
static const BkModelVertex *sample(Reference *r, unsigned a, uint32_t m, char e[256]) {
  Actor *actor = r->actors + a;
  const BkModelSubmesh *sub = actor->model->submeshes + m;
  const BkMorphMesh *morph = a == 0
      ? bk_face_assets_mesh(bk_ending_tertiary_assets_face(r->assets), m) : NULL;
  if (!morph) morph = bk_ending_tertiary_assets_mesh(r->assets, roles[a], m);
  const BkModelVertex *vertices = morph ? bk_morph_mesh_vertices(morph) : sub->vertices;
  if (actor->meshes[m].skin) {
    size_t floats;
    const float *world = bk_actor_pose_world(actor->pose, &floats);
    if (!bk_skin_mesh_apply(actor->meshes[m].skin, world, floats, vertices, sub->vertex_count, e))
      return NULL;
    vertices = bk_skin_mesh_vertices(actor->meshes[m].skin);
  }
  return vertices;
}
static int capture_actor(Reference *r, unsigned a, unsigned view, char e[256]) {
  Actor *actor = r->actors + a;
  if (actor->captured[view]) return 1;
  for (uint32_t m = 0; m < actor->model->submesh_count; ++m) {
    const BkModelSubmesh *sub = actor->model->submeshes + m;
    const BkModelVertex *source = sample(r, a, m, e);
    const BkModelMaterial *material = bk_material_pose_material(
        bk_ending_tertiary_assets_materials(r->assets, roles[a]), sub->material_index);
    if (!source || !material) return fail(e, "missing live geometry/material");
    memcpy(actor->meshes[m].vertices[view], source, (size_t)sub->vertex_count * sizeof(*source));
    actor->meshes[m].material[view] = *material;
  }
  actor->captured[view] = 1;
  if (a != 0) return 1;
  BkBomDualAssets *bom = bk_ending_tertiary_assets_bom(r->assets);
  for (unsigned i = 0; i < r->count; ++i) {
    const BkBomAssetMesh *source = bk_bom_dual_assets_mesh(bom, r->plans[i].source);
    const BkModelVertex *vertices = sample(r, source->actor, source->submesh, e);
    const float *world = bk_actor_pose_frame(r->actors[source->actor].pose, source->frame);
    if (!vertices || !world) return fail(e, "missing draw-time BOM source");
    uint32_t n = r->actors[source->actor].model->submeshes[source->submesh].vertex_count;
    memcpy(r->source[view][i], vertices, (size_t)n * sizeof(*vertices));
    memcpy(r->source_world[view][i], world, 64);
  }
  return 1;
}
/* Observe the original descriptor's selected root walks. Publication and
 * special-scene mutations happen only on this independent CPU forest. */
static int capture_draw(void *p, const BkDrawDispatch *dispatch,
                         const BkMenuCamera *camera, char e[256]) {
  Reference *r = p;
  (void)camera;
  unsigned view = dispatch->mode == 2;
  const unsigned main_slots[] = {0, 4, 5}, second_slots[] = {20, 21};
  const unsigned *slots = view ? second_slots : main_slots;
  BkActorForest *forest = bk_ending_tertiary_assets_forest(r->assets);
  for (unsigned i = 0; i < (view ? 2u : 3u); ++i) {
    if (!dispatch->objects[slots[i]]) continue;
    const BkFrameVisit *walk;
    uint32_t count;
    if (!bk_actor_forest_draw(forest, dispatch->objects[slots[i]], &walk, &count, e)) return 0;
    unsigned actors = 0;
    for (uint32_t n = 0; n < count; ++n) {
      uint32_t registry, frame;
      if (!walk[n].submit || !bk_actor_forest_binding(forest, walk[n].node, &registry, &frame)) continue;
      int a = actor_index(r, registry);
      if (a >= 0) actors |= 1u << (unsigned)a;
    }
    for (unsigned a = 0; a < 4; ++a)
      if ((actors & (1u << a)) && !capture_actor(r, a, view, e)) return 0;
  }
  ++r->calls;
  return 1;
}
static int observe_event(void *p, BkEndingSpecialRenderEvent event, unsigned arg, char e[256]) {
  Reference *r = p;
  if ((unsigned)event > BK_ENDING_SPECIAL_BEGIN ||
      (event == BK_ENDING_SPECIAL_CLEAR && arg != 2) ||
      (event == BK_ENDING_SPECIAL_VIEWPORT && arg > 1))
    return fail(e, "invalid native render event");
  ++r->events;
  return 1;
}
static int transfer(const BkModelVertex *in, BkModelVertex *out, const float *m) {
  float position[4];
  bk_matrix_point(position, in->position, m);
  if (!isfinite(position[3]) || position[3] == 0) return 0;
  if (fabs((double)position[3] - 1) > (double)1e-5f)
    for (unsigned j = 0; j < 3; ++j) position[j] = (float)((double)position[j] / position[3]);
  for (unsigned j = 0; j < 3; ++j) {
    float normal = (float)((double)in->normal[0] * m[j] + (double)in->normal[1] * m[4+j] +
                           (double)in->normal[2] * m[8+j]);
    if (!isfinite(position[j]) || !isfinite(normal)) return 0;
    out->position[j] = position[j];
    out->normal[j] = normal;
  }
  return 1;
}
static int apply_bom(Reference *r, unsigned view, uint32_t submesh,
                      const int32_t disabled[5], char e[256]) {
  BkBomDualAssets *bom = bk_ending_tertiary_assets_bom(r->assets);
  int group = -1;
  for (unsigned i = 0; i < r->count; ++i) {
    const BkBomAssetMesh *target = bk_bom_dual_assets_mesh(bom, r->plans[i].target);
    if (target->submesh == submesh) { group = r->groups[i]; break; }
  }
  if (group < 0) return 1;
  for (unsigned i = 0; i < r->count; ++i) {
    if (r->groups[i] != group || disabled[i] == 1) continue;
    const BkBomAssetMesh *target = bk_bom_dual_assets_mesh(bom, r->plans[i].target);
    Mesh *mesh = r->actors[0].meshes + target->submesh;
    if (r->plans[i].count && mesh->skin)
      mesh->replay[view] = 1;
    for (size_t j = 0; j < r->plans[i].count; ++j) {
      if (!transfer(r->source[view][i] + r->mapping[i][j],
                    mesh->vertices[view] + r->plans[i].indices[j], r->source_world[view][i]))
        return fail(e, "invalid reference transfer");
      ++transfers;
      if (i < 3) ++upper_transfers; else ++lower_transfers;
    }
  }
  return 1;
}
static int compare(const BkLitVertex *got, const BkModelVertex *want, uint32_t count,
                    const BkModelMaterial *material, char e[256]) {
  BkMaterialState state;
  if (!bk_material_state(material, 0, &state, e)) return 0;
  for (uint32_t i = 0; i < count; ++i) {
    const float p[] = {got[i].base.x, got[i].base.y, got[i].base.z};
    for (unsigned j = 0; j < 6; ++j) {
      double a = j < 3 ? p[j] : got[i].normal[j-3];
      double b = j < 3 ? want[i].position[j] : want[i].normal[j-3];
      double d = fabs(a-b) / fmax(1, fabs(b));
      if (d > worst) worst = d;
      if (!isfinite(d) || d > 3e-5) {
        snprintf(e, 256, "vertex%u component%u got%.9g expected%.9g", i, j, a, b);
        return 0;
      }
    }
    float diffuse[] = {got[i].base.r, got[i].base.g, got[i].base.b, got[i].base.a};
    if (memcmp(diffuse, state.diffuse, 16) ||
        memcmp(got[i].ambient, material->ambient, sizeof(got[i].ambient)) ||
        memcmp(got[i].emissive, material->emissive, sizeof(got[i].emissive)) ||
        memcmp(got[i].specular, material->specular, sizeof(got[i].specular)) ||
        got[i].power != material->power || got[i].base.u != want[i].uv[0][0] ||
        got[i].base.v != want[i].uv[0][1]) return fail(e, "lost material/UV fields");
    ++vertices;
  }
  return 1;
}
static int advance(BkEndingTertiaryAssets *assets, unsigned step, uint32_t *random, char e[256]) {
  BkActorForest *forest = bk_ending_tertiary_assets_forest(assets);
  for (unsigned a = 0; a < 4; ++a) {
    BkActorPose *pose = bk_ending_tertiary_assets_pose(assets, roles[a]);
    if (!pose) continue;
    unsigned hidden = a == 3 ? (step % 8 >= 4) : a == 0 ? step % 12 == 9 : (step+a) % 8 >= 4;
    if (!bk_actor_forest_visibility(forest, bk_ending_tertiary_assets_root(assets, roles[a]), hidden, e)) return 0;
    if (a != 3 && step == 0 && !bk_actor_pose_request_mode(pose, a == 0 ? 4 : a == 1 ? 7 : 9,
                                                         BK_CLIP_REQUEST_CONFIGURED, e)) return 0;
    float seconds = step % 5 ? .125f : 0;
    if (a == 3) {
      if (!bk_ending_tertiary_assets_advance(assets, roles[a], seconds, e)) return 0;
    } else if (!bk_ending_tertiary_assets_advance_plain(assets, roles[a], seconds,
                  step % 3 ? BK_CLIP_PLAIN_SCHEDULED : BK_CLIP_PLAIN_SOURCE, e)) return 0;
  }
  uint32_t primary_frame, registry;
  if (!bk_actor_forest_binding(forest, bk_ending_tertiary_assets_root(assets, 0), &registry, &primary_frame)) return 0;
  BkMaterialAlphaEdit alpha = {.frame = primary_frame, .alpha = step % 12 == 4 ? 0 : 1};
  const BkEndingTertiaryConfig *config = bk_ending_tertiary_assets_config(assets);
  return bk_material_pose_alpha(bk_ending_tertiary_assets_materials(assets, 0), &alpha, 1, e) &&
         bk_face_assets_step(bk_ending_tertiary_assets_face(assets), bk_ending_tertiary_assets_face_state(assets),
             config->expression_a, (float)((step*17)%100), 1100+step*125, 1101+step*125,
             1102+step*125, 1103+step*125, random, e) &&
         bk_eye_assets_select(bk_ending_tertiary_assets_eyes(assets), step%2, e);
}
static int draw(BkRenderer *renderer, BkEndingNormalRender *render, const BkViewport *viewport, char e[256]) {
  return bk_renderer_begin(renderer, e) && bk_renderer_viewport(renderer, viewport, e) &&
         bk_ending_normal_render_draw(render, e) && bk_renderer_end(renderer, e);
}
static int profile(BkRenderer *renderer, BkResourceStore *store, BkResourceStore *missing_movie,
                     unsigned group, unsigned variant, unsigned retained, char e[256]) {
  int ok = 0;
  BkEndingTertiaryAssets *assets[2] = {0};
  BkEndingNormalRender *render = NULL;
  BkEndingBackgroundAssets *background = NULL;
  BkEndingSpecialMaterial *materials = NULL;
  BkLitVertex *readback = NULL;
  Reference ref = {0};
  /*Dialogue reset intentionally retains orbit fields: both independent
   * fixtures must start with the same defined process state. */
  BkMenuCamera camera[2] = {0}; BkEndingCameraPresets presets[2] = {0};
  uint32_t random[2] = {123, 123}, clocks[] = {100, 110, 120, 130};
  for (unsigned i = 0; i < 2; ++i) {
    CHECK(bk_menu_camera_dialogue(camera+i));
    if (retained) {
      char name[32];
      snprintf(name, sizeof(name), "m%02u_90.xan", group+1);
      background = bk_ending_background_assets_create(store, name, e);
      CHECK(background);
      assets[i] = bk_ending_tertiary_assets_create_reloaded(store, group, variant, background,
          clocks, random+i, camera+i, presets+i, e);
    } else assets[i] = bk_ending_tertiary_assets_create(store, group, variant, clocks,
                                                       random+i, camera+i, presets+i, e);
    CHECK(assets[i] && bk_ending_tertiary_assets_load_background(assets[i], store, e));
    bk_ending_background_assets_destroy(background); background = NULL;
  }
  CHECK(initialize_reference(&ref, assets[1], retained, e));
  CHECK(ref.count == (group == 2 ? 5u : 0u));
  size_t material_count;
  materials = material_registry(assets[0], retained, &material_count);
  CHECK(materials);
  uint32_t max = 0;
  for (unsigned a = 0; a < 4; ++a) if (ref.actors[a].model)
    for (uint32_t m = 0; m < ref.actors[a].model->submesh_count; ++m)
      if (ref.actors[a].model->submeshes[m].vertex_count > max) max = ref.actors[a].model->submeshes[m].vertex_count;
  readback = malloc((size_t)max * sizeof(*readback)); CHECK(readback);
  BkRenderStats before = bk_renderer_stats(renderer);
  CHECK(!bk_ending_tertiary_render_create(renderer, missing_movie, assets[0], 1000, e));
  CHECK(before.live_allocations == bk_renderer_stats(renderer).live_allocations &&
        before.live_bytes == bk_renderer_stats(renderer).live_bytes); ++rejected;
  e[0] = 0;
  render = bk_ending_tertiary_render_create(renderer, store, assets[0], 1000, e); CHECK(render);
  BkViewport viewport = {8, 6, 48, 36};
  BkFog fog = {0}; BkRenderStats stable = {0};
  for (unsigned step = 0; step < 24; ++step) {
    for (unsigned a = 0; a < 4; ++a) {
      memset(ref.actors[a].captured, 0, sizeof(ref.actors[a].captured));
      if (ref.actors[a].model)
        for (uint32_t m = 0; m < ref.actors[a].model->submesh_count; ++m) {
          memset(ref.actors[a].meshes[m].seen, 0, sizeof(ref.actors[a].meshes[m].seen));
          memset(ref.actors[a].meshes[m].replay, 0, sizeof(ref.actors[a].meshes[m].replay));
        }
    }
    ref.calls = ref.events = 0;
    CHECK(advance(assets[0], step, random, e) && advance(assets[1], step, random+1, e));
    CHECK(random[0] == random[1]);
    int32_t disabled[5];
    for (unsigned i = 0; i < 5; ++i) disabled[i] = (int32_t[]){0,1,2,-1}[(step+i)%4];
    int dual = step%3 == 1;
    BkEndingFrameState state = {.group=(uint8_t)group, .phase=step%12==11?7:3};
    dual &= state.phase != 7;
    int32_t action=(int32_t)variant, index=(int32_t)((step*7)%108), offset=(int32_t)(step%3);
    uint8_t camera_variant=5, restore_hidden=(uint8_t)(step%2);
    float cameras[BK_ENDING_SPECIAL_CAMERAS][4]; CHECK(bk_ending_special_cameras(cameras, group));
    for (unsigned i = 0; i < 2; ++i) {
      BkActorForest *forest = bk_ending_tertiary_assets_forest(assets[i]);
      uint32_t primary=bk_ending_tertiary_assets_root(assets[i],0), auxiliary=0;
      if (group==2) auxiliary=bk_ending_tertiary_assets_root(assets[i],1);
      uint32_t head=bk_actor_forest_node(forest,bk_ending_tertiary_assets_registry(assets[i],0),
          bk_ending_tertiary_assets_node(assets[i],0));
      BkDrawDispatch dispatch;
      BkDrawDispatchInput input={.flow=16,.group=(int32_t)group,.event_state=state.phase,
          .root_721b28=primary,.root_721b2c=auxiliary,
          .root_721b34=bk_ending_tertiary_assets_root(assets[i],5)};
      CHECK(bk_draw_dispatch_select(&input,&dispatch));
      BkEndingSpecialBindings bindings={&state,&action,&camera_variant,&restore_hidden,&index,&offset,
          cameras,bk_actor_forest_world(forest,head)+12,&primary,&auxiliary};
      if (i) {
        if (dual) {
          BkEndingSpecialScene scene={forest,camera+i,ref.materials,ref.material_count,&ref,capture_draw,observe_event};
          CHECK(bk_ending_special_scene_draw(&scene,&bindings,&dispatch,e));
        } else CHECK(capture_draw(&ref,&dispatch,camera+i,e));
      } else {
        if (!step) {
          BkDrawDispatch invalid=dispatch;invalid.objects[4]=1;
          CHECK(!bk_ending_normal_render_prepare_regular(render,&invalid,camera,&fog,
              bk_ending_tertiary_assets_materials(assets[0],0),disabled,ref.count,e));
          CHECK(!bk_ending_normal_render_pass_count(render));++rejected;
          e[0] = 0;
        }
        CHECK(bk_ending_normal_render_movie_step(render,1100+(int32_t)step*125,1101+(int32_t)step*125,e));
        if (dual) {
          BkEndingSpecialRenderInput special={.bindings=&bindings,.dispatch=&dispatch,.camera=camera,.fog=&fog,
              .primary_materials=bk_ending_tertiary_assets_materials(assets[0],0),
              .materials=materials,.material_count=material_count,.bom_disabled=disabled,.bom_count=ref.count,
              .main_viewport=viewport,.special_viewport={24,6,32,24}};
          CHECK(bk_ending_normal_render_prepare_special(render,&special,e));
        } else CHECK(bk_ending_normal_render_prepare_regular(render,&dispatch,camera,&fog,
            bk_ending_tertiary_assets_materials(assets[0],0),disabled,ref.count,e));
      }
    }
    CHECK(!memcmp(camera,camera+1,sizeof(*camera)));
    unsigned passes=bk_ending_normal_render_pass_count(render);
    CHECK(passes==(state.phase==7?0u:2u+(unsigned)dual));
    CHECK(ref.calls==(dual?2u:1u));
    CHECK(draw(renderer,render,&viewport,e));
    for (unsigned p=0;p<passes;++p) {
      unsigned view=bk_ending_normal_render_pass_view(render,p);
      CHECK(view==(p>=2));
      if (!p && step%8>=4) {CHECK(!bk_ending_normal_render_queue_count(render,p));++hidden_backgrounds;}
      for (uint32_t q=0;q<bk_ending_normal_render_queue_count(render,p);++q) {
        uint32_t actor,frame,mesh;
        CHECK(bk_ending_normal_render_queue_item(render,p,q,&actor,&frame,&mesh));
        int a=actor_index(&ref,actor);CHECK(a>=0 && !(view&&a==3));
        CHECK(ref.actors[a].captured[view] && mesh<ref.actors[a].model->submesh_count);
        if (a==0) CHECK(apply_bom(&ref,view,mesh,disabled,e));
        ref.actors[a].meshes[mesh].seen[view]=1;
      }
    }
    for (unsigned a=0;a<4;++a) if (ref.actors[a].model)
      for (unsigned view=0;view<2;++view)
        for (uint32_t m=0;m<ref.actors[a].model->submesh_count;++m) {
          Mesh *mesh=ref.actors[a].meshes+m;
          if (!mesh->seen[view]) continue;
          uint32_t count=ref.actors[a].model->submeshes[m].vertex_count;
          CHECK(bk_lit_mesh_readback(renderer,bk_ending_normal_render_view_mesh(render,view,
              ref.actors[a].registry,m),readback,count,e));
          if (!compare(readback,mesh->vertices[view],count,mesh->material+view,e)) {
            fprintf(stderr,"third render frame%u view%u actor%u mesh%u\n",step,view,a,m);goto done;
          }
          model_morphs+=bk_morph_group_mesh(bk_ending_tertiary_assets_morph(assets[1],roles[a]),m)!=NULL;
          face_morphs+=a==0&&bk_face_assets_mesh(bk_ending_tertiary_assets_face(assets[1]),m)!=NULL;
        }
    if (step%6==0 || !passes) {
      uint8_t first[64*48*4],again[sizeof(first)];
      CHECK(bk_renderer_readback(renderer,first,sizeof(first),e));
      BkRenderStats held=bk_renderer_stats(renderer);uint32_t movie=bk_ending_normal_render_movie_frame(render);
      CHECK(draw(renderer,render,&viewport,e));
      CHECK(bk_renderer_readback(renderer,again,sizeof(again),e));
      CHECK(!memcmp(first,again,sizeof(first))&&movie==bk_ending_normal_render_movie_frame(render));
      /* A BOM callback changes skinned outputs. The next presentation must
       * restore precisely those targets before replaying transfers; this is
       * not a new CPU animation/world update. Other meshes remain cached. */
      unsigned replay = 0;
      for (unsigned a = 0; a < 4; ++a) if (ref.actors[a].model)
        for (uint32_t m = 0; m < ref.actors[a].model->submesh_count; ++m)
          for (unsigned view = 0; view < 2; ++view)
            replay += ref.actors[a].meshes[m].replay[view];
      CHECK(bk_renderer_stats(renderer).skin_dispatches-held.skin_dispatches==replay);
      redraw_skin_replays += replay;
      ++redraws;
    }
    BkRenderStats current=bk_renderer_stats(renderer);
    if (!step) stable=current;
    else CHECK(stable.live_allocations==current.live_allocations&&stable.live_bytes==current.live_bytes);
    ++frames;dual_frames+=dual;empty_frames+=!passes;
  }
  printf("third GPU group%u variant%u retained%u:24 frames\n",group,variant,retained);fflush(stdout);
  ++profiles;ok=1;
done:
  bk_ending_normal_render_destroy(render);
  release_reference(&ref);free(materials);free(readback);
  for (unsigned i=0;i<2;++i) bk_ending_tertiary_assets_destroy(assets[i]);
  bk_ending_background_assets_destroy(background);
  return ok;
}
int main(int argc,char **argv) {
  if (argc<2 || argc>3) return 2;
  unsigned first=argc==3?(unsigned)strtoul(argv[2],NULL,10):0,last=argc==3?first+1:5;
  if (first>=5) return 2;
  unsigned group=first,variant=0,retained=0;
  char e[256]={0},path[2048];int result=1;
  BkRenderer *renderer=bk_renderer_create(64,48,stdout,e);
  BkResourceStore *store=NULL,*missing=NULL;
  CHECK(renderer);store=bk_resources_create(e);missing=bk_resources_create(e);CHECK(store&&missing);
  const char *packs[]={"bk3_11","fambom","bk3_04","bk3_03","bk3_18"};
  for (unsigned i=0;i<5;++i) {
    CHECK(snprintf(path,sizeof(path),"%s/%s.pp",argv[1],packs[i])<(int)sizeof(path));
    CHECK(bk_resources_mount(store,packs[i],path,e));
    if (i!=4) CHECK(bk_resources_mount(missing,packs[i],path,e));
  }
  BkRenderStats baseline=bk_renderer_stats(renderer);
  for (;group<last;++group) for (variant=0;variant<2;++variant) for (retained=0;retained<2;++retained) {
    CHECK(profile(renderer,store,missing,group,variant,retained,e));
    BkRenderStats current=bk_renderer_stats(renderer);
    CHECK(current.live_allocations==baseline.live_allocations&&current.live_bytes==baseline.live_bytes);
  }
  if (first<=2&&last>2) CHECK(upper_transfers&&lower_transfers);
  printf("PASS third GPU profiles=%u frames=%u dual=%u empty=%u hidden_background=%u vertices=%llu transfers=%llu upper=%llu lower=%llu model_morphs=%u face_morphs=%u redraws=%u redraw_skin_replays=%u rejected=%u relative=%.9g stable_allocations=1\n",
      profiles,frames,dual_frames,empty_frames,hidden_backgrounds,(unsigned long long)vertices,
      (unsigned long long)transfers,(unsigned long long)upper_transfers,(unsigned long long)lower_transfers,
      model_morphs,face_morphs,redraws,redraw_skin_replays,rejected,worst);
  result=0;
done:
  bk_resources_destroy(missing);bk_resources_destroy(store);bk_renderer_destroy(renderer);
  return result;
}
