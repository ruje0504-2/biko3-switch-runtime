#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "save/record_file.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef BK_TEST_NO_REPLACE
#undef rename
int rename(const char *, const char *);
static unsigned calls, fail_mask;
int bk_test_rename(const char *a, const char *b) {
  unsigned call = calls++;
  if (call < 32 && (fail_mask & (1u << call))) { errno = EIO; return -1; }
  struct stat st;
  if (!stat(b, &st)) { errno = EEXIST; return -1; }
  return rename(a, b);
}
static void reset(unsigned faults) { calls = 0; fail_mask = faults; }
#endif
/* Deliberately separate, noncontiguous lanes: save cannot dump a C struct. */
static void allocate(BkRecordView v[5]) {
  for (unsigned g = 0; g < 5; ++g) {
    v[g] = (BkRecordView){{calloc(10000,4),calloc(10000,4)},calloc(10000,4),calloc(1,4)};
    assert(v[g].retained[0] && v[g].retained[1] && v[g].actions && v[g].count);
  }
}
static void release(BkRecordView v[5]) {
  for (unsigned g = 0; g < 5; ++g) {
    free(v[g].retained[0]); free(v[g].retained[1]); free(v[g].actions); free(v[g].count);
  }
}
static uint32_t rng = 0x50caa2;
static uint32_t random_word(void) { rng = rng * 1664525u + 1013904223u; return rng; }
static void fill(BkRecordView v[5]) {
  for (unsigned g = 0; g < 5; ++g) {
    for (unsigned i = 0; i < 10000; ++i) {
      v[g].retained[0][i] = random_word(); v[g].retained[1][i] = random_word();
      uint32_t n = random_word(); memcpy(&v[g].actions[i], &n, 4);
    }
    const int32_t counts[] = {0, 10000, -1, INT32_MIN, INT32_MAX};
    *v[g].count = counts[g];
  }
}
static void same(const BkRecordView a[5], const BkRecordView b[5]) {
  for (unsigned g = 0; g < 5; ++g)
    assert(!memcmp(a[g].retained[0],b[g].retained[0],40000) &&
           !memcmp(a[g].retained[1],b[g].retained[1],40000) &&
           !memcmp(a[g].actions,b[g].actions,40000) && *a[g].count == *b[g].count);
}
static void write_bytes(const char *p, const void *data, size_t n) {
  FILE *f = fopen(p,"wb"); assert(f && fwrite(data,1,n,f)==n && !fclose(f));
}
int main(void) {
  char root[] = "/tmp/bk-records-XXXXXX", e[256], path[1100], part[1120], backup[1120];
  assert(mkdtemp(root));
  BkRecordFile *f = bk_record_file_create(root,e); assert(f);
  snprintf(path,sizeof(path),"%s/save/records.bkr",root);
  snprintf(part,sizeof(part),"%s.part",path); snprintf(backup,sizeof(backup),"%s.bak",path);
  BkRecordView a[5],b[5],old[5]; allocate(a);allocate(b);allocate(old);fill(a);fill(b);
  uint8_t *bytes=malloc(BK_RECORD_FILE_BYTES+1), *native=malloc(BK_RECORD_NATIVE_BYTES);
  uint8_t *saved=malloc(BK_RECORD_FILE_BYTES), *scratch=malloc(BK_RECORD_FILE_BYTES);
  assert(bytes && native && saved && scratch);
  assert(bk_record_encode(b,bytes,e) && bk_record_decode(old,bytes,BK_RECORD_FILE_BYTES,e));
  assert(bk_record_file_read(f,b,e)==BK_RESOURCE_MISSING); same(b,old);
  assert(bk_record_native_encode(a,native,e));
  assert(bk_record_native_decode(b,native,BK_RECORD_NATIVE_BYTES,e));same(a,b);
  assert(bk_record_encode(a,bytes,e)); memcpy(saved,bytes,BK_RECORD_FILE_BYTES);
  assert(bk_record_decode(b,bytes,BK_RECORD_FILE_BYTES,e));same(a,b);
  for (unsigned k=0; k<32+30; ++k) {
    unsigned off = k<32 ? k : 32 + (k-32)*20000;
    bytes[off]^=0x80; assert(!bk_record_decode(b,bytes,BK_RECORD_FILE_BYTES,e));same(a,b);
    bytes[off]^=0x80;
  }
  assert(!bk_record_decode(b,bytes,BK_RECORD_FILE_BYTES-1,e));same(a,b);
  assert(!bk_record_decode(b,bytes,BK_RECORD_FILE_BYTES+1,e));same(a,b);
  assert(!bk_record_native_decode(b,native,BK_RECORD_NATIVE_BYTES-1,e));same(a,b);
  BkRecordView invalid[5];memcpy(invalid,b,sizeof(invalid));invalid[4].count=NULL;
  assert(!bk_record_native_decode(invalid,native,BK_RECORD_NATIVE_BYTES,e));same(a,b);
  /* Encoded input and destinations may overlap. Every group/count survives. */
  BkRecordView alias[5];
  for(unsigned g=0;g<5;++g) {
    uint8_t *p=scratch+g*120004;
    alias[g]=(BkRecordView){{(uint32_t *)p,(uint32_t *)(p+40000)},(int32_t *)(p+80000),(int32_t *)(p+120000)};
  }
  memcpy(scratch,bytes,BK_RECORD_FILE_BYTES);
  assert(bk_record_decode(alias,scratch,BK_RECORD_FILE_BYTES,e));same(a,alias);
  assert(bk_record_encode(alias,scratch,e) && !memcmp(scratch,saved,BK_RECORD_FILE_BYTES));
  assert(bk_record_file_store(f,a,e));
  for(unsigned i=0;i<3;++i) {
    fill(a);
#ifdef BK_TEST_NO_REPLACE
    reset(0);
#endif
    assert(bk_record_file_store(f,a,e));
#ifdef BK_TEST_NO_REPLACE
    assert(calls==3);
#endif
    bk_record_file_destroy(f); f=bk_record_file_create(root,e); assert(f);
    assert(bk_record_file_read(f,b,e)==BK_RESOURCE_OK);same(a,b);
  }
  assert(bk_record_encode(a,saved,e));
  memcpy(bytes,saved,BK_RECORD_FILE_BYTES);bytes[35]^=1;
  write_bytes(path,bytes,BK_RECORD_FILE_BYTES);
  assert(bk_record_file_read(f,b,e)==BK_RESOURCE_ERROR && !bk_record_file_store(f,a,e));same(a,b);
  write_bytes(path,saved,BK_RECORD_FILE_BYTES-1);
  assert(bk_record_file_read(f,b,e)==BK_RESOURCE_ERROR);
  memcpy(bytes,saved,BK_RECORD_FILE_BYTES);bytes[BK_RECORD_FILE_BYTES]=0;
  write_bytes(path,bytes,BK_RECORD_FILE_BYTES+1);
  assert(bk_record_file_read(f,b,e)==BK_RESOURCE_ERROR);same(a,b);
  write_bytes(path,saved,BK_RECORD_FILE_BYTES);
  assert(!mkdir(part,0777));assert(!bk_record_file_store(f,old,e));assert(!rmdir(part));
  assert(bk_record_file_read(f,b,e)==BK_RESOURCE_OK);same(a,b);
#ifdef BK_TEST_NO_REPLACE
  const unsigned faults[]={1u,2u,4u,4u|8u};
  for(unsigned i=0;i<4;++i) {
    reset(faults[i]);assert(!bk_record_file_store(f,old,e));reset(0);
    bk_record_file_destroy(f);f=bk_record_file_create(root,e);assert(f);
    assert(bk_record_file_read(f,b,e)==BK_RESOURCE_OK);same(a,b);
  }
#endif
  assert(!rename(path,backup));
  assert(bk_record_file_read(f,b,e)==BK_RESOURCE_OK);same(a,b);
  bk_record_file_destroy(f);assert(!remove(path));
  snprintf(path,sizeof(path),"%s/save",root);assert(!rmdir(path)&&!rmdir(root));
  release(a);release(b);release(old);free(bytes);free(native);free(saved);free(scratch);
  puts("PASS records:600020 bytes,all words/lanes/counts,62 corruptions,overlap,missing,size,3 restarts,failed write/recovery");
#ifdef BK_TEST_NO_REPLACE
  puts("PASS records Switch no-overwrite FS:4 injected rename failures/restart");
#endif
}
