#ifndef BK_SCENE_CONTROL_HELP_H
#define BK_SCENE_CONTROL_HELP_H
#include "render/renderer.h"
typedef enum {
  BK_HELP_NONE, BK_HELP_TITLE, BK_HELP_SELECTION, BK_HELP_DIALOGUE,
  BK_HELP_GAME, BK_HELP_PAUSE, BK_HELP_CHOICE, BK_HELP_SAVE,
  BK_HELP_GALLERY, BK_HELP_VOLUME, BK_HELP_ENDING, BK_HELP_SPECIAL,
  BK_HELP_LOADING, BK_HELP_FAILURE, BK_HELP_INSPECTION, BK_HELP_COUNT
} BkControlHelpPage;
typedef struct BkControlHelp BkControlHelp;
/* One immutable font atlas and prebuilt meshes. Draw after scene captures;
 * only the left 4:3 pillarbox is writable. No per-frame uploads or allocation. */
BkControlHelp *bk_control_help_create(BkRenderer *, char error[256]);
int bk_control_help_draw(BkControlHelp *, BkControlHelpPage, char error[256]);
void bk_control_help_destroy(BkControlHelp *);
#endif
