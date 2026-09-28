#include "resource/assets.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static int fail(char *e, const char *s) {
  snprintf(e, 256, "%s", s);
  return 0;
}

int bk_tbl_decode(const uint8_t *data, size_t n, uint8_t **out, size_t *length,
                  char e[256]) {
  *out = NULL;
  *length = 0;
  if (n < 5)
    return fail(e, "TBL header truncated");
  size_t declared = u32(data) ^ 0xa67f54cbU;
  if (declared < 2304 || declared > 16 * 1024 * 1024 || (declared - 2304) % 80)
    return fail(e, "TBL allocation size invalid");
  size_t target = declared - 256; /* writer scratch is outside bucket records */
  uint8_t *src = malloc(n - 4), *dst = malloc(declared), ring[4096] = {0};
  if (!src || !dst) {
    free(src);
    free(dst);
    return fail(e, "TBL allocation failed");
  }
  memcpy(src, data + 4, n - 4);
  n -= 4;
  const uint8_t key[4] = {0x2f, 0xca, 0xd8, 0x35};
  for (size_t i = 0; i < n / 4 * 4; i++)
    src[i] ^= key[i & 3];
  size_t p = 0, k = 0;
  unsigned cursor = 0xfee, flags = 0;
  while (k < target) {
    flags >>= 1;
    if (!(flags & 256)) {
      if (p >= n)
        goto truncated;
      flags = src[p++] | 0xff00;
    }
    if (flags & 1) {
      if (p >= n)
        goto truncated;
      dst[k++] = ring[cursor] = src[p++];
      cursor = (cursor + 1) & 4095;
    } else {
      if (p + 2 > n)
        goto truncated;
      unsigned a = src[p++], b = src[p++], start = a | ((b & 240) << 4),
               run = (b & 15) + 3;
      if (run > declared - k)
        goto truncated;
      for (unsigned j = 0; j < run; j++) {
        uint8_t v = ring[(start + j) & 4095];
        dst[k++] = ring[cursor] = v;
        cursor = (cursor + 1) & 4095;
      }
    }
  }
  free(src);
  *out = dst;
  *length = target;
  return 1;
truncated:
  free(src);
  free(dst);
  return fail(e, "TBL compressed data truncated or overlong");
}

static int read_all(FILE *f, uint8_t **out, size_t *size, char *e) {
  if (fseek(f, 0, SEEK_END))
    return fail(e, "file seek failed");
  long n = ftell(f);
  if (n < 0 || (unsigned long)n > 256U * 1024 * 1024)
    return fail(e, "file too large");
  rewind(f);
  *out = malloc(n ? n : 1);
  *size = (size_t)n;
  if (!*out)
    return fail(e, "file allocation failed");
  if (fread(*out, 1, *size, f) != *size) {
    free(*out);
    *out = NULL;
    return fail(e, "file read failed");
  }
  return 1;
}

void bk_archive_close(BkArchive *a) {
  if (a->file)
    fclose(a->file);
  free(a->entries);
  memset(a, 0, sizeof(*a));
}

