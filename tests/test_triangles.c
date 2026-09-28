#include "model/triangles.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  char error[256];
  uint8_t header[332] = {0};
  uint16_t indices[] = {5, 3, 2, 1, 4, 0};
  BkModelSubmesh sub = {
      .vertex_count = 6, .index_count = 6, .indices = indices};
  BkModel model = {.source = header,
                   .source_size = sizeof(header),
                   .submeshes = &sub,
                   .submesh_count = 1};
  BkModelTriangles out;
  assert(bk_model_triangles(&model, 0, &out, error));
  assert(out.count == 6 && !memcmp(out.indices, indices, sizeof(indices)));
  bk_model_triangles_free(&out);
  header[56] = 1;
  sub.indices = NULL; /* Strip/fan ignore the file's indexed-list payload. */
  sub.index_count = 0;
  const uint16_t strip[] = {0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5};
  assert(bk_model_triangles(&model, 0, &out, error));
  assert(out.count == 12 && !memcmp(out.indices, strip, sizeof(strip)));
  bk_model_triangles_free(&out);
  header[56] = 2;
  const uint16_t fan[] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5};
  assert(bk_model_triangles(&model, 0, &out, error));
  assert(out.count == 12 && !memcmp(out.indices, fan, sizeof(fan)));
  bk_model_triangles_free(&out);
  sub.vertex_count = 65536;
  assert(bk_model_triangles(&model, 0, &out, error));
  assert(out.count == 65534 * 3 && out.indices[out.count - 1] == 65535);
  bk_model_triangles_free(&out);
  sub.vertex_count = 65537;
  assert(!bk_model_triangles(&model, 0, &out, error) && !out.indices &&
         !out.count);
  sub.vertex_count = 2;
  assert(!bk_model_triangles(&model, 0, &out, error));
  sub.vertex_count = 6;
  header[56] = 3;
  assert(!bk_model_triangles(&model, 0, &out, error));
  header[56] = 0;
  assert(!bk_model_triangles(&model, 0, &out, error));
  sub.indices = indices;
  sub.index_count = 6;
  indices[5] = 6;
  assert(!bk_model_triangles(&model, 0, &out, error));
  indices[5] = 0;
  for (unsigned i = 0; i < sizeof(header); i++) {
    model.source_size = i;
    assert(!bk_model_triangles(&model, 0, &out, error));
  }
  model.source_size = sizeof(header);
  for (unsigned i = 0; i < 2; i++) {
    header[i ? 60 : 0] = 1;
    assert(!bk_model_triangles(&model, 0, &out, error));
    header[i ? 60 : 0] = 0;
  }
  assert(bk_model_triangles(&model, 0, &out, error));
  bk_model_triangles_free(&out);
  puts("PASS triangle list/strip/fan conversion, winding, index domain and "
       "bounds");
  return 0;
}
