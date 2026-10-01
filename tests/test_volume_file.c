#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "save/volume_file.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void write_values(const char *path,const int32_t values[3]) {
  uint8_t bytes[12];for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)bytes[i*4+j]=(uint8_t)((uint32_t)values[i]>>(j*8));
  FILE *f=fopen(path,"wb");assert(f && fwrite(bytes,1,12,f)==12 && !fclose(f));
}
static void same(const BkVolumeFile *f,const int32_t v[3]) {assert(f && !memcmp(bk_volume_file_values(f),v,12));}
int main(void) {
  char e[256],root[]="/tmp/bk-volume-XXXXXX",game[1100],data[1100],port[1100],save[1100],path[1200],original[1200],part[1220];
  assert(mkdtemp(root));snprintf(game,sizeof(game),"%s/game",root);snprintf(data,sizeof(data),"%s/game/Data",root);snprintf(port,sizeof(port),"%s/port",root);snprintf(save,sizeof(save),"%s/port/save",root);
  assert(!mkdir(game,0777) && !mkdir(data,0777));
  snprintf(original,sizeof(original),"%s/volsetting.cfg",data);snprintf(path,sizeof(path),"%s/volume.cfg",save);snprintf(part,sizeof(part),"%s.part",path);
  const int32_t zero[]={0,0,0},old[]={-1500,-2500,-1999},next[]={-5999,-4137,-27};
  BkVolumeFile *f=bk_volume_file_create(NULL,e);same(f,zero);assert(!bk_volume_file_store(f,next,e));same(f,zero);bk_volume_file_destroy(f);
  write_values(original,old);
  f=bk_volume_file_create(port,e);same(f,zero);const int32_t *stable=bk_volume_file_values(f);
  assert(!bk_volume_file_store(f,(int32_t[]){-6001,0,0},e));same(f,zero);
  assert(bk_volume_file_store(f,next,e));same(f,next);assert(stable==bk_volume_file_values(f));
  BkVolumeFile *read=bk_volume_file_create(port,e);same(read,next);bk_volume_file_destroy(read);
  read=bk_volume_file_create(NULL,e);same(read,zero);bk_volume_file_destroy(read);
  assert(!mkdir(part,0777));assert(!bk_volume_file_store(f,old,e));same(f,next);assert(!rmdir(part));
  read=bk_volume_file_create(port,e);same(read,next);bk_volume_file_destroy(read);
  assert(bk_volume_file_store(f,old,e));same(f,old);bk_volume_file_destroy(f);
  for(unsigned n=0;n<14;++n) {
    if(n==12)continue;
    FILE *out=fopen(path,"wb");assert(out);for(unsigned i=0;i<n;++i)assert(fputc(0,out)!=EOF);assert(!fclose(out));
    assert(!bk_volume_file_create(port,e)); /* no corrupt-port fallback */
  }
  write_values(path,(int32_t[]){1,-500,-300});assert(!bk_volume_file_create(port,e));
  assert(!remove(path));f=bk_volume_file_create(port,e);same(f,zero);bk_volume_file_destroy(f);
  uint8_t expected[12],unchanged[12];
  for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)expected[i*4+j]=(uint8_t)((uint32_t)old[i]>>(j*8));
  FILE *source=fopen(original,"rb");assert(source && fread(unchanged,1,12,source)==12 && !fclose(source));
  assert(!memcmp(expected,unchanged,12));
  assert(!remove(original) && !rmdir(save) && !rmdir(port) && !rmdir(data) && !rmdir(game) && !rmdir(root));
  puts("volume file PASS MAX defaults/independent settings/readonly source/reload/failure/length/range");return 0;
}
