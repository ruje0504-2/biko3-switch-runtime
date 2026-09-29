#ifndef BK_SCENE_ENDING_BACKGROUND_ASSETS_H
#define BK_SCENE_ENDING_BACKGROUND_ASSETS_H
#include "model/material_animation.h"
#include "model/morph_group.h"
#include "resource/store.h"
#include "world/actor_pose.h"

typedef struct BkEndingBackgroundAssets BkEndingBackgroundAssets;
/* Borrowed resource pointers. Only the enclosing owner releases them. The
 * same background survives a stage replacement and the previous GPU frame. */
typedef struct {
  BkModel *model;
  BkClipSet *clips;
  BkActorPose *pose;
  BkMaterialPose *materials;
  BkMaterialAnimation *animation;
  BkMorphGroup *morph;
  uint32_t root;
} BkEndingBackgroundData;

/* Decode an actual bk3_03 actor, without global attachment, orientation or
 * clip selection. Those operations belong to the original loader boundary. */
BkEndingBackgroundAssets *bk_ending_background_assets_create(
    BkResourceStore *, const char *name, char error[256]);
BkEndingBackgroundAssets *bk_ending_background_assets_retain(
    BkEndingBackgroundAssets *);
void bk_ending_background_assets_destroy(BkEndingBackgroundAssets *);
const BkEndingBackgroundData *bk_ending_background_assets_data(
    const BkEndingBackgroundAssets *);
const char *bk_ending_background_assets_name(const BkEndingBackgroundAssets *);
/*4026fe ordering, including hidden-clock retention and disjoint model
 * effects. No world publication, audio command, or material copy. */
int bk_ending_background_assets_advance(BkEndingBackgroundAssets *, float seconds,
                                        char error[256]);
#endif
