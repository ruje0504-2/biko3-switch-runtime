#include "world/frame_tree.h"
#include <stdlib.h>
#include <string.h>
typedef struct {
  uint32_t parent, first, last, previous, next;
} Node;
struct BkFrameTree {
  uint32_t count;
  Node *nodes;
  uint32_t *stack;
};
BkFrameTree *bk_frame_tree_create(uint32_t count) {
  if (!count || count > BK_FRAME_TREE_LIMIT)
    return NULL;
  BkFrameTree *t = calloc(1, sizeof(*t));
  if (!t)
    return NULL;
  t->count = count;
  t->nodes = malloc(count * sizeof(*t->nodes));
  t->stack = malloc(count * sizeof(*t->stack));
  if (!t->nodes || !t->stack) {
    bk_frame_tree_destroy(t);
    return NULL;
  }
  for (uint32_t i = 0; i < count; ++i)
    t->nodes[i] = (Node){BK_FRAME_NONE, BK_FRAME_NONE, BK_FRAME_NONE,
                         BK_FRAME_NONE, BK_FRAME_NONE};
  return t;
}
void bk_frame_tree_destroy(BkFrameTree *t) {
  if (t) {
    free(t->nodes);
    free(t->stack);
    free(t);
  }
}
int bk_frame_tree_copy(BkFrameTree *out, const BkFrameTree *in) {
  if (!out || !in || out->count != in->count)
    return 0;
  if (out != in)
    memcpy(out->nodes, in->nodes, in->count * sizeof(*in->nodes));
  return 1;
}
int bk_frame_tree_copy_prefix(BkFrameTree *out, const BkFrameTree *in) {
  if (!out || !in || out->count < in->count)
    return 0;
  for (uint32_t i = 0; i < out->count; ++i) {
    const Node *n = &out->nodes[i];
    const uint32_t edges[] = {n->parent, n->first, n->last, n->previous,
                              n->next};
    for (unsigned j = 0; j < 5; ++j)
      if (edges[j] != BK_FRAME_NONE &&
          (i < in->count) != (edges[j] < in->count))
        return 0;
  }
  if (out != in)
    memcpy(out->nodes, in->nodes, in->count * sizeof(*in->nodes));
  return 1;
}
uint32_t bk_frame_tree_count(const BkFrameTree *t) { return t ? t->count : 0; }
uint32_t bk_frame_tree_parent(const BkFrameTree *t, uint32_t n) {
  return t && n < t->count ? t->nodes[n].parent : BK_FRAME_NONE;
}
uint32_t bk_frame_tree_first(const BkFrameTree *t, uint32_t n) {
  return t && n < t->count ? t->nodes[n].first : BK_FRAME_NONE;
}
uint32_t bk_frame_tree_next(const BkFrameTree *t, uint32_t n) {
  return t && n < t->count ? t->nodes[n].next : BK_FRAME_NONE;
}
static void unlink_node(BkFrameTree *t, uint32_t child) {
  Node *c = &t->nodes[child];
  if (c->parent == BK_FRAME_NONE)
    return;
  Node *p = &t->nodes[c->parent];
  if (c->previous != BK_FRAME_NONE)
    t->nodes[c->previous].next = c->next;
  else
    p->first = c->next;
  if (c->next != BK_FRAME_NONE)
    t->nodes[c->next].previous = c->previous;
  else
    p->last = c->previous;
  c->parent = c->previous = c->next = BK_FRAME_NONE;
}
int bk_frame_tree_attach(BkFrameTree *t, uint32_t parent, uint32_t child,
                         int *refresh) {
  if (!t || !refresh || parent >= t->count || child >= t->count || child == 0)
    return 0;
  for (uint32_t p = parent; p != BK_FRAME_NONE; p = t->nodes[p].parent)
    if (p == child)
      return 0;
  if (t->nodes[child].parent == parent) {
    *refresh = 0;
    return 1;
  }
  unlink_node(t, child);
  Node *p = &t->nodes[parent], *c = &t->nodes[child];
  c->parent = parent;
  c->previous = p->last;
  if (p->last == BK_FRAME_NONE)
    p->first = child;
  else
    t->nodes[p->last].next = child;
  p->last = child;
  *refresh = 1;
  return 1;
}
int bk_frame_tree_detach(BkFrameTree *t, uint32_t parent, uint32_t child) {
  if (!t || parent >= t->count || child >= t->count)
    return 0;
  if (t->nodes[child].parent == parent)
    unlink_node(t, child);
  return 1;
}
static int walk(BkFrameTree *t, uint32_t target, const uint32_t *hidden,
                BkFrameVisit *out, uint32_t capacity, uint32_t *count) {
  if (!t || !count || capacity < t->count - 1 || (t->count > 1 && !out))
    return 0;
  uint32_t used = 0, depth = 0, node = t->nodes[0].first;
  int draw = hidden != NULL, selected = target == 0;
  if (draw && hidden[target]) {
    *count = 0;
    return 1;
  }
  while (node != BK_FRAME_NONE || depth) {
    if (node == BK_FRAME_NONE) {
      node = t->stack[--depth];
      continue;
    }
    Node *n = &t->nodes[node];
    if (draw && node == target)
      selected = 1;
    out[used++] = (BkFrameVisit){node, draw && selected && !hidden[node]};
    uint32_t next = draw && node == target ? BK_FRAME_NONE : n->next;
    if (n->first != BK_FRAME_NONE && (!draw || !hidden[node])) {
      if (next != BK_FRAME_NONE)
        t->stack[depth++] = next;
      node = n->first;
    } else
      node = next;
  }
  *count = used;
  return 1;
}
int bk_frame_tree_refresh_walk(BkFrameTree *t, BkFrameVisit *out,
                               uint32_t capacity, uint32_t *count) {
  return walk(t, BK_FRAME_NONE, NULL, out, capacity, count);
}
int bk_frame_tree_draw_walk(BkFrameTree *t, uint32_t target,
                            const uint32_t *hidden, BkFrameVisit *out,
                            uint32_t capacity, uint32_t *count) {
  if (!t || target >= t->count || !hidden)
    return 0;
  return walk(t, target, hidden, out, capacity, count);
}
int bk_frame_tree_camera_path(BkFrameTree *t, uint32_t camera, uint32_t *out,
                              uint32_t capacity, uint32_t *count) {
  if (!t || !count || camera >= t->count || capacity < t->count - 1 ||
      (t->count > 1 && !out))
    return 0;
  uint32_t n = camera, used = 0;
  while (n != 0 && n != BK_FRAME_NONE) {
    t->stack[used++] = n;
    n = t->nodes[n].parent;
  }
  if (n == BK_FRAME_NONE)
    used = 0;
  for (uint32_t i = 0; i < used; ++i)
    out[i] = t->stack[used - 1 - i];
  *count = used;
  return 1;
}
