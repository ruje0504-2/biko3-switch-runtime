#include "scene/ending_auxiliary.h"
typedef struct {
  const BkEndingAuxiliaryServices *services;
  BkEndingAuxiliaryState *state;
  BkEndingFrameState *frame;
} Context;
static int active(void *p, int32_t *slot, char e[256]) {
  Context *c = p;
  BkClipState clock;
  if (!bk_actor_pose_state(c->services->primary, &clock)) {
    snprintf(e, 256, "ending auxiliary: missing primary clip state");
    return 0;
  }
  *slot = clock.slot;
  return 1;
}
static int write_clip(void *p, unsigned slot, BkEndingClipWrite kind,
                      int32_t value, char e[256]) {
  Context *c = p;
  BkClipEdit edit = {.slot = slot};
  switch (kind) {
  case BK_ENDING_CLIP_CHAIN:
    edit.fields = BK_CLIP_EDIT_CHAIN;
    edit.chain = value;
    break;
  case BK_ENDING_CLIP_NEXT:
    edit.fields = BK_CLIP_EDIT_NEXT;
    edit.next = value;
    break;
  case BK_ENDING_CLIP_REWIND: {
    BkClipTiming timing;
    if (!bk_actor_pose_timing(c->services->primary, slot, &timing)) {
      snprintf(e, 256, "ending auxiliary: missing source slot%u", slot);
      return 0;
    }
    edit.fields = BK_CLIP_EDIT_SOURCE;
    edit.source = timing.start;
    break;
  }
  default:
    return 0;
  }
  return bk_actor_pose_edit_clips(c->services->primary, &edit, 1, e);
}
static int request(void *p, unsigned slot, char e[256]) {
  Context *c = p;
  return bk_actor_pose_request_mode(c->services->primary, slot,
                                    BK_CLIP_REQUEST_CONFIGURED, e);
}
static int audio(void *p, const BkEndingAudioCall *call, int *playing,
                 char e[256]) {
  Context *c = p;
  return bk_ending_audio_call(c->services->audio, c->frame->group,
                              c->state->variant, c->state->selection, call,
                              playing, e);
}
static int eyes(void *p, unsigned slot, char e[256]) {
  return bk_eye_assets_select(((Context *)p)->services->eyes, slot, e);
}
static int speech(void *p, unsigned slot, const char *name, int32_t volume,
                  char e[256]) {
  Context *c = p;
  return bk_ending_audio_speech(c->services->audio, slot, name, volume, e);
}
int bk_ending_ui_tail_apply(const BkEndingAuxiliaryServices *v, BkEndingUi *ui,
                            BkEndingStageUi *stage, BkEndingUiNormalNotice *n,
                            BkEndingUiAuxNotice *a,
                            const BkEndingUiTailBindings *b, float scale,
                            float seconds, BkEndingUiFrame *out, char e[256]) {
  if (!v || !b) {
    snprintf(e, 256, "ending UI tail: missing resource bindings");
    return 0;
  }
  Context context = {v, b->auxiliary, b->frame};
  BkEndingUiTailOps ops = {{&context, active, write_clip, request, audio, eyes},
                           speech};
  return bk_ending_ui_tail(ui, stage, n, a, b, &ops, scale, seconds, out, e);
}
int bk_ending_auxiliary_apply(const BkEndingAuxiliaryServices *v,
                              BkEndingAuxiliaryState *s, BkEndingFrameState *f,
                              int32_t proposed, int32_t voice_volume,
                              int32_t effect_volume, int32_t *result,
                              char e[256]) {
  if (!v || !v->primary || !v->eyes || !v->audio) {
    snprintf(e, 256, "ending auxiliary: incomplete resources");
    return 0;
  }
  Context context = {v, s, f};
  BkEndingAuxiliaryOps ops = {&context, active, write_clip,
                              request,  audio,  eyes};
  return bk_ending_auxiliary_change(s, f, proposed, voice_volume, effect_volume,
                                    &ops, result, e);
}
