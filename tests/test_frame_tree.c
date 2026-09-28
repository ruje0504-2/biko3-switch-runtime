#include "world/frame_tree.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void attach(BkFrameTree *t, uint32_t p, uint32_t c) {
  int refresh = -1;
  assert(bk_frame_tree_attach(t, p, c, &refresh) && refresh == 1);
}
int main(void) {
  BkFrameTree *t = bk_frame_tree_create(8);
  assert(t);
  int refresh = 99;
  attach(t, 0, 1);
  attach(t, 1, 2);
  attach(t, 1, 3);
  attach(t, 2, 4);
  attach(t, 0, 5);
  attach(t, 5, 6);
  assert(bk_frame_tree_attach(t, 1, 2, &refresh) && refresh == 0);
  refresh = 99;
  assert(!bk_frame_tree_attach(t, 4, 1, &refresh) && refresh == 99);
  assert(!bk_frame_tree_attach(t, 2, 0, &refresh));
  assert(!bk_frame_tree_attach(t, 2, 2, &refresh));
  uint32_t hidden[8] = {0}, n = 99, path[8];
  BkFrameVisit v[8];
  assert(bk_frame_tree_draw_walk(t, 2, hidden, v, 8, &n) && n == 5);
  uint32_t order[] = {1, 2, 4, 5, 6};
  for (unsigned i = 0; i < n; ++i)
    assert(v[i].node == order[i] && v[i].submit == (i > 0));
  /* Request2 skips sibling3 but the native latch still draws5/6. */
  hidden[1] = 1;
  assert(bk_frame_tree_draw_walk(t, 0, hidden, v, 8, &n) && n == 3);
  assert(v[0].node == 1 && v[0].submit == 0 && v[1].node == 5);
  assert(bk_frame_tree_draw_walk(t, 1, hidden, v, 8, &n) && n == 0);
  assert(bk_frame_tree_refresh_walk(t, v, 8, &n) && n == 6);
  assert(bk_frame_tree_camera_path(t, 4, path, 8, &n) && n == 3 &&
         path[0] == 1 && path[1] == 2 && path[2] == 4);
  assert(bk_frame_tree_camera_path(t, 7, path, 8, &n) && n == 0);
  assert(bk_frame_tree_detach(t, 0, 2) && bk_frame_tree_parent(t, 2) == 1);
  assert(bk_frame_tree_detach(t, 1, 2) &&
         bk_frame_tree_parent(t, 2) == BK_FRAME_NONE);
  attach(t, 1, 2);
  assert(bk_frame_tree_first(t, 1) == 3 && bk_frame_tree_next(t, 3) == 2);
  memset(v, 0x5a, sizeof(v));
  BkFrameVisit before[8];
  memcpy(before, v, sizeof(v));
  n = 99;
  assert(!bk_frame_tree_draw_walk(t, 0, hidden, v, 6, &n) && n == 99 &&
         !memcmp(before, v, sizeof(v)));
  assert(!bk_frame_tree_draw_walk(t, 8, hidden, v, 8, &n));
  BkFrameTree *expanded = bk_frame_tree_create(11);
  assert(expanded);
  attach(expanded, 8, 9);
  attach(expanded, 9, 10);
  assert(bk_frame_tree_copy_prefix(expanded, t));
  for (unsigned i = 0; i < 8; i++) {
    assert(bk_frame_tree_parent(expanded, i) == bk_frame_tree_parent(t, i));
    assert(bk_frame_tree_first(expanded, i) == bk_frame_tree_first(t, i));
    assert(bk_frame_tree_next(expanded, i) == bk_frame_tree_next(t, i));
  }
  assert(bk_frame_tree_parent(expanded, 8) == BK_FRAME_NONE &&
         bk_frame_tree_parent(expanded, 10) == 9);
  attach(expanded, 2, 8);
  assert(!bk_frame_tree_copy_prefix(expanded, t));
  assert(bk_frame_tree_parent(expanded, 8) == 2);
  assert(!bk_frame_tree_copy_prefix(t, expanded));
  assert(bk_frame_tree_detach(expanded, 2, 8));
  assert(bk_frame_tree_copy_prefix(expanded, t));
  /* The inverse crossing also must not leave a suffix pointing into a
   * replaced prefix. */
  attach(expanded, 10, 5);
  assert(!bk_frame_tree_copy_prefix(expanded, t));
  assert(bk_frame_tree_parent(expanded, 5) == 10);
  bk_frame_tree_destroy(expanded);
  bk_frame_tree_destroy(t);
  assert(!bk_frame_tree_create(0) &&
         !bk_frame_tree_create(BK_FRAME_TREE_LIMIT + 1));
  t = bk_frame_tree_create(1025);
  assert(t);
  for (unsigned i = 1; i < 1025; ++i)
    attach(t, i - 1, i);
  BkFrameVisit deep[1025];
  uint32_t flags[1025] = {0};
  assert(bk_frame_tree_draw_walk(t, 0, flags, deep, 1025, &n) && n == 1024 &&
         deep[1023].node == 1024);
  bk_frame_tree_destroy(t);
  puts("PASS ordered reparent/detach, cycle rejection, native requested-root "
       "latch, hidden refresh/draw, camera path and deep iterative walk");
}
