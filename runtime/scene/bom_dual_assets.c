#include "scene/bom_dual_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MESH_CAPACITY = BK_BOM_SET_CAPACITY * 2 };
typedef struct {
  BkMaterialPose *materials;
  BkMaterialAnimation *animation;
  BkMorphGroup *morph;
} Effects;
struct BkBomDualAssets {
  BkBomActor actors[3];
  BkBomBindingSet config;
  BkBomDualBinding bindings[BK_BOM_SET_CAPACITY];
  BkBomAssetMesh meshes[MESH_CAPACITY];
  uint32_t mesh_count;
  BkBomDeform *deform;
  Effects effects[3]; /* primary effects remain with the enclosing loader */
  int failed;
};
static int fail(char error[256], const char *why) {
  snprintf(error, 256, "dual BOM assets: %s", why);
  return 0;
}
void bk_bom_dual_assets_destroy(BkBomDualAssets *a) {
  if (!a) return;
  bk_bom_deform_destroy(a->deform);
  for (unsigned i = 1; i < 3; ++i) {
    bk_morph_group_destroy(a->effects[i].morph);
    bk_material_animation_destroy(a->effects[i].animation);
    bk_material_pose_destroy(a->effects[i].materials);
  }
  free(a);
}
static int supported(const BkModel *m, char error[256]) {
  const char *allowed[] = {"MATE", "TEXT", "MESH", "FRAM", "ANIM", "MATA", "MORP", "FOG "};
  for (uint32_t i = 0; i < m->chunk_count; ++i) {
    unsigned j;
    for (j = 0; j < sizeof(allowed) / sizeof(*allowed); ++j)
      if (!memcmp(m->chunks[i].tag, allowed[j], 4)) break;
    if (j == sizeof(allowed) / sizeof(*allowed))
      return fail(error, "unsupported auxiliary effect chunk");
  }
  return 1;
}
static int check_actors(const BkBomActor actors[3], BkActorForest *forest,
                        char error[256]) {
  if (!actors || !actors[0].pose || !forest)
    return fail(error, "missing primary or forest");
  for (unsigned i = 0; i < 3; ++i) {
    if (!actors[i].pose) continue;
    for (unsigned j = 0; j < i; ++j)
      if (actors[i].pose == actors[j].pose)
        return fail(error, "duplicate actor identity");
    const BkModel *m = bk_actor_pose_model(actors[i].pose);
    uint32_t node = bk_actor_forest_node(forest, actors[i].forest_actor, actors[i].root);
    if (!m || !actors[i].model_name || actors[i].root >= m->frame_count ||
        m->frames[actors[i].root].parent_index != BK_MODEL_NONE ||
        node == BK_FRAME_NONE ||
        bk_frame_tree_parent(bk_actor_forest_tree(forest), node) != 0 ||
        bk_actor_forest_world(forest, node) != bk_actor_pose_frame(actors[i].pose, actors[i].root))
      return fail(error, "actors must be bound beneath global root");
  }
  return 1;
}
static int find_aux(const BkBomDualAssets *a, const char *name,
                     uint32_t *actor, uint32_t *frame, char error[256]) {
  *actor = *frame = BK_MODEL_NONE;
  for (unsigned i = 1; i < 3; ++i) {
    if (!a->actors[i].pose) continue;
    if (!bk_model_find_frame_first(bk_actor_pose_model(a->actors[i].pose),
                                    a->actors[i].root, name, frame, error))
      return 0;
    if (*frame != BK_MODEL_NONE) {
      *actor = i;
      return 1;
    }
  }
  return 1;
}
static int mesh(BkBomDualAssets *a, const char *name, uint32_t *out, char error[256]) {
  *out = BK_MODEL_NONE;
  if (!*name) return 1;
  BkBomAssetMesh found = {BK_MODEL_NONE, BK_MODEL_NONE, BK_MODEL_NONE};
  for (unsigned actor = 0; actor < 3; ++actor) {
    if (!a->actors[actor].pose) continue;
    const BkModel *m = bk_actor_pose_model(a->actors[actor].pose);
    for (uint32_t s = 0; s < m->submesh_count; ++s) {
      char candidate[256];
      if (!bk_model_submesh_name(m, s, a->actors[actor].model_name, candidate, error))
        return 0;
      if (strcmp(candidate, name)) continue;
      if (found.actor != BK_MODEL_NONE)
        return fail(error, "ambiguous registry mesh");
      found.actor = actor;
      found.submesh = s;
      for (uint32_t f = 0; f < m->frame_count; ++f)
        if (m->frames[f].mesh_index == m->submeshes[s].mesh_index) {
          if (found.frame != BK_MODEL_NONE)
            return fail(error, "ambiguous mesh parent");
          found.frame = f;
        }
      if (found.frame == BK_MODEL_NONE) return fail(error, "missing mesh parent");
    }
  }
  if (found.actor == BK_MODEL_NONE) return 1;
  for (uint32_t i = 0; i < a->mesh_count; ++i)
    if (a->meshes[i].actor == found.actor && a->meshes[i].submesh == found.submesh) {
      *out = i;
      return 1;
    }
  if (a->mesh_count == MESH_CAPACITY) return fail(error, "too many registry meshes");
  *out = a->mesh_count;
  a->meshes[a->mesh_count++] = found;
  return 1;
}
static int fixed_step(BkBomDualAssets *a, unsigned actor, char error[256]) {
  BkClipSample sample;
  int submitted;
  Effects *effects = &a->effects[actor];
  if (!bk_actor_pose_advance_frame(a->actors[actor].pose, &sample, &submitted, error))
    return 0;
  if (!submitted) return 1;
  if (effects->animation && !bk_material_animation_sample(
          effects->animation, sample.from, effects->materials, error))
    return 0;
  return !effects->morph || bk_morph_group_sample(effects->morph, sample.from, NULL, 0, error);
}
static int bind_set(BkBomDualAssets *a, BkResourceStore *store, const char *pack,
                     const BkBomBindingSet *config, BkActorForest *forest,
                     char error[256]) {
  /* The candidate borrows the existing effect instances. It only acquires
   * VIX and a replacement mapping. Rebinding must not restart MORP clocks. */
  BkBomDualAssets next = *a;
  next.config = *config;
  next.mesh_count = 0;
  next.deform = NULL;
  memset(next.bindings, 0, sizeof(next.bindings));
  uint16_t *indices[BK_BOM_SET_CAPACITY] = {0};
  BkModelVertex *snapshots[MESH_CAPACITY] = {0};
  BkBomDeformBinding plans[BK_BOM_SET_CAPACITY] = {0};
  BkBlob raw = {0};
  const BkModel *primary = bk_actor_pose_model(a->actors[0].pose);
  for (uint32_t i = 0; i < config->count; ++i) {
    const BkBomBinding *b = &config->bindings[i];
    BkBomDualBinding *v = &next.bindings[i];
    v->nodes.source = v->nodes.target = BK_MODEL_NONE;
    if (!bk_model_find_frame_first(primary, a->actors[0].root, b->parent, &v->nodes.parent, error) ||
        !bk_model_find_frame_first(primary, a->actors[0].root, b->reference, &v->nodes.reference, error) ||
        !find_aux(a, b->child, &v->child_actor, &v->nodes.child, error) ||
        !bk_model_find_frame_first(primary, a->actors[0].root, b->primary_aux, &v->nodes.primary_aux, error) ||
        !find_aux(a, b->secondary_aux, &v->secondary_aux_actor, &v->nodes.secondary_aux, error))
      goto bad;
    if (v->nodes.parent == BK_MODEL_NONE || v->nodes.child == BK_MODEL_NONE ||
        v->nodes.child == a->actors[v->child_actor].root) {
      fail(error, "missing required parent/child or unsupported root child");
      goto bad;
    }
    if (*b->selection) {
      BkResourceResult result = bk_resources_read(store, pack, b->selection, &raw, error);
      if (result == BK_RESOURCE_ERROR) goto bad;
      v->nodes.missing_selection = result == BK_RESOURCE_MISSING;
      if (raw.size / 2 > 2u * 1024 * 1024) {
        fail(error, "selection too large");
        goto bad;
      }
      v->nodes.selection_count = (uint32_t)(raw.size / 2);
      if (v->nodes.selection_count) {
        indices[i] = malloc((size_t)v->nodes.selection_count * sizeof(uint16_t));
        if (!indices[i]) {
          fail(error, "selection allocation failed");
          goto bad;
        }
        for (uint32_t j = 0; j < v->nodes.selection_count; ++j)
          indices[i][j] = (uint16_t)(raw.data[j * 2] | (uint16_t)raw.data[j * 2 + 1] << 8);
      }
      bk_blob_free(&raw);
      if (!mesh(&next, b->target_mesh, &v->nodes.target, error) ||
          !mesh(&next, b->source_mesh, &v->nodes.source, error))
        goto bad;
    }
    plans[i] = (BkBomDeformBinding){v->nodes.source, v->nodes.target, indices[i], v->nodes.selection_count};
  }
  a->failed = 1; /* Remaining errors can retain a native pose/effect prefix. */
  for (uint32_t i = 0; i < config->count; ++i) {
    const BkBomDualBinding *v = &next.bindings[i];
    if (!bk_actor_pose_align_reference(a->actors[v->child_actor].pose, v->nodes.child,
            bk_actor_pose_frame(a->actors[0].pose, v->nodes.parent), error))
      goto bad;
  }
  if (!bk_actor_forest_refresh(forest, error)) goto bad;
  BkBomMeshView views[MESH_CAPACITY] = {0};
  for (uint32_t i = 0; i < next.mesh_count; ++i) {
    const BkBomAssetMesh *m = &next.meshes[i];
    BkActorPose *pose = a->actors[m->actor].pose;
    const BkModelSubmesh *submesh = bk_actor_pose_model(pose)->submeshes + m->submesh;
    const BkMorphMesh *morph = bk_morph_group_mesh(a->effects[m->actor].morph, m->submesh);
    /* The mapping constructor shares a view type with the writable CPU draw
     * callback. Keep the effect owner's const contract: snapshot just for
     * construction, then release. No second live vertex/effect owner exists. */
    if (morph) {
      snapshots[i] = malloc((size_t)submesh->vertex_count * sizeof(BkModelVertex));
      if (!snapshots[i]) {
        fail(error, "mapping snapshot allocation failed");
        goto bad;
      }
      memcpy(snapshots[i], bk_morph_mesh_vertices(morph),
             (size_t)submesh->vertex_count * sizeof(BkModelVertex));
    }
    views[i] = (BkBomMeshView){morph ? snapshots[i] : submesh->vertices,
                               submesh->vertex_count, bk_actor_pose_frame(pose, m->frame)};
  }
  next.deform = bk_bom_deform_create(views, next.mesh_count, plans, config->count, error);
  if (!next.deform) goto bad;
  for (unsigned actor = 1; actor < 3; ++actor) {
    if (!a->actors[actor].pose) continue;
    BkActorVisibilityEdit hide = {a->actors[actor].root, 1};
    if (!bk_actor_pose_request_active(a->actors[actor].pose, 0, error) ||
        !fixed_step(a, actor, error) ||
        !bk_actor_pose_visibility(a->actors[actor].pose, &hide, 1, error))
      goto bad;
  }
  bk_bom_deform_destroy(a->deform);
  next.failed = 0;
  *a = next;
  for (unsigned i = 0; i < BK_BOM_SET_CAPACITY; ++i) free(indices[i]);
  for (unsigned i = 0; i < MESH_CAPACITY; ++i) free(snapshots[i]);
  return 1;
bad:
  bk_blob_free(&raw);
  for (unsigned i = 0; i < BK_BOM_SET_CAPACITY; ++i) free(indices[i]);
  for (unsigned i = 0; i < MESH_CAPACITY; ++i) free(snapshots[i]);
  bk_bom_deform_destroy(next.deform);
  return 0;
}
BkBomDualAssets *bk_bom_dual_assets_create(
    BkResourceStore *store, const char *pack, const BkBomConfig *config,
    const BkBomActor actors[3], BkActorForest *forest, char error[256]) {
  BkBomBindingSet set = {0};
  if (!store || !pack || !*pack) {
    fail(error, "missing resource store/selection pack");
    return NULL;
  }
  if (!bk_bom_binding_set_append(&set, config, error) || !check_actors(actors, forest, error))
    return NULL;
  BkBomDualAssets *a = calloc(1, sizeof(*a));
  if (!a) {
    fail(error, "allocation failed");
    return NULL;
  }
  memcpy(a->actors, actors, sizeof(a->actors));
  for (unsigned actor = 1; actor < 3; ++actor) {
    if (!actors[actor].pose) continue;
    const BkModel *model = bk_actor_pose_model(actors[actor].pose);
    Effects *effects = &a->effects[actor];
    if (!supported(model, error) ||
        !(effects->materials = bk_material_pose_create(model, error)) ||
        (bk_model_chunk(model, "MATA") &&
         !(effects->animation = bk_material_animation_create(model, error))) ||
        (bk_model_chunk(model, "MORP") &&
         !(effects->morph = bk_morph_group_create(model, error))))
      goto bad;
  }
  if (!bind_set(a, store, pack, &set, forest, error)) goto bad;
  /* Names are needed again for combined registry reconstruction on append;
   * callers retain their clip/model owner until this borrower is destroyed. */
  return a;
bad:
  bk_bom_dual_assets_destroy(a);
  return NULL;
}
int bk_bom_dual_assets_append(BkBomDualAssets *a, BkResourceStore *store,
                              const char *pack, const BkBomConfig *file,
                              BkActorForest *forest, char error[256]) {
  if (!a || a->failed || !store || !pack || !*pack)
    return fail(error, "invalid or failed owner/resources");
  BkBomBindingSet next = a->config;
  return bk_bom_binding_set_append(&next, file, error) &&
         check_actors(a->actors, forest, error) &&
         bind_set(a, store, pack, &next, forest, error);
}
static int valid_actor(const BkBomDualAssets *a, unsigned actor) {
  return a && !a->failed && actor > 0 && actor < 3 && a->actors[actor].pose;
}
int bk_bom_dual_assets_advance_frame(BkBomDualAssets *a, unsigned actor, char error[256]) {
  if (!valid_actor(a, actor)) return fail(error, "missing auxiliary or failed owner");
  if (fixed_step(a, actor, error)) return 1;
  a->failed = 1;
  return 0;
}
static int advance_effects(BkBomDualAssets *a, unsigned actor, float seconds,
                            int plain_mode, char error[256]) {
  if (!valid_actor(a, actor)) return fail(error, "missing auxiliary or failed owner");
  Effects *p = &a->effects[actor];
  BkPlaybackEffects effects;
  int submitted;
  int ok = plain_mode < 0
      ? bk_actor_pose_advance_effects(a->actors[actor].pose, seconds, &effects, &submitted, error)
      : bk_actor_pose_advance_plain(a->actors[actor].pose, seconds,
                                     (BkClipPlainMode)plain_mode, &effects, &submitted, error);
  if (!ok)
    goto bad;
  if (!submitted) return 1;
  if (effects.pose.blend && p->morph && !bk_morph_group_blend(p->morph,
          effects.pose.from, effects.pose.to, effects.pose.weight, NULL, 0, error))
    goto bad;
  if (p->animation && !bk_material_animation_sample(p->animation, effects.source, p->materials, error))
    goto bad;
  if (!effects.pose.blend && p->morph && !bk_morph_group_sample(p->morph, effects.source, NULL, 0, error))
    goto bad;
  return 1;
bad:
  a->failed = 1;
  return 0;
}
int bk_bom_dual_assets_advance(BkBomDualAssets *a, unsigned actor, float seconds,
                               char error[256]) {
  return advance_effects(a, actor, seconds, -1, error);
}
int bk_bom_dual_assets_advance_plain(BkBomDualAssets *a, unsigned actor,
                                     float seconds, BkClipPlainMode mode,
                                     char error[256]) {
  if (mode < BK_CLIP_PLAIN_SCHEDULED || mode > BK_CLIP_PLAIN_FORCE_CHAIN)
    return fail(error, "invalid plain scheduler mode");
  return advance_effects(a, actor, seconds, (int)mode, error);
}
uint32_t bk_bom_dual_assets_count(const BkBomDualAssets *a) {
  return a && !a->failed ? a->config.count : 0;
}
uint32_t bk_bom_dual_assets_mode(const BkBomDualAssets *a) {
  return a && !a->failed ? a->config.mode : 0;
}
const BkBomDualBinding *bk_bom_dual_assets_binding(const BkBomDualAssets *a, uint32_t i) {
  return a && !a->failed && i < a->config.count ? &a->bindings[i] : NULL;
}
uint32_t bk_bom_dual_assets_mesh_count(const BkBomDualAssets *a) {
  return a && !a->failed ? a->mesh_count : 0;
}
const BkBomAssetMesh *bk_bom_dual_assets_mesh(const BkBomDualAssets *a, uint32_t i) {
  return a && !a->failed && i < a->mesh_count ? &a->meshes[i] : NULL;
}
int bk_bom_dual_assets_mapping(const BkBomDualAssets *a, uint32_t i,
                               int32_t *group, const uint32_t **sources, size_t *count) {
  return a && !a->failed && bk_bom_deform_mapping(a->deform, i, group, sources, count);
}
int bk_bom_dual_assets_plan(const BkBomDualAssets *a, uint32_t i, BkBomDeformBinding *out) {
  return a && !a->failed && bk_bom_deform_plan(a->deform, i, out);
}
BkActorPose *bk_bom_dual_assets_actor(const BkBomDualAssets *a, unsigned actor) {
  return a && !a->failed && actor < 3 ? a->actors[actor].pose : NULL;
}
BkMaterialPose *bk_bom_dual_assets_materials(const BkBomDualAssets *a, unsigned actor) {
  return valid_actor(a, actor) ? a->effects[actor].materials : NULL;
}
const BkMaterialAnimation *bk_bom_dual_assets_material_animation(const BkBomDualAssets *a, unsigned actor) {
  return valid_actor(a, actor) ? a->effects[actor].animation : NULL;
}
const BkMorphGroup *bk_bom_dual_assets_morph(const BkBomDualAssets *a, unsigned actor) {
  return valid_actor(a, actor) ? a->effects[actor].morph : NULL;
}
