#ifndef BK_GAME_AREA_ENTRY_H
#define BK_GAME_AREA_ENTRY_H
#include "game/actor_entry.h"
#include "game/camera_policy.h"
/* Represented actor/controller stores in4e94e4, AFTER area++ and next CKP
 * loading. Retain actors, animations/materials, inventory, actor action tables,
 * route start/end metadata, movement interpolation, AI clocks and face state.
 * The enclosing scene must load the next route/background/props/items/track
 * in original order. Actor model matrices stay held until their next normal
 * spatial/presentation stages;4befc0 itself only writes CPU body fields. This
 * is not a fresh entry reset or a complete resource transition. Outputs are
 * atomic. */
int bk_area_entry_reset(BkPlayerControl *, BkNpcSpatialState *,
                        float *npc_vertical, BkPlayerView *,
                        BkGameCameraState *, const BkNpcEntryRoute *,
                        const BkNpcFootstepActions *, const BkRoute *,
                        const BkEntryRequest *next, char error[256]);
#endif
