#define _POSIX_C_SOURCE 200809L
#include "save/volume_file.h"
#include "save/file_replace_internal.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct BkVolumeFile { int32_t values[3]; char path[1100]; };
static int fail(char e[256],const char *why) {snprintf(e,256,"volume file: %s",why);return 0;}
static int valid(const int32_t *v) {
  if (!v) return 0;
  for(unsigned i=0;i<3;++i) if(v[i]<-6000 || v[i]>0) return 0;
  return 1;
}
static int directory(const char *path,char e[256]) {
  struct stat st;
  if(!stat(path,&st)) return S_ISDIR(st.st_mode) || fail(e,"output is not a directory");
  return (errno==ENOENT && !mkdir(path,0777)) || fail(e,"cannot create output directory");
}
/* 1 loaded,0 absent,-1 invalid/unreadable. */
static int read_values(const char *path,int32_t values[3],char e[256]) {
  FILE *f=fopen(path,"rb");
  if(!f) {if(errno==ENOENT)return 0;fail(e,"cannot read settings");return -1;}
  uint8_t bytes[12];int32_t next[3];
  int ok=fread(bytes,1,12,f)==12;
  if(fgetc(f)!=EOF || ferror(f))ok=0;
  if(fclose(f))ok=0;
  if(!ok) {fail(e,"settings must contain exactly12 bytes");return -1;}
  for(unsigned i=0;i<3;++i) {
    uint32_t raw=0;
    for(unsigned j=0;j<4;++j) raw|=(uint32_t)bytes[i*4+j]<<(j*8);
    next[i]=raw>INT32_MAX?(int32_t)((int64_t)raw-INT64_C(4294967296)):(int32_t)raw;
  }
  if(!valid(next)) {fail(e,"settings outside original slider range");return -1;}
  memcpy(values,next,sizeof(next));return 1;
}
BkVolumeFile *bk_volume_file_create(const char *root,const char *game,char e[256]) {
  if((root && (!*root || strlen(root)>980)) || (game && (!*game || strlen(game)>980))) {
    fail(e,"invalid root");return NULL;
  }
  BkVolumeFile *s=calloc(1,sizeof(*s));
  if(!s) {fail(e,"allocation failed");return NULL;}
  int read=0;char path[1100];
  if(root) {
    snprintf(path,sizeof(path),"%s/save",root);
    if(!directory(root,e) || !directory(path,e))goto bad;
    snprintf(s->path,sizeof(s->path),"%s/save/volume.cfg",root);
    if(!bk_save_file_recover(s->path,e))goto bad;
    read=read_values(s->path,s->values,e);if(read<0)goto bad;
  }
  if(!read && game) {
    snprintf(path,sizeof(path),"%s/Data/volsetting.cfg",game);
    if(read_values(path,s->values,e)<0)goto bad;
  }
  return s;
bad:free(s);return NULL;
}
void bk_volume_file_destroy(BkVolumeFile *s) {free(s);}
const int32_t *bk_volume_file_values(const BkVolumeFile *s) {return s?s->values:NULL;}
int bk_volume_file_store(BkVolumeFile *s,const int32_t values[3],char e[256]) {
  if(!s || !s->path[0] || !valid(values))return fail(e,"invalid values or no writable port root");
  int32_t next[3];memcpy(next,values,sizeof(next));
  uint8_t bytes[12];
  for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)bytes[i*4+j]=(uint8_t)((uint32_t)next[i]>>(j*8));
  char tmp[1120];snprintf(tmp,sizeof(tmp),"%s.part",s->path);
  FILE *f=fopen(tmp,"wb");if(!f)return fail(e,"cannot open temporary settings");
  int ok=fwrite(bytes,1,12,f)==12;
  if(ok && fflush(f))ok=0;
  if(ok && fsync(fileno(f)))ok=0;
  if(fclose(f))ok=0;
  if(!ok) {remove(tmp);return fail(e,"write/flush/close failed");}
  if(!bk_save_file_replace(tmp,s->path,e)) {remove(tmp);return 0;}
  memcpy(s->values,next,sizeof(next));return 1;
}
