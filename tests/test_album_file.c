#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "save/capture_file.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void) {
  char root[]="/tmp/bk-album-XXXXXX",e[256],name[256],path[512];
  assert(mkdtemp(root));BkCaptureFiles *f=bk_capture_files_create(root,e);assert(f);
  const char *prefix[]={"ri_","re_","cr_","ma_","mi_"};
  unsigned reads=0,removed=0;
  for(unsigned g=0;g<5;++g)for(unsigned i=0;i<137;++i) {
    snprintf(name,sizeof(name),"%s%03u.%s",prefix[g],i,i%2?"BMP":"bmp");
    uint8_t data[]={(uint8_t)g,(uint8_t)i,0xff};BkBlob b={data,sizeof(data)};
    assert(bk_capture_file_write(f,1,name,&b,e));
  }
  for(unsigned g=0;g<5;++g) {
    BkPhotoList frozen={0},live={0};
    assert(bk_capture_files_list_photos(f,g,g==4?101:100,&frozen,e));
    assert(bk_capture_files_list_photos(f,g,INT32_MAX,&live,e));
    assert(frozen.count==(g==4?101:100) && live.count==137);
    for(size_t i=0;i<frozen.count;++i)assert(!strcmp(frozen.names[i],live.names[i]));
    char deleted[256];strcpy(deleted,frozen.names[50]);
    BkBlob b={0};assert(bk_capture_file_read_photo(f,deleted,3,&b,e)==BK_RESOURCE_OK);
    assert(b.size==3 && b.data[0]==g);bk_blob_free(&b);++reads;
    assert(bk_capture_file_read_photo(f,deleted,2,&b,e)==BK_RESOURCE_ERROR && !b.data && !b.size);
    assert(bk_capture_file_remove_photo(f,deleted,e));++removed;
    assert(!strcmp(deleted,frozen.names[50]));
    assert(bk_capture_file_read_photo(f,deleted,3,&b,e)==BK_RESOURCE_MISSING && !b.data);
    assert(bk_capture_file_read_photo(f,frozen.names[51],3,&b,e)==BK_RESOURCE_OK);bk_blob_free(&b);++reads;
    bk_photo_list_free(&live);
    assert(bk_capture_files_list_photos(f,g,INT32_MAX,&live,e) && live.count==136);
    for(size_t i=0;i<live.count;++i) {
      assert(strcmp(live.names[i],deleted));assert(bk_capture_file_remove_photo(f,live.names[i],e));++removed;
    }
    assert(bk_capture_file_remove_photo(f,deleted,e));
    bk_photo_list_free(&live);bk_photo_list_free(&frozen);
  }
  /* External photos may have longer names than the game's generated stamp. */
  memset(name,'x',250);memcpy(name,"ri_",3);memcpy(name+250,".bmp",5);
  snprintf(path,sizeof(path),"%s/album/%s",root,name);FILE *file=fopen(path,"wb");assert(file);
  assert(fwrite("photo",1,5,file)==5 && !fclose(file));
  BkPhotoList list={0};assert(bk_capture_files_list_photos(f,0,100,&list,e));
  assert(list.count==1 && !strcmp(list.names[0],name));
  BkBlob b={0};assert(bk_capture_file_read_photo(f,name,5,&b,e)==BK_RESOURCE_OK && b.size==5);bk_blob_free(&b);++reads;
  assert(bk_capture_file_remove_photo(f,name,e));++removed;bk_photo_list_free(&list);
  assert(!bk_capture_file_remove_photo(f,"../outside.bmp",e));
  assert(!bk_capture_file_remove_photo(f,"ri_../outside.bmp",e));
  assert(!bk_capture_file_remove_photo(f,"sy_99.bmp",e));
  assert(bk_capture_file_read_photo(f,"../outside.bmp",10,&b,e)==BK_RESOURCE_ERROR);
  assert(!bk_capture_files_list_photos(f,5,100,&list,e));
  snprintf(path,sizeof(path),"%s/album/ri_dir.bmp",root);assert(!mkdir(path,0700));
  assert(!bk_capture_file_remove_photo(f,"ri_dir.bmp",e));assert(!rmdir(path));
  snprintf(path,sizeof(path),"%s/album",root);assert(!rmdir(path));
  assert(bk_capture_files_list_photos(f,0,100,&list,e) && !list.count && !list.names);
  bk_capture_files_destroy(f);assert(!rmdir(root));
  printf("PASS album-files 5groups frozen100/101 live137, reads%u deletes%u, no index shift, bounded reads and owned basenames\n",reads,removed);
}
