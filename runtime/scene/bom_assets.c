#include "scene/bom_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BkBomAssets {
  BkBomActor actors[2];
  BkBomAssetBinding bindings[4];
  BkBomAssetMesh meshes[8];
  uint32_t count, mesh_count;
  BkBomDeform *deform;
  BkMaterialPose *materials;
  BkMaterialAnimation *animation;
  BkMorphGroup *morph;
};
static int fail(char e[256], const char *why) {
  snprintf(e, 256, "BOM assets: %s", why);
  return 0;
}
void bk_bom_assets_destroy(BkBomAssets *a) {
  if (!a)
    return;
  bk_bom_deform_destroy(a->deform);
  bk_morph_group_destroy(a->morph);
  bk_material_animation_destroy(a->animation);
  bk_material_pose_destroy(a->materials);
  free(a);
}
static int mesh(BkBomAssets *a, const char *name, uint32_t *out, char e[256]) {
  *out = BK_MODEL_NONE;
  if (!*name)
    return 1;
  BkBomAssetMesh found = {BK_MODEL_NONE, BK_MODEL_NONE, BK_MODEL_NONE};
  for (unsigned actor = 0; actor < 2; actor++) {
    const BkModel *m = bk_actor_pose_model(a->actors[actor].pose);
    for (uint32_t s = 0; s < m->submesh_count; s++) {
      char candidate[256];
      if (!bk_model_submesh_name(m, s, a->actors[actor].model_name, candidate,
                                 e))
        return 0;
      if (strcmp(candidate, name))
        continue;
      if (found.actor != BK_MODEL_NONE)
        return fail(e, "ambiguous registry mesh");
      found.actor = actor;
      found.submesh = s;
      for (uint32_t f = 0; f < m->frame_count; f++)
        if (m->frames[f].mesh_index == m->submeshes[s].mesh_index) {
          if (found.frame != BK_MODEL_NONE)
            return fail(e, "ambiguous mesh parent");
          found.frame = f;
        }
      if (found.frame == BK_MODEL_NONE)
        return fail(e, "missing mesh parent");
    }
  }
  if (found.actor == BK_MODEL_NONE)
    return 1;
  for (uint32_t i = 0; i < a->mesh_count; i++)
    if (a->meshes[i].actor == found.actor &&
        a->meshes[i].submesh == found.submesh) {
      *out = i;
      return 1;
    }
  if (a->mesh_count == 8)
    return fail(e, "too many registry meshes");
  *out = a->mesh_count;
  a->meshes[a->mesh_count++] = found;
  return 1;
}
static int supported(const BkModel *m, char e[256]) {
  const char *allowed[] = {"MATE", "TEXT", "MESH", "FRAM",
                           "ANIM", "MATA", "MORP", "FOG "};
  for (uint32_t i = 0; i < m->chunk_count; i++) {
    unsigned j;
    for (j = 0; j < sizeof(allowed) / sizeof(*allowed); j++)
      if (!memcmp(m->chunks[i].tag, allowed[j], 4))
        break;
    if (j == sizeof(allowed) / sizeof(*allowed))
      return fail(e, "unsupported secondary effect chunk");
  }
  return 1;
}
int bk_bom_assets_advance_frame(BkBomAssets *a, char e[256]) {
  if (!a)
    return fail(e, "missing instance");
  BkClipSample sample;
  int submitted;
  if (!bk_actor_pose_advance_frame(a->actors[1].pose, &sample, &submitted, e))
    return 0;
  if (!submitted)
    return 1;
  if (a->animation &&
      !bk_material_animation_sample(a->animation, sample.from, a->materials, e))
    return 0;
  return !a->morph || bk_morph_group_sample(a->morph, sample.from, NULL, 0, e);
}
int bk_bom_assets_advance(BkBomAssets *a, float seconds, char e[256]) {
  if (!a)
    return fail(e, "missing instance");
  BkPlaybackEffects effects;
  int submitted;
  if (!bk_actor_pose_advance_effects(a->actors[1].pose, seconds, &effects,
                                     &submitted, e))
    return 0;
  if (!submitted)
    return 1;
  if (effects.pose.blend && a->morph &&
      !bk_morph_group_blend(a->morph, effects.pose.from, effects.pose.to,
                            effects.pose.weight, NULL, 0, e))
    return 0;
  if (a->animation && !bk_material_animation_sample(
                          a->animation, effects.source, a->materials, e))
    return 0;
  return effects.pose.blend || !a->morph ||
         bk_morph_group_sample(a->morph, effects.source, NULL, 0, e);
}
BkBomAssets *bk_bom_assets_create(BkResourceStore *store, const char *pack,
                                  const BkBomConfig *config,
                                  const BkBomActor actors[2],
                                  BkActorForest *forest, char e[256]) {
  if (!store || !pack || !*pack || !config || config->count > 4 || !actors ||
      !forest || !actors[0].pose || !actors[1].pose ||
      actors[0].pose == actors[1].pose) {
    fail(e, "invalid resources/actors/configuration");
    return NULL;
  }
  for (unsigned i = 0; i < 2; i++) {
    const BkModel *m = bk_actor_pose_model(actors[i].pose);
    uint32_t root =
        bk_actor_forest_node(forest, actors[i].forest_actor, actors[i].root);
    if (!actors[i].model_name || actors[i].root >= m->frame_count ||
        m->frames[actors[i].root].parent_index != BK_MODEL_NONE ||
        root == BK_FRAME_NONE ||
        bk_frame_tree_parent(bk_actor_forest_tree(forest), root) != 0 ||
        bk_actor_forest_world(forest, root) !=
            bk_actor_pose_frame(actors[i].pose, actors[i].root)) {
      fail(e, "actors must be bound beneath global root");
      return NULL;
    }
  }
  BkBomAssets *a = calloc(1, sizeof(*a));
  BkBlob raw = {0};
  uint16_t *indices[4] = {0};
  if (!a) {
    fail(e, "allocation failed");
    return NULL;
  }
  memcpy(a->actors, actors, sizeof(a->actors));
  a->count = config->count;
  const BkModel *primary = bk_actor_pose_model(actors[0].pose);
  const BkModel *secondary = bk_actor_pose_model(actors[1].pose);
  if (!supported(secondary, e))
    goto bad;
  a->materials = bk_material_pose_create(secondary, e);
  if (!a->materials)
    goto bad;
  if (bk_model_chunk(secondary, "MATA") &&
      !(a->animation = bk_material_animation_create(secondary, e)))
    goto bad;
  if (bk_model_chunk(secondary, "MORP") &&
      !(a->morph = bk_morph_group_create(secondary, e)))
    goto bad;
  BkBomDeformBinding bindings[4] = {0};
  for (uint32_t i = 0; i < a->count; i++) {
    const BkBomBinding *b = config->bindings + i;
    const char *fields[] = {b->parent,      b->reference,     b->child,
                            b->primary_aux, b->secondary_aux, b->source_mesh,
                            b->target_mesh, b->selection};
    for (unsigned j = 0; j < 8; j++)
      if (!memchr(fields[j], 0, 260)) {
        fail(e, "unterminated binding field");
        goto bad;
      }
    BkBomAssetBinding *v = a->bindings + i;
    v->source = v->target = BK_MODEL_NONE;
    if (!bk_model_find_frame_first(primary, actors[0].root, b->parent,
                                   &v->parent, e) ||
        !bk_model_find_frame_first(primary, actors[0].root, b->reference,
                                   &v->reference, e) ||
        !bk_model_find_frame_first(secondary, actors[1].root, b->child,
                                   &v->child, e) ||
        !bk_model_find_frame_first(primary, actors[0].root, b->primary_aux,
                                   &v->primary_aux, e) ||
        !bk_model_find_frame_first(secondary, actors[1].root, b->secondary_aux,
                                   &v->secondary_aux, e))
      goto bad;
    if (v->parent == BK_MODEL_NONE || v->child == BK_MODEL_NONE ||
        v->child == actors[1].root) {
      fail(e, "missing required parent/child or unsupported root child");
      goto bad;
    }
    if (*b->selection) {
      BkResourceResult result =
          bk_resources_read(store, pack, b->selection, &raw, e);
      if (result == BK_RESOURCE_ERROR)
        goto bad;
      v->missing_selection = result == BK_RESOURCE_MISSING;
      if (raw.size / 2 > 2u * 1024 * 1024) {
        fail(e, "selection too large");
        goto bad;
      }
      v->selection_count = (uint32_t)(raw.size / 2);
      if (v->selection_count) {
        indices[i] = malloc((size_t)v->selection_count * sizeof(uint16_t));
        if (!indices[i]) {
          fail(e, "selection allocation failed");
          goto bad;
        }
        for (uint32_t j = 0; j < v->selection_count; j++)
          indices[i][j] =
              (uint16_t)(raw.data[j * 2] | (uint16_t)raw.data[j * 2 + 1] << 8);
      }
      bk_blob_free(&raw);
      if (!mesh(a, b->target_mesh, &v->target, e) ||
          !mesh(a, b->source_mesh, &v->source, e))
        goto bad;
    }
    bindings[i] = (BkBomDeformBinding){v->source, v->target, indices[i],
                                       v->selection_count};
  }
  /* All resources are now owned. Match the mutation ordering of4a4dc3. */
  for (uint32_t i = 0; i < a->count; i++)
    if (!bk_actor_pose_align_reference(
            actors[1].pose, a->bindings[i].child,
            bk_actor_pose_frame(actors[0].pose, a->bindings[i].parent), e))
      goto bad;
  if (!bk_actor_forest_refresh(forest, e))
    goto bad;
  BkBomMeshView views[8] = {0};
  for (uint32_t i = 0; i < a->mesh_count; i++) {
    BkBomAssetMesh *r = a->meshes + i;
    BkActorPose *pose = actors[r->actor].pose;
    const BkModelSubmesh *sm =
        bk_actor_pose_model(pose)->submeshes + r->submesh;
    views[i] = (BkBomMeshView){sm->vertices, sm->vertex_count,
                               bk_actor_pose_frame(pose, r->frame)};
  }
  a->deform = bk_bom_deform_create(views, a->mesh_count, bindings, a->count, e);
  if (!a->deform || !bk_actor_pose_request_active(actors[1].pose, 0, e) ||
      !bk_bom_assets_advance_frame(a, e))
    goto bad;
  BkActorVisibilityEdit hide = {actors[1].root, 1};
  if (!bk_actor_pose_visibility(actors[1].pose, &hide, 1, e))
    goto bad;
  for (unsigned i = 0; i < 4; i++)
    free(indices[i]);
  /* Filenames are construction-only, not a borrowed lifetime requirement. */
  a->actors[0].model_name = a->actors[1].model_name = NULL;
  return a;
bad:
  bk_blob_free(&raw);
  for (unsigned i = 0; i < 4; i++)
    free(indices[i]);
  bk_bom_assets_destroy(a);
  return NULL;
}
uint32_t bk_bom_assets_count(const BkBomAssets *a) { return a ? a->count : 0; }
BkActorPose *bk_bom_assets_actor(const BkBomAssets *a, unsigned actor) {
  return a && actor < 2 ? a->actors[actor].pose : NULL;
}
const BkBomAssetBinding *bk_bom_assets_binding(const BkBomAssets *a,
                                               uint32_t i) {
  return a && i < a->count ? a->bindings + i : NULL;
}
uint32_t bk_bom_assets_mesh_count(const BkBomAssets *a) {
  return a ? a->mesh_count : 0;
}
const BkBomAssetMesh *bk_bom_assets_mesh(const BkBomAssets *a, uint32_t i) {
  return a && i < a->mesh_count ? a->meshes + i : NULL;
}
const BkMaterialPose *bk_bom_assets_materials(const BkBomAssets *a) {
  return a ? a->materials : NULL;
}
const BkMorphGroup *bk_bom_assets_morph(const BkBomAssets *a) {
  return a ? a->morph : NULL;
}
int bk_bom_assets_mapping(const BkBomAssets *a, uint32_t i, int32_t *group,
                          const uint32_t **sources, size_t *count) {
  return a && bk_bom_deform_mapping(a->deform, i, group, sources, count);
}
int bk_bom_assets_plan(const BkBomAssets *a, uint32_t i,
                       BkBomDeformBinding *out) {
  return a && bk_bom_deform_plan(a->deform, i, out);
}
