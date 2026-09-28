#ifndef BK_MODEL_TRIANGLES_H
#define BK_MODEL_TRIANGLES_H
#include "model/model.h"
typedef struct {
  uint16_t *indices;
  uint32_t count;
} BkModelTriangles;
/* Owned triangle-list upload indices. Original mesh header+56 is topology:
 * 0=indexed list, 1=nonindexed strip, 2=nonindexed fan. Strip/fan use the
 * complete sequential vertex stream, ignoring stored indices (0x445071).
 * Alternating strip winding and degenerate triangles are retained.
 * Unsupported header state/texture modes fail; output is empty on failure. */
int bk_model_triangles(const BkModel *model, uint32_t submesh,
                       BkModelTriangles *out, char error[256]);
void bk_model_triangles_free(BkModelTriangles *triangles);
#endif
