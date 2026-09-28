#include "model/bom_deform.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static const float I[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
int main(void) {
  char e[256];
  float w[16];
  memcpy(w, I, 64);
  BkModelVertex src[2] = {0}, dst[2] = {0};
  src[0].position[0] = src[1].position[0] = 2;
  src[0].normal[1] = 4;
  src[1].normal[1] = 8;
  dst[0].uv[3][0] = NAN;
  dst[0].beta = 99;
  dst[1].position[0] = 3;
  BkBomMeshView views[] = {{src, 2, w}, {dst, 2, I}};
  uint16_t ix[] = {0, 0, 1};
  BkBomDeformBinding b = {0, 1, ix, 3};
  BkBomDeform *d = bk_bom_deform_create(views, 2, &b, 1, e);
  assert(d);
  int32_t group, flags[1] = {0};
  const uint32_t *map;
  size_t count;
  assert(bk_bom_deform_mapping(d, 0, &group, &map, &count) && group == 0 &&
         count == 3);
  for (unsigned i = 0; i < 3; i++)
    assert(map[i] == 0); /* first equal candidate */
  ix[0] = 65535;         /* copied selection, not borrowed */
  BkBomDeformBinding plan;
  assert(bk_bom_deform_plan(d, 0, &plan));
  assert(plan.source == 0 && plan.target == 1 && plan.count == 3 &&
         plan.indices != ix && plan.indices[0] == 0);
  BkBomDeformBinding saved = plan;
  assert(!bk_bom_deform_plan(d, 1, &plan));
  assert(!memcmp(&saved, &plan, sizeof(plan)));
  assert(!bk_bom_deform_plan(d, 0, NULL));
  w[12] = 5;
  assert(bk_bom_deform_draw(d, 1, views, 2, flags, e));
  assert(dst[0].position[0] == 7 && dst[0].normal[1] == 4 &&
         dst[0].beta == 99 && isnan(dst[0].uv[3][0]));
  BkModelVertex before[2];
  memcpy(before, dst, sizeof(dst));
  w[15] = 0;
  assert(!bk_bom_deform_draw(d, 1, views, 2, flags, e));
  assert(!memcmp(before, dst, sizeof(dst)));
  flags[0] = 1;
  assert(bk_bom_deform_draw(d, 1, views, 2, flags,
                            e)); /* disabled skips invalid world */
  flags[0] = 2;
  assert(!bk_bom_deform_draw(d, 1, views, 2, flags, e));
  w[15] = 1;
  src[0].normal[0] = INFINITY;
  assert(!bk_bom_deform_draw(d, 1, views, 2, flags, e));
  assert(!memcmp(before, dst, sizeof(dst)));
  bk_bom_deform_destroy(d);
  assert(!bk_bom_deform_create(views, 2, &b, 1, e)); /* bad target index */
  ix[0] = 0;
  b.count = 1;
  src[0].normal[0] = 0;
  w[12] = 100005;
  assert(
      !bk_bom_deform_create(views, 2, &b, 1, e)); /* exactly100000 from dst7 */
  w[12] = 100004;
  d = bk_bom_deform_create(views, 2, &b, 1, e);
  assert(d);
  bk_bom_deform_destroy(d);
  BkBomMeshView overlap[] = {{src, 2, I}, {src + 1, 1, I}};
  assert(!bk_bom_deform_create(overlap, 2, NULL, 0, e));
  /* Aliased source/target, duplicate writes read the previous result. */
  src[0].position[0] = 1;
  src[1].position[0] = 50;
  memcpy(w, I, 64);
  BkBomDeformBinding alias = {0, 0, ix, 2};
  ix[0] = ix[1] = 0;
  d = bk_bom_deform_create(views, 2, &alias, 1, e);
  assert(d);
  w[12] = 1;
  flags[0] = -1;
  assert(bk_bom_deform_draw(d, 0, views, 2, flags, e));
  assert(src[0].position[0] == 3);
  bk_bom_deform_destroy(d);
  puts("PASS BOM nearest ties/radius, copied selection, raw flags, atomic "
       "failures, alias duplicates");
}