int bk_archive_open(BkArchive *a, const char *path, char e[256]) {
  memset(a, 0, sizeof(*a));
  a->file = fopen(path, "rb");
  if (!a->file) {
    snprintf(e, 256, "cannot open %.220s", path);
    return 0;
  }
  if (fseek(a->file, 0, SEEK_END))
    goto invalid;
  long end = ftell(a->file);
  rewind(a->file);
  if (end < 0 || (unsigned long)end > UINT32_MAX)
    goto invalid;
  size_t plen = strlen(path);
  char tbl[1024];
  if (plen < 3 || plen + 1 >= sizeof(tbl))
    goto invalid;
  memcpy(tbl, path, plen + 1);
  memcpy(tbl + plen - 2, "tbl", 4);
  FILE *tf = fopen(tbl, "rb");
  uint8_t *table = NULL, *packed = NULL;
  size_t tn = 0, pn = 0;
  if (tf) {
    int ok = read_all(tf, &packed, &pn, e);
    fclose(tf);
    if (ok)
      ok = bk_tbl_decode(packed, pn, &table, &tn, e);
    free(packed);
    if (!ok)
      goto error;
    a->count = (uint32_t)((tn - 2048) / 80);
    size_t cursor = 0;
    for (unsigned b = 0; b < 256; b++) {
      uint32_t start = u32(table + b * 8), count = u32(table + b * 8 + 4);
      if (start != cursor || (uint64_t)start + (uint64_t)count * 80 > tn - 2048)
        goto bad_table;
      for (uint32_t j = 0; j < count; j++)
        if (table[2048 + start + j * 80 + 16] != b)
          goto bad_table;
      cursor += (size_t)count * 80;
    }
    if (cursor != tn - 2048)
      goto bad_table;
    a->entries = calloc(a->count ? a->count : 1, sizeof(*a->entries));
    if (!a->entries)
      goto bad_table;
    for (uint32_t i = 0; i < a->count; i++) {
      const uint8_t *r = table + 2048 + i * 80;
      if (u32(r) != 80 || u32(r + 12) != 0 || !memchr(r + 16, 0, 64))
        goto bad_table;
      a->entries[i].offset = u32(r + 4);
      a->entries[i].size = u32(r + 8);
      memcpy(a->entries[i].name, r + 16, 64);
    }
    free(table);
    table = NULL;
  } else {
    uint8_t h[8];
    if (fread(h, 1, 8, a->file) != 8)
      goto invalid;
    a->count = u32(h);
    a->negated = 1;
    uint64_t offset = 8 + (uint64_t)a->count * 36;
    if (a->count > 100000 || offset + u32(h + 4) != (uint64_t)end)
      goto invalid;
    a->entries = calloc(a->count ? a->count : 1, sizeof(*a->entries));
    if (!a->entries)
      goto invalid;
    for (uint32_t i = 0; i < a->count; i++) {
      uint8_t name[32];
      if (fread(name, 1, 32, a->file) != 32)
        goto invalid;
      for (unsigned j = 0; j < 32; j++)
        a->entries[i].name[j] = (char)(uint8_t)(-name[j]);
    }
    for (uint32_t i = 0; i < a->count; i++) {
      uint8_t s[4];
      if (fread(s, 1, 4, a->file) != 4)
        goto invalid;
      a->entries[i].offset = (uint32_t)offset;
      a->entries[i].size = u32(s);
      offset += u32(s);
      if (offset > (uint64_t)end)
        goto invalid;
    }
    if (offset != (uint64_t)end)
      goto invalid;
  }
  for (uint32_t i = 0; i < a->count; i++) {
    BkEntry *r = a->entries + i;
    if (!r->name[0] || strchr(r->name, '/') || strchr(r->name, '\\') ||
        !strcmp(r->name, ".") || !strcmp(r->name, "..") ||
        (uint64_t)r->offset + r->size > (uint64_t)end)
      goto invalid;
    for (uint32_t j = 0; j < i; j++)
      if (!strcasecmp(r->name, a->entries[j].name))
        goto invalid;
  }
  return 1;
bad_table:
  free(table);
invalid:
  fail(e, "invalid archive/index bounds or record");
error:
  bk_archive_close(a);
  return 0;
}

const BkEntry *bk_archive_find(const BkArchive *a, const char *name) {
  for (uint32_t i = 0; i < a->count; i++)
    if (!strcasecmp(a->entries[i].name, name))
      return a->entries + i;
  return NULL;
}
int bk_archive_read(BkArchive *a, const BkEntry *r, uint8_t **out,
                    char e[256]) {
  *out = NULL;
  if (!r || r->size > 256U * 1024 * 1024)
    return fail(e, "resource missing or exceeds 256 MiB");
  uint8_t *b = malloc(r->size ? r->size : 1);
  if (!b)
    return fail(e, "resource allocation failed");
  if (fseek(a->file, r->offset, SEEK_SET) ||
      fread(b, 1, r->size, a->file) != r->size) {
    free(b);
    return fail(e, "short resource read");
  }
  if (a->negated)
    for (uint32_t i = 0; i < r->size; i++)
      b[i] = (uint8_t)(-b[i]);
  *out = b;
  return 1;
}

