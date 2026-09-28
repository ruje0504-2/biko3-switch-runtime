/* Replay actual mesh fixtures emitted by original_bom_deform_oracle.py. */
#include "model/bom_deform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int read_bytes(FILE *f, void *out, size_t n) {
  return fread(out, 1, n, f) == n;
}
static uint64_t digest(uint64_t h, const void *data, size_t n) {
  const unsigned char *p = data;
  while (n--) {
    h ^= *p++;
    h *= UINT64_C(1099511628211);
  }
  return h;
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  FILE *f = fopen(argv[1], "rb");
  if (!f)
    return 1;
  char magic[4], error[256] = {0};
  uint32_t profiles = 0;
  unsigned frames = 0;
  uint64_t vertices = 0, hash = UINT64_C(14695981039346656037);
  int ok = 0;
  if (!read_bytes(f, magic, 4) || memcmp(magic, "BOM1", 4) ||
      !read_bytes(f, &profiles, 4) || profiles != 7)
    goto done;
  for (uint32_t profile = 0; profile < profiles; profile++) {
    BkBomMeshView views[16] = {0};
    float world[16][16];
    BkBomDeformBinding bindings[8] = {0};
    uint16_t *indices[8] = {0};
    BkBomDeform *d = NULL;
    uint32_t meshes = 0, count = 0;
    int profile_ok = 0;
    if (!read_bytes(f, &meshes, 4) || !read_bytes(f, &count, 4) ||
        meshes > 16 || count > 8)
      goto cleanup;
    for (uint32_t i = 0; i < meshes; i++) {
      if (!read_bytes(f, &views[i].count, 4) || !views[i].count ||
          views[i].count > 2000000 || !read_bytes(f, world[i], 64))
        goto cleanup;
      views[i].world = world[i];
      size_t n = (size_t)views[i].count * sizeof(BkModelVertex);
      views[i].vertices = malloc(n);
      if (!views[i].vertices || !read_bytes(f, views[i].vertices, n))
        goto cleanup;
    }
    for (uint32_t i = 0; i < count; i++) {
      uint32_t n;
      if (!read_bytes(f, &bindings[i].source, 4) ||
          !read_bytes(f, &bindings[i].target, 4) || !read_bytes(f, &n, 4) ||
          n > 2000000)
        goto cleanup;
      bindings[i].count = n;
      if (n) {
        indices[i] = malloc((size_t)n * 2);
        if (!indices[i] || !read_bytes(f, indices[i], (size_t)n * 2))
          goto cleanup;
        bindings[i].indices = indices[i];
      }
    }
    d = bk_bom_deform_create(views, meshes, bindings, count, error);
    if (!d)
      goto cleanup;
    for (unsigned step = 0; step < 120; step++) {
      int32_t flags[8];
      for (uint32_t i = 0; i < count; i++)
        flags[i] = (int32_t[]){0, 1, 2, -1}[(step + i) % 4];
      for (uint32_t i = 0; i < meshes; i++) {
        world[i][12] = (float)((double)world[i][12] + .002 * (i + 1));
        world[i][13] = (float)((double)world[i][13] - .001);
      }
      uint32_t chosen = step % (count + 2),
               target = chosen < count ? bindings[chosen].target
                                       : BK_MODEL_NONE - (chosen - count);
      if (!bk_bom_deform_draw(d, target, views, meshes, flags, error))
        goto cleanup;
      for (uint32_t i = 0; i < meshes; i++) {
        hash = digest(hash, views[i].vertices,
                      (size_t)views[i].count * sizeof(BkModelVertex));
        vertices += views[i].count;
      }
      frames++;
    }
    profile_ok = 1;
  cleanup:
    bk_bom_deform_destroy(d);
    for (unsigned i = 0; i < 16; i++)
      free(views[i].vertices);
    for (unsigned i = 0; i < 8; i++)
      free(indices[i]);
    if (!profile_ok)
      goto done;
  }
  if (fgetc(f) != EOF)
    goto done;
  ok = 1;
  printf("PASS BOM actual fixtures: %u profiles %u frames %llu vertices "
         "FNV%016llx\n",
         profiles, frames, (unsigned long long)vertices,
         (unsigned long long)hash);
done:
  fclose(f);
  if (!ok)
    fprintf(stderr, "BOM probe failed: %s\n", error);
  return ok ? 0 : 1;
}
