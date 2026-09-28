#ifndef BK3_ASSETS_H
#define BK3_ASSETS_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  char name[65];
  uint32_t offset, size;
} BkEntry;
typedef struct {
  FILE *file;
  BkEntry *entries;
  uint32_t count;
  int negated;
} BkArchive;
typedef struct {
  uint32_t width, height;
  uint8_t *rgba;
} BkImage;
int bk_archive_open(BkArchive *a, const char *path, char error[256]);
void bk_archive_close(BkArchive *a);
const BkEntry *bk_archive_find(const BkArchive *a, const char *name);
int bk_archive_read(BkArchive *a, const BkEntry *e, uint8_t **bytes,
                    char error[256]);
int bk_image_decode(const uint8_t *bytes, size_t size, BkImage *image,
                    char error[256]);
void bk_image_free(BkImage *image);
int bk_tbl_decode(const uint8_t *data, size_t size, uint8_t **out,
                  size_t *length, char error[256]);
#endif
