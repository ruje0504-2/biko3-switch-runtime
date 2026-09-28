#include "model/morph_group.h"
#include "resource/store.h"
#include "world/actor_pose.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      goto done;                                                               \
  } while (0)
static uint64_t digest(uint64_t h, const void *data, size_t n) {
  const unsigned char *p = data;
  for (size_t i = 0; i < n; i++)
    h = (h ^ p[i]) * UINT64_C(1099511628211);
  return h;
}
static uint64_t pose_hash(BkActorPose *a, uint32_t count, int local) {
  uint64_t h = UINT64_C(14695981039346656037);
  for (uint32_t i = 0; i < count; i++) {
    if (local)
      h = digest(h, bk_actor_pose_local(a, i), 64);
    h = digest(h, bk_actor_pose_frame(a, i), 64);
    h = digest(h, bk_actor_pose_parent_world(a, i), 64);
  }
  return h;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  char e[256] = {0}, path[1024], name[32];
  BkResourceStore *store = bk_resources_create(e);
  BkBlob raw = {0};
  BkModel *model = NULL;
  BkClipSet *clips = NULL;
  BkActorPose *actor = NULL;
  BkModelPlayback *playback = NULL;
  BkMorphGroup *morph = NULL;
  unsigned profiles = 0, frames = 0, hidden = 0;
  uint64_t vertices = 0, hash = UINT64_C(14695981039346656037);
  int rc = 1;
  CHECK(store);
  for (unsigned i = 0; i < 2; i++) {
    snprintf(name, sizeof(name), "bk3_%02u", i ? 11 : 8);
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], name);
    CHECK(bk_resources_mount(store, name, path, e));
  }
  for (profiles = 0; profiles < 7; profiles++) {
    const char *pack = profiles < 5 ? "bk3_08" : "bk3_11";
    snprintf(name, sizeof(name), "h%02u_%02u.xan",
             profiles < 5 ? profiles + 1 : 3, profiles < 5 ? 1 : profiles + 25);
    CHECK(bk_resources_read(store, pack, name, &raw, e) == BK_RESOURCE_OK);
    clips = bk_clip_set_decode(raw.data, raw.size, e);
    bk_blob_free(&raw);
    CHECK(clips);
    CHECK(bk_resources_read(store, pack, bk_clip_model_name(clips), &raw, e) ==
          BK_RESOURCE_OK);
    CHECK(bk_model_decode(raw.data, raw.size, &model, e) == BK_MODEL_OK);
    bk_blob_free(&raw);
    uint32_t root = BK_MODEL_NONE, slots[128], count = 0;
    for (uint32_t i = 0; i < model->frame_count; i++)
      if (model->frames[i].parent_index == BK_MODEL_NONE) {
        assert(root == BK_MODEL_NONE);
        root = i;
      }
    for (uint32_t i = 0; i < 128; i++)
      if (bk_clip_definition(clips, i)->active)
        slots[count++] = i;
    CHECK(count && root != BK_MODEL_NONE);
    actor =
        bk_actor_pose_create_loaded(model, clips, root, (float[3]){0}, 0, e);
    morph = bk_morph_group_create(model, e);
    playback = bk_model_playback_create_loaded(model, clips, e);
    CHECK(actor && morph && playback);
    CHECK(bk_model_playback_request_active(playback, 0, e));
    BkClipState before, after;
    CHECK(bk_model_playback_state(playback, &before));
    BkModelRootTransform bad = {.frame = root};
    bad.world[0] = NAN;
    BkClipSample sample = {13, 71, 82, 93}, saved = sample;
    CHECK(!bk_model_playback_advance_frame(playback, &bad, &sample, e));
    CHECK(bk_model_playback_state(playback, &after));
    assert(!memcmp(&before, &after, sizeof(before)) &&
           !memcmp(&sample, &saved, sizeof(sample)));
    bk_model_playback_destroy(playback);
    playback = NULL;
    bk_actor_pose_publish(actor);
    uint32_t tracks = bk_morph_group_tracks(morph), mask[64];
    CHECK(tracks <= 64);
    for (unsigned step = 0; step < 240; step++) {
      if (step % 15 == 0)
        CHECK(bk_actor_pose_request_active(
            actor, step ? slots[(step / 15) % count] : 0, e));
      BkActorVisibilityEdit visibility = {root, step % 11 == 5 ? 7 : 0};
      CHECK(bk_actor_pose_visibility(actor, &visibility, 1, e));
      CHECK(bk_actor_pose_state(actor, &before));
      uint64_t held = pose_hash(actor, model->frame_count, 0);
      uint64_t locals = pose_hash(actor, model->frame_count, 1);
      int submitted = -1;
      sample = saved;
      CHECK(bk_actor_pose_advance_frame(actor, &sample, &submitted, e));
      assert(submitted == !visibility.hidden);
      assert(held == pose_hash(actor, model->frame_count, 0));
      CHECK(bk_actor_pose_state(actor, &after));
      if (submitted) {
        for (uint32_t t = 0; t < tracks; t++)
          mask[t] = (step + t) % 3 ? UINT32_MAX : 0;
        CHECK(bk_morph_group_sample(morph, sample.from, step % 4 ? mask : NULL,
                                    step % 4 ? tracks : 0, e));
      } else {
        assert(!memcmp(&before, &after, sizeof(before)));
        assert(!memcmp(&sample, &saved, sizeof(sample)));
        assert(locals == pose_hash(actor, model->frame_count, 1));
        hidden++;
      }
      if (step % 5 == 0)
        bk_actor_pose_publish(actor);
      uint64_t poses = pose_hash(actor, model->frame_count, 1);
      hash = digest(hash, &poses, sizeof(poses));
      hash = digest(hash, &after, sizeof(after));
      for (uint32_t i = 0; i < model->submesh_count; i++) {
        const BkMorphMesh *mesh = bk_morph_group_mesh(morph, i);
        if (!mesh)
          continue;
        vertices += bk_morph_mesh_count(mesh);
        hash = digest(hash, bk_morph_mesh_vertices(mesh),
                      bk_morph_mesh_count(mesh) * sizeof(BkModelVertex));
      }
      frames++;
    }
    bk_actor_pose_destroy(actor);
    actor = NULL;
    bk_clip_set_destroy(clips);
    clips = NULL;
    bk_model_destroy(model);
    model = NULL;
    CHECK(bk_morph_group_sample(morph, 1.25f, NULL, 0, e));
    bk_morph_group_destroy(morph);
    morph = NULL;
  }
  printf("PASS fixed animation: %u profiles, %u frames, %u hidden, %llu "
         "vertices, FNV%016llx; clock/ANIM/MORP only\n",
         profiles, frames, hidden, (unsigned long long)vertices,
         (unsigned long long)hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "profile%u: %s\n", profiles, e);
  bk_model_playback_destroy(playback);
  bk_actor_pose_destroy(actor);
  bk_morph_group_destroy(morph);
  bk_model_destroy(model);
  bk_clip_set_destroy(clips);
  bk_blob_free(&raw);
  bk_resources_destroy(store);
  return rc;
}
