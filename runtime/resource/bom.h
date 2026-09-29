#ifndef BK_RESOURCE_BOM_H
#define BK_RESOURCE_BOM_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
  char parent[260], reference[260], child[260];
  char primary_aux[260], secondary_aux[260];
  char target_mesh[260], source_mesh[260], selection[260];
} BkBomBinding;
typedef struct {
  uint32_t mode, count;
  /* Metadata rows0/1 are retained for inspection;4a5870 ignores them and
   *4a4dc3 uses its actual caller-provided primary/secondary actors. */
  char primary_clip[260], secondary_clip[260];
  BkBomBinding bindings[4];
} BkBomConfig;
/* Complete packaged BOM4a5870 decode into a fresh configuration.34 strings,
 * four groups of eight, stop at first empty parent. Raw mode at4208 retained.
 * No model/node lookup, attachment, vertex copy, or successful fake physics.
 * Safe bounds/NUL validation, trailing bytes ignored; failure leaves out held.
 */
int bk_bom_decode(const void *, size_t, BkBomConfig *, char error[256]);
/*4a65fc appends decoded rows to the existing eight-slot native binding set.
 * The four-row file format and BkBomConfig ABI stay unchanged. Clip metadata
 * is inspection-only and never selects actors. The last file supplies mode,
 * including files with no bindings. Failure leaves the entire set unchanged. */
enum { BK_BOM_SET_CAPACITY = 8 };
typedef struct {
  uint32_t mode, count;
  BkBomBinding bindings[BK_BOM_SET_CAPACITY];
} BkBomBindingSet;
int bk_bom_binding_set_append(BkBomBindingSet *, const BkBomConfig *,
                              char error[256]);
int bk_bom_decode_append(const void *, size_t, BkBomBindingSet *, char error[256]);
#endif
