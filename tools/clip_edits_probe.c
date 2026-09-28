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
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= UINT64_C(1099511628211);
  }
  return h;
}
static uint64_t matrices(BkActorPose *a, uint32_t count) {
  uint64_t h = UINT64_C(14695981039346656037);
  for (uint32_t i = 0; i < count; ++i) {
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
  int rc = 1;
  BkResourceStore *store = bk_resources_create(e);
  BkBlob raw = {0};
  BkClipSet *clips = NULL;
  BkModel *model = NULL;
  BkActorPose *a = NULL, *sibling = NULL;
  uint64_t hash = UINT64_C(14695981039346656037);
  unsigned frames = 0;
  CHECK(store);
  snprintf(path, sizeof(path), "%s/bk3_08.pp", argv[1]);
  CHECK(bk_resources_mount(store, "bk3_08", path, e));
  for (unsigned group = 1; group <= 5; ++group)
    for (unsigned variant = 0; variant < 3; ++variant) {
      snprintf(name, sizeof(name), "h%02u_%02u.xan", group,
               variant == 2 ? 20 : variant);
      CHECK(bk_resources_read(store, "bk3_08", name, &raw, e) ==
            BK_RESOURCE_OK);
      clips = bk_clip_set_decode(raw.data, raw.size, e);
      bk_blob_free(&raw);
      CHECK(clips);
      CHECK(bk_resources_read(store, "bk3_08", bk_clip_model_name(clips), &raw,
                              e) == BK_RESOURCE_OK);
      CHECK(bk_model_decode(raw.data, raw.size, &model, e) == BK_MODEL_OK);
      bk_blob_free(&raw);
      uint32_t root = BK_MODEL_NONE;
      for (uint32_t i = 0; i < model->frame_count; ++i)
        if (model->frames[i].parent_index == BK_MODEL_NONE) {
          assert(root == BK_MODEL_NONE);
          root = i;
        }
      CHECK(root != BK_MODEL_NONE);
      a = bk_actor_pose_create(model, clips, root, NULL, (float[3]){0}, 0, 4, 0,
                               e);
      CHECK(a);
      sibling = bk_actor_pose_create(model, clips, root, NULL, (float[3]){0}, 0,
                                     4, 0, e);
      CHECK(sibling);
      BkClipState sibling_clock;
      assert(bk_actor_pose_state(sibling, &sibling_clock));
      uint64_t sibling_matrices = matrices(sibling, model->frame_count);
      for (unsigned step = 0; step < 240; ++step) {
        const unsigned slots[] = {4, 5, 9, 13};
        unsigned slot = slots[(step / 15) % 4];
        BkClipTiming timing;
        assert(bk_actor_pose_timing(a, slot, &timing));
        BkClipEdit edits[] = {{slot, BK_CLIP_EDIT_CHAIN, step % 3 != 0, 0, 0},
                              {slot, BK_CLIP_EDIT_NEXT, 0,
                               (int32_t)slots[((step / 15) + 1) % 4], 0},
                              {slot, BK_CLIP_EDIT_SOURCE, 0, 0, timing.start}};
        uint64_t before = matrices(a, model->frame_count);
        CHECK(bk_actor_pose_edit_clips(a, edits, 3, e));
        CHECK(
            bk_actor_pose_request_mode(a, slot, BK_CLIP_REQUEST_CONFIGURED, e));
        assert(before == matrices(a, model->frame_count));
        BkClipState state, old;
        assert(bk_actor_pose_state(a, &old));
        BkClipEdit invalid[] = {{slot, BK_CLIP_EDIT_CHAIN, 0, 0, 0},
                                {slot, BK_CLIP_EDIT_SOURCE, 0, 0, NAN}};
        assert(!bk_actor_pose_edit_clips(a, invalid, 2, e));
        assert(bk_actor_pose_state(a, &state) &&
               !memcmp(&old, &state, sizeof(old)));
        int32_t chain, next;
        assert(bk_actor_pose_clip_link(a, slot, &chain, &next));
        assert(chain == edits[0].chain && next == edits[1].next);
        CHECK(bk_actor_pose_advance(
            a, -1, (float[]){0, .016f, .1f, .5f, 2}[step % 5], e));
        assert(bk_actor_pose_clip_link(a, slot, &chain, &next) &&
               chain == edits[0].chain && next == edits[1].next);
        assert(bk_actor_pose_clip_link(sibling, slot, &chain, &next));
        const BkClipDefinition *d = bk_clip_definition(clips, slot);
        assert(chain == d->chain && next == d->next);
        assert(bk_actor_pose_state(sibling, &state) &&
               !memcmp(&sibling_clock, &state, sizeof(state)));
        assert(matrices(sibling, model->frame_count) == sibling_matrices);
        bk_actor_pose_publish(a);
        uint64_t h = matrices(a, model->frame_count);
        hash = digest(hash, &h, sizeof(h));
        assert(bk_actor_pose_state(a, &state));
        hash = digest(hash, &state, sizeof(state));
        ++frames;
      }
      bk_actor_pose_destroy(a);
      a = NULL;
      bk_actor_pose_destroy(sibling);
      sibling = NULL;
      bk_model_destroy(model);
      model = NULL;
      bk_clip_set_destroy(clips);
      clips = NULL;
    }
  printf("PASS clip edits: 15 real actors, %u frames, private siblings, "
         "FNV%016llx\n",
         frames, (unsigned long long)hash);
  rc = 0;
done:
  if (rc)
    fprintf(stderr, "%s: %s\n", name, e);
  bk_actor_pose_destroy(a);
  bk_actor_pose_destroy(sibling);
  bk_model_destroy(model);
  bk_clip_set_destroy(clips);
  bk_blob_free(&raw);
  bk_resources_destroy(store);
  return rc;
}
