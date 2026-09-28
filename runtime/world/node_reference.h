#ifndef BK_WORLD_NODE_REFERENCE_H
#define BK_WORLD_NODE_REFERENCE_H
/* Independent submitted local, published world and cached parent world. */
typedef struct {
  float local[16], world[16], parent_world[16];
} BkNodeReference;
/* 422c49 changes only local translation, using OLD published world and the
 * cached parent. 4230bd changes only the local3x3. Both immediately update
 * this node's world, without touching parent cache, descendants or topology.
 * Degenerate directions follow original zero-vector normalization; invalid
 * matrices, a singular parent, or overflow fail atomically. */
int bk_node_reference_position(BkNodeReference *, const float reference[16],
                               const float position[3], char error[256]);
int bk_node_reference_orientation(BkNodeReference *, const float reference[16],
                                  const float forward[3], const float up[3],
                                  char error[256]);
/*4241e3: axis rotation * reference world, retain old node world XYZ, then
 * inverse cached parent; replaces the full local and immediately its world. */
int bk_node_reference_rotation(BkNodeReference *, const float reference[16],
                               const float axis[3], float radians,
                               char error[256]);
/*42363b: mode0 replaces local; mode1 prepends rotation; all other modes append.
 * Nonzero modes retain local translation, not world translation. */
int bk_node_local_rotation(BkNodeReference *, int mode, const float axis[3],
                           float radians, char error[256]);
/*422ea4: node world * inverse reference world, return raw translation XYZ. */
int bk_node_reference_offset(float out[3], const float world[16],
                             const float reference[16], char error[256]);
/*425196 mode0: replace world3x3 by Y-up aim, then local=world*inverse(old
 * parent). Does not recompose world or change other world fields. Original
 * zero direction normalization is retained; later view inversion may reject
 * a degenerate result. Target is world-space; original reference is unused. */
int bk_node_reference_aim(BkNodeReference *, const float target[3],
                          char error[256]);
#endif
