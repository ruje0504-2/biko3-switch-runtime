#include "media/avi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  size_t offset, size;
  uint32_t key;
} Packet;
struct BkAvi {
  BkAviInfo info;
  uint8_t *bytes;
  Packet *packets;
};
struct BkAviDecoder {
  const BkAvi *avi;
  uint16_t *pixels, *scratch;
  uint32_t frame;
};
typedef struct {
  const uint8_t *tag, *data;
  size_t offset, size;
} Chunk;
static uint16_t u16(const uint8_t *p) {
  return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}
static uint32_t u32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static int fail(char error[256], const char *why) {
  snprintf(error, 256, "AVI MSV1: %s", why);
  return 0;
}
static int tag(const uint8_t *p, const char *s) { return !memcmp(p, s, 4); }
static int next(const uint8_t *b, size_t *at, size_t end, Chunk *c,
                char error[256]) {
  if (*at > end || end - *at < 8)
    return fail(error, "truncated chunk header");
  c->offset = *at;
  c->tag = b + *at;
  c->size = u32(b + *at + 4);
  *at += 8;
  c->data = b + *at;
  if (c->size > end - *at || ((c->size & 1) && c->size == end - *at))
    return fail(error, "truncated chunk/padding");
  *at += c->size + (c->size & 1);
  if (tag(c->tag, "LIST") && c->size < 4)
    return fail(error, "truncated list type");
  return 1;
}
static int header(const uint8_t *b, const Chunk *hdrl, BkAviInfo *info,
                  char error[256]) {
  Chunk main = {0}, stream = {0}, sh = {0}, sf = {0}, c;
  size_t end = hdrl->offset + 8 + hdrl->size;
  for (size_t at = hdrl->offset + 12; at < end;) {
    if (!next(b, &at, end, &c, error))
      return 0;
    if (tag(c.tag, "avih")) {
      if (main.data)
        return fail(error, "duplicate avih");
      main = c;
    } else if (tag(c.tag, "LIST")) {
      if (!tag(c.data, "strl") || stream.data)
        return fail(error, "unsupported/duplicate stream list");
      stream = c;
    }
  }
  if (!main.data || main.size != 56 || !stream.data ||
      u32(main.data + 24) != 1 || u32(main.data + 20))
    return fail(error, "requires one non-interleaved video stream");
  end = stream.offset + 8 + stream.size;
  for (size_t at = stream.offset + 12; at < end;) {
    if (!next(b, &at, end, &c, error))
      return 0;
    if (tag(c.tag, "strh")) {
      if (sh.data)
        return fail(error, "duplicate strh");
      sh = c;
    } else if (tag(c.tag, "strf")) {
      if (sf.data)
        return fail(error, "duplicate strf");
      sf = c;
    } else if (tag(c.tag, "indx") || tag(c.tag, "LIST"))
      return fail(error, "OpenDML/nested stream metadata is unsupported");
  }
  if (!sh.data || sh.size != 56 || !sf.data || sf.size != 40 ||
      !tag(sh.data, "vids") || !tag(sh.data + 4, "msvc") || u32(sh.data + 16) ||
      u32(sh.data + 28) || u32(sh.data + 44) || u32(sf.data) != 40 ||
      u16(sf.data + 12) != 1 || u16(sf.data + 14) != 16 ||
      !tag(sf.data + 16, "CRAM") || u32(sf.data + 32) || u32(sf.data + 36))
    return fail(error, "requires start0 MS Video 1 / CRAM RGB555");
  *info = (BkAviInfo){u32(sf.data + 4), u32(sf.data + 8), u32(sh.data + 32),
                      u32(sh.data + 20), u32(sh.data + 24)};
  if (!info->width || info->width > 2048 || info->width % 4 || !info->height ||
      info->height > 2048 || info->height % 4 || !info->frames ||
      info->frames > 65536 || !info->scale || info->scale > INT32_MAX ||
      !info->rate || info->frames != u32(main.data + 16) ||
      info->width != u32(main.data + 32) || info->height != u32(main.data + 36))
    return fail(error,
                "invalid dimensions/frame count/rate or header mismatch");
  return 1;
}
static int movie(BkAvi *a, size_t begin, size_t end, unsigned depth,
                 uint32_t *count, char error[256]) {
  Chunk c;
  if (depth > 8)
    return fail(error, "record nesting exceeds limit");
  for (size_t at = begin; at < end;) {
    if (!next(a->bytes, &at, end, &c, error))
      return 0;
    if (tag(c.tag, "LIST") && tag(c.data, "rec ")) {
      if (!movie(a, c.offset + 12, c.offset + 8 + c.size, depth + 1, count,
                 error))
        return 0;
    } else if (tag(c.tag, "00db") || tag(c.tag, "00dc")) {
      /* AVI's db suffix does NOT override this stream's CRAM codec. */
      if (*count >= a->info.frames || !c.size)
        return fail(error, "excess/empty video packet");
      a->packets[(*count)++] = (Packet){c.offset + 8, c.size, 0};
    } else if (!tag(c.tag, "JUNK"))
      return fail(error, "unsupported movie stream/chunk");
  }
  return 1;
}
BkAvi *bk_avi_open(const void *bytes, size_t size, char error[256]) {
  const uint8_t *b = bytes;
  BkAvi *a = NULL;
  Chunk hdrl = {0}, movi = {0}, index = {0}, c;
  if (!b || size < 12 || !tag(b, "RIFF") || !tag(b + 8, "AVI ") ||
      u32(b + 4) < 4 || u32(b + 4) > 256 * 1024 * 1024 - 8 ||
      (uint64_t)u32(b + 4) + 8 > size) {
    fail(error, "invalid RIFF extent/type");
    return NULL;
  }
  size_t extent = (size_t)u32(b + 4) + 8;
  for (size_t at = 12; at < extent;) {
    if (!next(b, &at, extent, &c, error))
      return NULL;
    Chunk *dst = NULL;
    if (tag(c.tag, "LIST")) {
      if (tag(c.data, "hdrl"))
        dst = &hdrl;
      else if (tag(c.data, "movi"))
        dst = &movi;
      else {
        fail(error, "unsupported top-level list");
        return NULL;
      }
    } else if (tag(c.tag, "idx1"))
      dst = &index;
    if (dst) {
      if (dst->data) {
        fail(error, "duplicate header/movie/index");
        return NULL;
      }
      *dst = c;
    }
  }
  if (!hdrl.data || !movi.data || !index.data) {
    fail(error, "missing header/movie/legacy index");
    return NULL;
  }
  BkAviInfo info;
  if (!header(b, &hdrl, &info, error))
    return NULL;
  if (index.size != (size_t)info.frames * 16) {
    fail(error, "index/frame count mismatch");
    return NULL;
  }
  a = calloc(1, sizeof(*a));
  if (!a)
    goto allocation;
  a->info = info;
  a->bytes = malloc(extent);
  a->packets = calloc(info.frames, sizeof(*a->packets));
  if (!a->bytes || !a->packets)
    goto allocation;
  memcpy(a->bytes, b, extent);
  uint32_t count = 0, key = 0;
  if (!movie(a, movi.offset + 12, movi.offset + 8 + movi.size, 0, &count,
             error))
    goto bad;
  if (count != info.frames) {
    fail(error, "movie/frame count mismatch");
    goto bad;
  }
  for (uint32_t i = 0; i < count; i++) {
    const uint8_t *ix = index.data + i * 16;
    Packet *p = &a->packets[i];
    if (memcmp(ix, b + p->offset - 8, 4) || (u32(ix + 4) & ~16u) ||
        (!i && !(u32(ix + 4) & 16)) ||
        (uint64_t)u32(ix + 8) + movi.offset + 8 != p->offset - 8 ||
        u32(ix + 12) != p->size) {
      fail(error, "invalid index entry/key/offset/size");
      goto bad;
    }
    if (u32(ix + 4) & 16)
      key = i;
    p->key = key;
  }
  return a;
allocation:
  fail(error, "allocation failed");
bad:
  bk_avi_destroy(a);
  return NULL;
}
void bk_avi_destroy(BkAvi *a) {
  if (a) {
    free(a->bytes);
    free(a->packets);
    free(a);
  }
}
const BkAviInfo *bk_avi_info(const BkAvi *a) { return a ? &a->info : NULL; }
BkAviDecoder *bk_avi_decoder_create(const BkAvi *a, char error[256]) {
  BkAviDecoder *d = NULL;
  if (!a) {
    fail(error, "missing AVI");
    return NULL;
  }
  d = calloc(1, sizeof(*d));
  if (!d)
    goto bad;
  d->avi = a;
  d->frame = UINT32_MAX;
  size_t size = (size_t)a->info.width * a->info.height * sizeof(uint16_t);
  d->pixels = malloc(size);
  d->scratch = malloc(size);
  if (!d->pixels || !d->scratch)
    goto bad;
  return d;
bad:
  fail(error, "decoder allocation failed");
  bk_avi_decoder_destroy(d);
  return NULL;
}
void bk_avi_decoder_destroy(BkAviDecoder *d) {
  if (d) {
    free(d->pixels);
    free(d->scratch);
    free(d);
  }
}
/* Block syntax checked against FFmpeg's MS Video 1 decoder (see THIRD_PARTY).
 * This bounds-checked packet walker writes an uncommitted RGB555 image. */
