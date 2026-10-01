#ifndef BK_APP_ALBUM_SESSION_H
#define BK_APP_ALBUM_SESSION_H
#include "scene/scene.h"
#include "scene/album_menu.h"
typedef struct BkAlbumSession BkAlbumSession;
typedef struct {
  BkSceneServices services;
  BkAlbumMenu *menu; /* Front-end process state; initialized once. */
  BkViewport viewport;
  float pointer[2];
  double wall_seconds;
  void *context;
  int (*sound)(void *,unsigned,char[256]);
  int (*playing)(void *,unsigned,int *,char[256]);
  int (*schedule)(void *,uint8_t,uint8_t,char[256]);
} BkAlbumSessionConfig;
BkAlbumSession *bk_album_session_create(const BkAlbumSessionConfig *,char[256]);
void bk_album_session_destroy(BkAlbumSession *);
int bk_album_session_stop(BkAlbumSession *,char[256]);
int bk_album_session_step(BkAlbumSession *,double seconds,double wall_seconds,
                           const BkInput *,char[256]);
int bk_album_session_draw(BkAlbumSession *,char[256]);
const BkVirtualPointer *bk_album_session_pointer(const BkAlbumSession *);
#endif
