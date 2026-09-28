#ifndef BK_SCENE_PROP_AUDIO_H
#define BK_SCENE_PROP_AUDIO_H
#include "game/prop_sound.h"
#include "media/audio.h"
#include "scene/prop_assets.h"
typedef struct BkPropAudio BkPropAudio;
/* Preloads bk3_02 effects, initializes per-prop buffers, starts authored loops.
 * Borrows mixer; owns clips; reserves [first_voice, first_voice+prop_count).
 * Fresh sound stages are zero, retained[] restores caller-owned original
 * +450 values on entry reload. Stop before destroy; no device poll/fill. */
BkPropAudio *bk_prop_audio_create(BkResourceStore *, BkAudio *,
                                  unsigned first_voice, const BkPropAssets *,
                                  const BkPropSoundState *retained,
                                  size_t retained_count, int32_t effect_volume,
                                  char error[256]);
void bk_prop_audio_destroy(BkPropAudio *);
int bk_prop_audio_stop(BkPropAudio *, char error[256]);
const BkPropSoundState *bk_prop_audio_state(const BkPropAudio *,
                                            uint32_t index);
int bk_prop_audio_set_stage(BkPropAudio *, uint32_t index, int8_t stage);
/* Complete4f4306 after prop display: borrows the existing motion/contact and
 * sound owners, commits CPU policy then applies ordered cursor-preserving
 * Play/Stop. No renderer/pose publication, sink poll/fill, or second copy of
 * actor state. Backend failure after CPU commit terminates the frame. */
int bk_prop_audio_interact(BkPropAudio *, BkPropAssets *, BkPlayerControl *,
                           int8_t *npc_stimulus, uint8_t *outcome,
                           BkPropInteractionShared *,
                           const BkPropInteractionInput *,
                           BkPropInteractionCommands *, char error[256]);
/*5121ce all spatial updates with512c0e consumed immediately per prop.
 * Listener is the current player, not camera. Playback status comes from
 * actual mixer, input voice flags are ignored. Presentation follows this. */
int bk_prop_audio_step(BkPropAudio *, BkPropAssets *, BkPropShared *,
                       const BkPropMotionInput *, const BkCollision *,
                       const BkNpcSceneInput *, size_t ground_count,
                       BkPropMotionEffects effects[16],
                       const BkPropSoundInput *, char error[256]);
/*51b244 first-arrival one-shot: keep the loaded clip/gain/frequency, rewind0.
 */
int bk_prop_audio_restart(BkPropAudio *, uint32_t index, char error[256]);
#endif