static int decode(const BkAvi *a, uint32_t frame, uint16_t *image,
                  char error[256]) {
  const Packet *p = &a->packets[frame];
  const uint8_t *b = a->bytes + p->offset;
  uint32_t wide = a->info.width / 4;
  uint32_t blocks = wide * (a->info.height / 4);
  size_t at = 0;
  for (uint32_t block = 0; block < blocks;) {
    if (p->size - at < 2)
      return fail(error, "truncated block opcode");
    uint16_t opcode = u16(b + at);
    at += 2;
    if ((opcode & 0xfc00) == 0x8400) {
      uint32_t run = opcode & 0x3ff;
      if (!run || run > blocks - block || p->key == frame)
        return fail(error, "invalid skip run or dependent indexed key frame");
      block += run;
      continue;
    }
    uint16_t palette[8];
    unsigned colors = 1;
    if (opcode < 0x8000) {
      if (p->size - at < 4)
        return fail(error, "truncated two-color block");
      colors = (u16(b + at) & 0x8000) ? 8 : 2;
      if (p->size - at < colors * 2)
        return fail(error, "truncated eight-color block");
      for (unsigned i = 0; i < colors; i++)
        palette[i] = u16(b + at + 2 * i) & 0x7fff;
      at += colors * 2;
    } else
      palette[0] = opcode & 0x7fff;
    uint32_t left = (block % wide) * 4;
    uint32_t bottom = a->info.height - 1 - (block / wide) * 4;
    for (unsigned pixel = 0; pixel < 16; pixel++) {
      unsigned x = pixel % 4, y = pixel / 4, color = 0;
      if (colors != 1) {
        color = ((opcode >> pixel) & 1) ^ 1;
        if (colors == 8)
          color += (y / 2) * 4 + (x / 2) * 2;
      }
      image[(size_t)(bottom - y) * a->info.width + left + x] = palette[color];
    }
    ++block;
  }
  if (at != p->size && (p->size - at != 2 || u16(b + at)))
    return fail(error, "unexpected packet tail");
  return 1;
}
int bk_avi_decoder_frame(BkAviDecoder *d, uint32_t frame, char error[256]) {
  if (!d || frame >= d->avi->info.frames)
    return fail(error, "frame index out of range");
  if (d->frame == frame)
    return 1;
  const BkAvi *a = d->avi;
  size_t size = (size_t)a->info.width * a->info.height * sizeof(uint16_t);
  uint32_t start = a->packets[frame].key;
  if (d->frame != UINT32_MAX && d->frame >= start && d->frame < frame) {
    memcpy(d->scratch, d->pixels, size);
    start = d->frame + 1;
  }
  /* A key frame must overwrite every pixel: decode rejects any skip. */
  for (uint32_t i = start; i <= frame; i++)
    if (!decode(a, i, d->scratch, error))
      return 0;
  uint16_t *swap = d->pixels;
  d->pixels = d->scratch;
  d->scratch = swap;
  d->frame = frame;
  return 1;
}
const uint16_t *bk_avi_decoder_pixels(const BkAviDecoder *d) {
  return d && d->frame != UINT32_MAX ? d->pixels : NULL;
}
uint32_t bk_avi_decoder_index(const BkAviDecoder *d) {
  return d ? d->frame : UINT32_MAX;
}
