#ifndef BK_SCENE_ENDING_GALLERY_EFFECT_H
#define BK_SCENE_ENDING_GALLERY_EFFECT_H
#include "game/ending_gallery_effect.h"
#include "scene/ending_audio.h"
#include "world/actor_pose.h"
/*Borrow the current actor/audio owners and the process RNG. The state in
 *bindings outlives resource reloads. This call never advances the actor.*/
int bk_ending_gallery_effect_apply(BkActorPose *, BkEndingAudio *, uint32_t *,
    const BkEndingGalleryEffectBindings *, float seconds, char error[256]);
#endif
