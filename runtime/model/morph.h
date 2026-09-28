#ifndef BK_MODEL_MORPH_H
#define BK_MODEL_MORPH_H
#include "model/model.h"
typedef struct BkModelMorph BkModelMorph;
typedef struct {
  float time;
  uint32_t interpolation, vertex_count;
  const BkModelVertex *vertices;
  float matrix[16];
} BkMorphKey;
typedef struct {
  uint32_t submesh, key_count;
} BkMorphTrack;
/* 0x41ac62 loader:72-byte group,24-byte tracks,12-byte key header,
 * vertex_count*60 bytes,64-byte matrix. Exported pointer-like IDs only
 * resolve the immutable model's submesh table; never dereferenced.
 * Owns decoded copies, so the source model can be released after creation.
 * Unused UV1..3 bits are preserved, including exporter nonfinite residue.
 * This decoder does not sample, retarget, select vertices or change meshes. */
BkModelMorph *bk_model_morph_create(const BkModel *model, char error[256]);
void bk_model_morph_destroy(BkModelMorph *morph);
uint32_t bk_model_morph_count(const BkModelMorph *morph);
const BkMorphTrack *bk_model_morph_track(const BkModelMorph *morph,
                                        uint32_t track);
const BkMorphKey *bk_model_morph_key(const BkModelMorph *morph,
                                    uint32_t track, uint32_t key);
#endif
