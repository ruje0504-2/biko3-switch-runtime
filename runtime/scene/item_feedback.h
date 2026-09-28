#ifndef BK_SCENE_ITEM_FEEDBACK_H
#define BK_SCENE_ITEM_FEEDBACK_H
#include "game/item.h"
#include "media/audio.h"
#include "resource/dialogue.h"
#include "resource/message.h"
typedef struct BkItemFeedback BkItemFeedback;
/* Normal item service load: owns bk3_05/i00_00.txt and bk3_02/se101.wav,
 * borrows mixer, exclusively controls an explicit voice. Volume is captured
 * at load time, as in4ec9f0. Keep this instance when native load suppression
 * retains the previous sound. Includes retained51765d metadata, but not the
 * font or notice policy. Creation loads only; no voice commands. Destroy
 * releases owned resources; stop explicitly before releasing the mixer (queued
 * PCM is retained).
 */
BkItemFeedback *bk_item_feedback_create(BkResourceStore *, BkAudio *,
                                        unsigned voice, int32_t volume,
                                        char error[256]);
void bk_item_feedback_destroy(BkItemFeedback *);
int bk_item_feedback_stop(BkItemFeedback *, char error[256]);
/* Required ordered services for item_assets_pickup / item_pickup_run.
 * Calls restart one-shot PCM at0, then load the selected raw message.
 * Borrowed message is replaced by the next successful notice callback.
 * No implicit audio poll/fill, input handling, or text rendering. */
BkItemPickupOps bk_item_feedback_ops(BkItemFeedback *);
const BkMessage *bk_item_feedback_message(const BkItemFeedback *);
/*4ec87c reloads51765d without resetting retained label range/current or
 * replacing the se101 clip. Message pointer identity remains stable. */
int bk_item_feedback_reload_message(BkItemFeedback *, BkResourceStore *,
                                    char error[256]);
const BkDialogue *bk_item_feedback_dialogue(const BkItemFeedback *);
void bk_item_feedback_close_message(BkItemFeedback *);
/*4ef018 releases the pickup buffer;4ec9f0 recreates it without losing the
 * separate retained dialogue metadata. Neither reload starts playback. */
int bk_item_feedback_release_sound(BkItemFeedback *, char error[256]);
int bk_item_feedback_reload_sound(BkItemFeedback *, BkResourceStore *,
                                  char error[256]);
#endif
