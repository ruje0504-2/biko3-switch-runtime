#ifndef BK_SCENE_OUTCOME_AUDIO_H
#define BK_SCENE_OUTCOME_AUDIO_H
#include "media/audio.h"
typedef struct BkOutcomeAudio BkOutcomeAudio;
/*4e82b8 se100(B537D8)/se007(B53558). Distinct voices, captured MUSIC volume
 * BE9A0C, not effects volume. Own clips, borrow mixer. create does not play. */
BkOutcomeAudio *bk_outcome_audio_create(BkResourceStore *, BkAudio *,
                                        unsigned outcome_voice,
                                        unsigned response_voice,
                                        int32_t music_volume, char error[256]);
void bk_outcome_audio_destroy(BkOutcomeAudio *);
int bk_outcome_audio_stop(BkOutcomeAudio *, char error[256]);
/*DirectSound Play(0,0,0): repeat while playing does not rewind; a naturally
 * ended buffer resumes from0. These match BkCommonHudOps callback signatures.
 */
int bk_outcome_audio_play_outcome(void *, char error[256]);
int bk_outcome_audio_play_response(void *, char error[256]);
#endif
