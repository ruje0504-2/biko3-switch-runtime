#ifndef BK_RESOURCE_STORE_H
#define BK_RESOURCE_STORE_H
#include "resource/assets.h"
typedef struct BkResourceStore BkResourceStore;
typedef struct {
  uint8_t *data;
  size_t size;
} BkBlob;
typedef enum {
  BK_RESOURCE_ERROR = -1,
  BK_RESOURCE_MISSING = 0,
  BK_RESOURCE_OK = 1
} BkResourceResult;
BkResourceStore *bk_resources_create(char error[256]);
void bk_resources_destroy(BkResourceStore *store);
/* Latest mount wins for this pack; missing entries fall through to base.
 * Corruption/read errors never fall through. A failed mount leaves state
 * intact. Calls and archive FILE handles are single-threaded. Names are ASCII
 * pack IDs. */
int bk_resources_mount(BkResourceStore *store, const char *pack,
                       const char *archive_path, char error[256]);
/* Read-only loose assets, such as original CKP routes outside PP archives.
 * Keys are case-insensitive basenames; ambiguous case variants are errors.
 * Same overlay/failure rules as archives. A required per-file size limit
 * prevents unbounded whole-file reads; streaming media uses a separate API. */
int bk_resources_mount_directory(BkResourceStore *store, const char *pack,
                                 const char *directory, size_t size_limit,
                                 char error[256]);
enum { BK_PATCH_CHINESE = 1, BK_PATCH_UNCENSORED = 2 };
/* Optional, read-only patch.pp. Uses the existing PP container with keyed
 * replacement/range records; absent files leave the Japanese assets intact. */
int bk_resources_load_patch(BkResourceStore *, const char *path, char error[256]);
unsigned bk_resources_patch_flags(const BkResourceStore *);
BkResourceResult bk_resources_read(BkResourceStore *store, const char *pack,
                                   const char *name, BkBlob *out,
                                   char error[256]);
void bk_blob_free(BkBlob *blob);
#endif