void bk_image_free(BkImage *im) {
  free(im->rgba);
  memset(im, 0, sizeof(*im));
}
static int image_alloc(BkImage *im, uint32_t w, uint32_t h, char *e) {
  if (!w || !h || w > 8192 || h > 8192 || (uint64_t)w * h > 32 * 1024 * 1024)
    return fail(e, "invalid image dimensions");
  im->width = w;
  im->height = h;
  im->rgba = malloc((size_t)w * h * 4);
  return im->rgba ? 1 : fail(e, "image allocation failed");
}
int bk_image_decode(const uint8_t *b, size_t n, BkImage *im, char e[256]) {
  memset(im, 0, sizeof(*im));
  if (n >= 54 && b[0] == 'B' && b[1] == 'M') {
    uint32_t dib = u32(b + 14), off = u32(b + 10), w = u32(b + 18),
             rawh = u32(b + 22), compression = u32(b + 30);
    int32_t sh;
    memcpy(&sh, &rawh, 4);
    if (sh == INT32_MIN || dib < 40 || (uint64_t)dib + 14 > n ||
        u16(b + 26) != 1 || compression)
      return fail(e, "unsupported BMP header/compression");
    uint32_t h = (uint32_t)(sh < 0 ? -sh : sh);
    unsigned depth = u16(b + 28);
    if (depth != 8 && depth != 24 && depth != 32)
      return fail(e, "unsupported BMP depth");
    size_t stride = (((size_t)w * depth + 31) / 32) * 4;
    uint32_t colors = depth == 8 ? (u32(b + 46) ? u32(b + 46) : 256) : 0;
    if (colors > 256 || (uint64_t)14 + dib + colors * 4 > off || off > n ||
        (h && stride > (n - off) / h))
      return fail(e, "truncated BMP pixels/palette");
    if (!image_alloc(im, w, h, e))
      return 0;
    for (uint32_t y = 0; y < h; y++)
      for (uint32_t x = 0; x < w; x++) {
        const uint8_t *p =
            b + off + (sh < 0 ? y : h - 1 - y) * stride + x * (depth / 8);
        if (depth == 8) {
          if (*p >= colors) {
            bk_image_free(im);
            return fail(e, "BMP palette index outside table");
          }
          p = b + 14 + dib + (*p) * 4;
        }
        uint8_t *d = im->rgba + ((size_t)y * w + x) * 4;
        d[0] = p[2];
        d[1] = p[1];
        d[2] = p[0];
        d[3] = 255; /* BI_RGB alpha reserved */
      }
    return 1;
  }
  if (n < 18 || b[1] != 0 || (b[2] != 2 && b[2] != 10) ||
      (b[16] != 24 && b[16] != 32) || (b[17] & 0xc0))
    return fail(e, "unsupported image format");
  uint32_t w = u16(b + 12), h = u16(b + 14);
  unsigned bytes = b[16] / 8;
  size_t p = 18 + b[0];
  if (p > n)
    return fail(e, "TGA image ID truncated");
  if (!image_alloc(im, w, h, e))
    return 0;
  size_t total = (size_t)w * h, i = 0;
  while (i < total) {
    unsigned count = 1, repeated = 0;
    if (b[2] == 10) {
      if (p >= n)
        goto truncated_image;
      unsigned tag = b[p++];
      count = (tag & 127) + 1;
      repeated = tag & 128;
    }
    if (count > total - i)
      goto truncated_image;
    const uint8_t *pixel = NULL;
    for (unsigned k = 0; k < count; k++, i++) {
      if (!repeated || k == 0) {
        if (bytes > n - p)
          goto truncated_image;
        pixel = b + p;
        p += bytes;
      }
      uint32_t x = (uint32_t)(i % w), y = (uint32_t)(i / w);
      if (!(b[17] & 32))
        y = h - 1 - y;
      if (b[17] & 16)
        x = w - 1 - x;
      uint8_t *d = im->rgba + ((size_t)y * w + x) * 4;
      d[0] = pixel[2];
      d[1] = pixel[1];
      d[2] = pixel[0];
      d[3] = bytes == 4 ? pixel[3] : 255;
    }
  }
  return 1;
truncated_image:
  bk_image_free(im);
  return fail(e, "TGA pixels truncated or packet exceeds image");
}
