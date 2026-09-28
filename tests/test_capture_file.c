#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "resource/bitmap.h"
#include "save/capture_file.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void) {
  char e[256], root[] = "/tmp/bk-capture-XXXXXX", path[256], tmp[256];
  assert(mkdtemp(root));
  BkCaptureFiles *files = bk_capture_files_create(root, e);
  assert(files);
  BkBlob read = {0};
  assert(bk_capture_file_read_pause(files, 64, &read, e) ==
         BK_RESOURCE_MISSING);
  assert(bk_capture_file_remove_pause(files, e));
  BkBlob b = {(uint8_t *)"first", 5};
  assert(bk_capture_file_write(files, 0, "sy_99.bmp", &b, e));
  assert(bk_capture_file_read_pause(files, 4, &read, e) == BK_RESOURCE_ERROR);
  assert(!read.data && !read.size);
  assert(bk_capture_file_read_pause(files, 5, &read, e) == BK_RESOURCE_OK);
  assert(read.size == 5 && !memcmp(read.data, "first", 5));
  uint8_t *owned = read.data;
  assert(bk_capture_file_read_pause(files, 64, &read, e) == BK_RESOURCE_ERROR);
  assert(read.data == owned && read.size == 5);
  bk_blob_free(&read);
  snprintf(path, sizeof(path), "%s/sy_99.bmp", root);
  snprintf(tmp, sizeof(tmp), "%s/sy_99.bmp.part", root);
  assert(!mkdir(tmp, 0700));
  b = (BkBlob){(uint8_t *)"second", 6};
  assert(!bk_capture_file_write(files, 0, "sy_99.bmp", &b, e));
  FILE *f = fopen(path, "rb");
  assert(f);
  char data[8] = {0};
  assert(fread(data, 1, 8, f) == 5);
  assert(!fclose(f));
  assert(!memcmp(data, "first", 5));
  assert(!rmdir(tmp));
  assert(bk_capture_file_write(files, 0, "sy_99.bmp", &b, e));
  assert(!bk_capture_file_write(files, 1, "../escape.bmp", &b, e));
  assert(!bk_capture_file_write(files, 0, "wrong.bmp", &b, e));
  BkCaptureTime t = {2026, 9, 27, 23, 58, 59, 99};
  char name[128];
  assert(bk_capture_photo_name(name, 4, &t));
  assert(!strcmp(name, "mi_2026_0927_2358_5999.bmp"));
  assert(bk_capture_file_write(files, 1, name, &b, e));
  for (unsigned w = 1; w <= 33; ++w)
    for (unsigned h = 1; h <= 11; h += 5) {
      BkImage im = {w, h, malloc((size_t)w * h * 4)}, decoded = {0};
      assert(im.rgba);
      for (size_t p = 0; p < (size_t)w * h * 4; ++p)
        im.rgba[p] = (uint8_t)(p * 17 + w);
      BkBlob raw = {0};
      assert(bk_bitmap_encode(&im, &raw, e));
      uint8_t *old = raw.data;
      assert(!bk_bitmap_encode(&im, &raw, e));
      assert(raw.data == old);
      assert(bk_image_decode(raw.data, raw.size, &decoded, e));
      assert(decoded.width == w && decoded.height == h);
      for (size_t p = 0; p < (size_t)w * h; ++p) {
        assert(!memcmp(im.rgba + p * 4, decoded.rgba + p * 4, 3));
        assert(decoded.rgba[p * 4 + 3] == 255);
      }
      bk_blob_free(&raw);
      bk_image_free(&decoded);
      bk_image_free(&im);
    }
  assert(bk_capture_file_remove_pause(files, e));
  assert(bk_capture_file_remove_pause(files, e));
  assert(bk_capture_file_read_pause(files, 64, &read, e) ==
         BK_RESOURCE_MISSING);
  assert(!mkdir(path, 0700));
  assert(!bk_capture_file_remove_pause(files, e));
  assert(!rmdir(path));
  snprintf(path, sizeof(path), "%s/album/%s", root, name);
  f = fopen(path, "rb");
  assert(f && fread(data, 1, 8, f) == 6 && !memcmp(data, "second", 6));
  assert(!fclose(f));
  bk_capture_files_destroy(files);
  assert(!remove(path));
  snprintf(path, sizeof(path), "%s/album", root);
  assert(!rmdir(path));
  assert(!rmdir(root));
  puts("PASS bitmap odd-row roundtrips and capture files, failed replacement "
       "preserves old data");
}
