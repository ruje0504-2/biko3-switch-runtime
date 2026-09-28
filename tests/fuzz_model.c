#include "model/model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t rng = 0x26719211;
static uint32_t next(void) {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}
static void word(uint8_t *p, uint32_t v) {
  for (unsigned j = 0; j < 4; j++)
    p[j] = (uint8_t)(v >> (8 * j));
}
static void number(uint8_t *p, float v) {
  uint32_t b;
  memcpy(&b, &v, 4);
  word(p, b);
}
int main(void) {
  uint8_t base[2048] = {0}, input[2048];
  size_t p = 12;
  memcpy(base, "OBJM", 4);
  memcpy(base + p, "MATE", 4);
  word(base + p + 4, 140);
  p += 8;
  memcpy(base + p, "material", 8);
  word(base + p + 64, 10);
  for (unsigned j = 0; j < 8; j++)
    number(base + p + 68 + j * 4, 1);
  p += 140;
  memcpy(base + p, "FRAM", 4);
  word(base + p + 4, 396);
  p += 8;
  memcpy(base + p, "root", 4);
  word(base + p + 64, 100);
  word(base + p + 176, 30);
  for (unsigned j = 0; j < 4; j++)
    number(base + p + 68 + (j * 5) * 4, 1);
  p += 396;
  memcpy(base + p, "MESH", 4);
  word(base + p + 4, 72 + 332 + 180 + 6);
  p += 8;
  memcpy(base + p, "mesh", 4);
  word(base + p + 64, 30);
  word(base + p + 68, 1);
  p += 72;
  word(base + p + 4, 10);
  word(base + p + 64, 3);
  word(base + p + 68, 3);
  p += 332;
  for (unsigned j = 0; j < 3; j++) {
    number(base + p + j * 60 + (j % 3) * 4, 1);
    number(base + p + j * 60 + 24, 1);
  }
  p += 180;
  base[p + 2] = 1;
  base[p + 4] = 2;
  p += 6;
  char error[256];
  BkModel *model = NULL;
  if (bk_model_decode(base, p, &model, error) != BK_MODEL_OK) {
    fprintf(stderr, "%s\n", error);
    return 1;
  }
  bk_model_destroy(model);
  unsigned accepted = 0;
  for (unsigned run = 0; run < 12000; run++) {
    memcpy(input, base, p);
    size_t size = p;
    if (run % 3 == 0)
      size = next() % (p + 1);
    else
      for (unsigned k = 0, mutations = 1 + next() % 12; k < mutations; k++)
        input[next() % p] ^= (uint8_t)(1 + next() % 255);
    BkModelResult result = bk_model_decode(input, size, &model, error);
    if (result == BK_MODEL_OK) {
      accepted++;
      float *world = calloc(model->frame_count * 16 + 1, sizeof(float));
      if (!world)
        return 1;
      bk_model_world_matrices(model, world, model->frame_count * 16, error);
      free(world);
    } else if (model) {
      fprintf(stderr, "failure left a live model\n");
      return 1;
    }
    bk_model_destroy(model);
    model = NULL;
  }
  printf(
      "PASS: 12000 mutated/truncated OBJM cases; %u valid mutations decoded\n",
      accepted);
  return 0;
}
