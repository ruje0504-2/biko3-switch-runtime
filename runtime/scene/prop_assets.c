#include "scene/prop_assets.h"
#include "core/camera.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkModel *model;
  BkClipSet *clips;
  BkRoute *route;
  BkActorPose *pose;
  BkMaterialPose *materials;
  BkPropState state;
  BkPropInteractionState interaction;
  BkNpcSceneState ground;
  uint32_t root;
} Instance;
struct BkPropAssets {
  uint32_t group, area, count;
  const BkPropConfig *config;
  Instance instances[16];
};
static int fail(char error[256], const char *message) {
  snprintf(error, 256, "prop assets: %s", message);
  return 0;
}
void bk_prop_assets_destroy(BkPropAssets *a) {
  if (!a)
    return;
  for (uint32_t i = 0; i < a->count; i++) {
    Instance *p = &a->instances[i];
    bk_material_pose_destroy(p->materials);
    bk_actor_pose_destroy(p->pose);
    bk_route_destroy(p->route);
    bk_clip_set_destroy(p->clips);
    bk_model_destroy(p->model);
  }
  free(a);
}
static void word(unsigned char *p, uint32_t v) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (unsigned char)(v >> (i * 8));
}
static void number(unsigned char *p, float f) {
  uint32_t bits;
  memcpy(&bits, &f, 4);
  word(p, bits);
}
BkPropAssets *bk_prop_assets_create(BkResourceStore *resources, uint32_t group,
                                    uint32_t area, const BkModel *background,
                                    const float *world, size_t world_floats,
                                    char error[256]) {
  const BkPropConfig *config;
  uint32_t count;
  if (!resources || !bk_prop_config(&config, &count, group, area)) {
    fail(error, "invalid resources/profile");
    return NULL;
  }
  if (group == 0 && area == 5 &&
      (!background || !world ||
       world_floats != (size_t)background->frame_count * 16)) {
    fail(error, "background cache required for anchored props");
    return NULL;
  }
  BkPropAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  a->group = group;
  a->area = area;
  a->count = count;
  a->config = config;
  BkBlob blob = {0};
  for (uint32_t i = 0; i < count; i++) {
    const BkPropConfig *c = &config[i];
    Instance *p = &a->instances[i];
    if (bk_resources_read(resources, "bk3_07", c->clip, &blob, error) !=
        BK_RESOURCE_OK)
      goto bad;
    p->clips = bk_clip_set_decode(blob.data, blob.size, error);
    bk_blob_free(&blob);
    if (!p->clips)
      goto bad;
    if (bk_resources_read(resources, "bk3_07", bk_clip_model_name(p->clips),
                          &blob, error) != BK_RESOURCE_OK)
      goto bad;
    int ok =
        bk_model_decode(blob.data, blob.size, &p->model, error) == BK_MODEL_OK;
    bk_blob_free(&blob);
    if (!ok)
      goto bad;
    unsigned char raw[1280] = {0};
    if (c->route_file) {
      if (bk_resources_read(resources, "routes", c->route_file, &blob, error) !=
          BK_RESOURCE_OK)
        goto bad;
      memcpy(raw, blob.data, blob.size < sizeof(raw) ? blob.size : sizeof(raw));
      bk_blob_free(&blob);
    } else
      for (uint32_t j = 0; j < c->point_count; j++) {
        for (unsigned k = 0; k < 3; k++)
          number(raw + j * 20 + k * 4, c->points[j].position[k]);
        number(raw + j * 20 + 12, c->points[j].parameter);
        raw[j * 20 + 16] = c->points[j].flags;
      }
    if (c->route_file || c->point_count) {
      p->route = bk_route_decode(raw, sizeof(raw), error);
      if (!p->route)
        goto bad;
      if (bk_route_count(p->route) >= 64 || c->cursor < 0 ||
          c->cursor >= (int32_t)bk_route_count(p->route)) {
        fail(error, "invalid prop route bounds");
        goto bad;
      }
    }
    static const BkRoutePoint zero = {0};
    const BkRoutePoint *point =
        p->route ? bk_route_point(p->route, c->cursor) : &zero;
    p->state.kind = c->kind;
    p->state.actions[3] = 1;
    p->state.path.cursor = c->cursor;
    p->state.path.last = (int32_t)bk_route_count(p->route) - 1;
    memcpy(p->state.path.position, point->position, 12);
    p->state.path.yaw = point->parameter;
    if (bk_prop_smooth_heading(c->kind)) {
      const BkRoutePoint *next = bk_route_point(p->route, c->cursor + 1);
      if (!next || !bk_route_heading(&p->state.path.yaw, point->position[0],
                                     point->position[2], next->position[0],
                                     next->position[2])) {
        fail(error, "invalid initial route heading");
        goto bad;
      }
    }
    p->root = BK_MODEL_NONE;
    for (uint32_t f = 0; f < p->model->frame_count; f++)
      if (p->model->frames[f].parent_index == BK_MODEL_NONE) {
        if (p->root != BK_MODEL_NONE) {
          fail(error, "multiple model roots");
          goto bad;
        }
        p->root = f;
      }
    p->pose = bk_actor_pose_create_loaded(p->model, p->clips, p->root,
                                          p->state.path.position,
                                          p->state.path.yaw, error);
    if (!p->pose)
      goto bad;
    p->materials = bk_material_pose_create(p->model, error);
    if (!p->materials)
      goto bad;
    if (c->anchor) {
      for (uint32_t f = 0; f < background->frame_count; f++)
        if (!strcmp(background->frames[f].name, c->anchor)) {
          BkActorPlacement placement = *bk_actor_pose_placement(p->pose);
          memcpy(placement.world, world + f * 16, 64);
          memcpy(placement.position, placement.world + 12, 12);
          if (!bk_actor_pose_place_exact(p->pose, &placement, error))
            goto bad;
          memcpy(p->state.path.position, placement.position, 12);
          break;
        }
    }
  }
  return a;
bad:
  bk_blob_free(&blob);
  bk_prop_assets_destroy(a);
  return NULL;
}
uint32_t bk_prop_assets_count(const BkPropAssets *a) {
  return a ? a->count : 0;
}
const BkPropState *bk_prop_assets_state(const BkPropAssets *a, uint32_t i) {
  return a && i < a->count ? &a->instances[i].state : NULL;
}
const BkActorPose *bk_prop_assets_pose(const BkPropAssets *a, uint32_t i) {
  return a && i < a->count ? a->instances[i].pose : NULL;
}
BkActorPose *bk_prop_assets_bind_pose(BkPropAssets *a, uint32_t i) {
  return a && i < a->count ? a->instances[i].pose : NULL;
}
int bk_prop_assets_bind_interaction(BkPropAssets *a, uint32_t i,
                                    BkPropInteractionActor *out) {
  if (!a || i >= a->count || !out)
    return 0;
  Instance *p = &a->instances[i];
  *out =
      (BkPropInteractionActor){.motion = &p->state,
                               .interaction = &p->interaction,
                               .world = bk_actor_pose_frame(p->pose, p->root)};
  return out->world != NULL;
}
const BkMaterialPose *bk_prop_assets_materials(const BkPropAssets *a,
                                               uint32_t i) {
  return a && i < a->count ? a->instances[i].materials : NULL;
}
const BkRoute *bk_prop_assets_route(const BkPropAssets *a, uint32_t i) {
  return a && i < a->count ? a->instances[i].route : NULL;
}
int bk_prop_assets_collision(const BkPropAssets *a, BkCollision *collision,
                             char error[256]) {
  if (!a || !collision)
    return fail(error, "missing collision instances");
  BkCollisionProp props[16] = {0};
  for (uint32_t i = 0; i < a->count; i++) {
    const Instance *p = &a->instances[i];
    props[i] =
        (BkCollisionProp){.active = 1,
                          .kind = p->state.kind,
                          .model = p->model,
                          .world = bk_actor_pose_frame(p->pose, 0),
                          .world_floats = (size_t)p->model->frame_count * 16,
                          .model_name = bk_clip_model_name(p->clips)};
    memcpy(props[i].position, p->state.path.position, 12);
  }
  return bk_collision_begin_props(collision, props, a->count, error);
}
int bk_prop_material_apply(BkMaterialPose *pose, uint32_t root,
                           const BkPropMotionEffects *out, char error[256]) {
  if (!pose || !out || out->material < -2 || out->material > 1)
    return fail(error, "invalid material command");
  if (out->material == -2)
    return 1;
  static const BkMaterialAlphaRule rules[2][4] = {
      {{"IDO_kage", -1}, {"IDO_kage_2", 0}, {"Atari_Hantei", 0}, {"NULL", 0}},
      {{"Atari_Hantei", 0}, {"NULL", 0}, {"NULL", 0}, {"NULL", 0}}};
  BkMaterialAlphaEdit edit = {.frame = root, .alpha = out->alpha};
  if (out->material >= 0) {
    edit.rules = rules[out->material];
    edit.rule_count = 4;
  }
  return bk_material_pose_alpha(pose, &edit, 1, error);
}
int bk_prop_assets_step_spatial_consume(
    BkPropAssets *a, BkPropShared *shared, const BkPropMotionInput *in,
    const BkCollision *collision, const BkNpcSceneInput *ground,
    size_t ground_count, BkPropMotionEffects effects[16],
    BkPropPlacedConsumer consume, void *context, char error[256]) {
  if (!a || !shared || !in || !effects || !isfinite(in->seconds) ||
      in->seconds < 0 || (double)in->seconds * 60 >= INT32_MAX)
    return fail(error, "invalid spatial input");
  int need_ground = bk_prop_needs_ground(a->group, a->area);
  if (need_ground && a->count &&
      (!collision || !ground || ground_count < a->count))
    return fail(error, "missing prop ground inputs");
  shared->distance = 0;
  for (uint32_t i = 0; i < a->count; i++) {
    Instance *p = &a->instances[i];
    BkPropState s = p->state;
    BkPropShared sh = *shared;
    BkPropMotionEffects out;
    s.path.velocity[0] = s.path.velocity[2] = 0;
    if (!bk_prop_motion_select(&s, &sh, in, &out))
      return fail(error, "invalid motion policy");
    if (!bk_prop_material_apply(p->materials, p->root, &out, error))
      return 0;
    /* The car reaction requests a zero-distance route step for EVERY prop,
     * including stationary objects with zero/one points. Native515145 then
     * reads a negative predecessor outside that object's route. Preserve
     * the stationary placement instead; still run ground/facing/publish and
     * audio below. Positive movement and normal multi-point paths stay strict. */
    int stationary = sh.distance == 0 && s.path.last <= 0;
    if (out.allowed && !stationary) {
      BkPropRouteEffects route;
      if (!bk_prop_route_step(&s.path, p->route, sh.distance, collision, s.kind,
                              &route, error)) {
        char cause[256];
        snprintf(cause, sizeof(cause), "%s", error);
        snprintf(error, 256, "prop%u kind%d cursor%d last%d d%.4g: %.150s", i,
                 s.kind, s.path.cursor, s.path.last, (double)sh.distance, cause);
        return 0;
      }
      if (route.crossed) {
        sh.last_crossed = route.last_crossed;
        const BkRoutePoint *point =
            bk_route_point(p->route, route.last_crossed);
        if (!bk_prop_point_apply(&s, (int8_t)point->flags, in))
          return fail(error, "invalid point action");
      }
    }
    BkNpcSceneState scene = p->ground;
    if (need_ground) {
      memcpy(scene.position, s.path.position, 12);
      scene.hidden = s.hidden;
      if (!bk_npc_scene_step(&scene, collision, &ground[i], in->seconds, error))
        return 0;
      memcpy(s.path.position, scene.position, 12);
    }
    static const BkRoutePoint zero = {0};
    const BkRoutePoint *point =
        p->route ? bk_route_point(p->route, s.path.cursor) : &zero;
    if (!point)
      return fail(error, "invalid target point");
    if (s.path.last == 0)
      s.target_yaw = point->parameter;
    else if (!bk_route_heading(&s.target_yaw, s.path.position[0],
                               s.path.position[2], point->position[0],
                               point->position[2]))
      return fail(error, "invalid target heading");
    if (bk_prop_smooth_heading(s.kind) &&
        !bk_angle_blend_degrees(&s.path.yaw, s.target_yaw, s.path.yaw,
                                (float)(2.0 * in->seconds)))
      return fail(error, "invalid heading interpolation");
    if (!bk_actor_pose_place(p->pose, s.path.position, s.path.yaw, error))
      return 0;
    p->state = s;
    p->ground = scene;
    *shared = sh;
    effects[i] = out;
    if (consume && !consume(context, i, &p->state, error))
      return 0;
  }
  return 1;
}
int bk_prop_assets_step_spatial(
    BkPropAssets *a, BkPropShared *shared, const BkPropMotionInput *in,
    const BkCollision *collision, const BkNpcSceneInput *ground,
    size_t ground_count, BkPropMotionEffects effects[16], char error[256]) {
  return bk_prop_assets_step_spatial_consume(a, shared, in, collision, ground,
                                             ground_count, effects, NULL, NULL,
                                             error);
}
int bk_prop_assets_step_presentation(BkPropAssets *a, float seconds,
                                     char error[256]) {
  if (!a || !isfinite(seconds) || seconds < 0 ||
      (double)seconds * 60 >= INT32_MAX)
    return fail(error, "invalid presentation input");
  for (uint32_t i = 0; i < a->count; i++) {
    Instance *p = &a->instances[i];
    BkActorVisibilityEdit edit = {p->root, p->state.hidden};
    if (!bk_actor_pose_visibility(p->pose, &edit, 1, error) ||
        !bk_actor_pose_advance(p->pose, p->state.action,
                               (float)((double)seconds * .5), error))
      return 0;
  }
  return 1;
}
void bk_prop_assets_publish(BkPropAssets *a) {
  if (a)
    for (uint32_t i = 0; i < a->count; i++)
      bk_actor_pose_publish(a->instances[i].pose);
}
