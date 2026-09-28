#include "scene/opening_session.h"
typedef struct {
  const BkOpeningServices *s;
  BkTextFlow *flow;
} Context;
static int valid(const BkOpeningServices *s) {
  return s && s->store && s->entry && s->dialogue && s->items && s->click &&
         s->text.recreate && s->text.clear && s->text.bind;
}
static int click(void *p, char e[256]) {
  return bk_system_audio_restart(((Context *)p)->s->click, e);
}
static int next(void *p, int *done, char e[256]) {
  return bk_dialogue_assets_next(((Context *)p)->s->dialogue, done, e);
}
static int bind(void *p, char e[256]) {
  const BkOpeningServices *s = ((Context *)p)->s;
  return s->text.bind(s->text.context, &s->dialogue->state.text, e);
}
static int clear(void *p, char e[256]) {
  const BkOpeningServices *s = ((Context *)p)->s;
  return s->text.clear(s->text.context, e);
}
static int close_dialogue(void *p, char e[256]) {
  (void)e;
  bk_dialogue_assets_close(((Context *)p)->s->dialogue);
  return 1;
}
static int items(void *p, char e[256]) {
  Context *c = p;
  const BkOpeningServices *s = c->s;
  if (!bk_item_feedback_reload_message(s->items, s->store, e) ||
      !s->text.recreate(s->text.context, BK_NOTICE_TEXT_ITEMS, e))
    return 0;
  c->flow->started = 0;
  c->flow->enabled = 1;
  return 1;
}
static int select_camera(void *p, char e[256]) {
  BkFollowCamera *c = bk_entry_assets_camera(((Context *)p)->s->entry);
  return bk_actor_pose_select(bk_follow_camera_bind_track(c), 0, 1, e);
}
int bk_opening_session_initialize(const BkOpeningServices *s,
                                  BkGameFrameState *g, BkItemNoticeState *n,
                                  BkTextFlow *flow, char e[256]) {
  if (!valid(s) || !g || !n || !flow || g->group >= 5 || n->panel.stage > 5 ||
      (g->camera.phase != 0 && g->camera.phase != 2 && g->camera.phase != 3)) {
    snprintf(e, 256, "opening session: invalid initialization");
    return 0;
  }
  Context c = {s, flow};
  if (!select_camera(&c, e))
    return 0;
  if (g->camera.phase == 0) {
    static const int32_t last[] = {10005, 20006, 30007, 40011, 50007};
    char filename[32];
    snprintf(filename, sizeof(filename), "i0%u_01.txt", g->group + 1);
    s->dialogue->state.first_label = (int32_t)(g->group + 1) * 10000;
    s->dialogue->state.last_label = last[g->group];
    int done;
    if (!bk_dialogue_assets_load(s->dialogue, s->store, filename, e) ||
        !s->text.recreate(s->text.context, BK_NOTICE_TEXT_OPENING, e) ||
        !next(&c, &done, e) || !bind(&c, e))
      return 0;
    flow->started = 0;
    flow->enabled = 1;
    bk_fade_sprite_request(&n->panel, 1);
    g->pickup.notice_visible = 1;
  } else {
    bk_fade_sprite_request(&n->panel, 0);
    g->pickup.notice_visible = 0;
  }
  return 1;
}
int bk_opening_session_step(const BkOpeningServices *s, BkGameFrameState *g,
                            BkItemNoticeState *n, BkTextFlow *flow, int advance,
                            char e[256]) {
  if (!valid(s) || !g || !n || !flow) {
    snprintf(e, 256, "opening session: missing services/state");
    return 0;
  }
  BkClipTiming timing;
  BkActorPose *track =
      bk_follow_camera_bind_track(bk_entry_assets_camera(s->entry));
  if (!bk_actor_pose_timing(track, 0, &timing)) {
    snprintf(e, 256, "opening session: missing camera slot0");
    return 0;
  }
  Context c = {s, flow};
  BkOpeningOps ops = {&c,    click,          next,  bind,
                      clear, close_dialogue, items, select_camera};
  BkOpeningBindings b = {&g->camera,
                         &g->pickup.notice_visible,
                         &g->npc.ai.point.motion.hidden,
                         &g->player_hidden,
                         &g->npc.ai.point.motion.behavior,
                         &n->panel,
                         flow};
  return bk_opening_phase_step(&b, &ops, advance, timing.source, timing.end, e);
}

int bk_notice_text_style(BkNoticeTextKind kind, BkTextStyle *out) {
  if (!out || (kind != BK_NOTICE_TEXT_OPENING && kind != BK_NOTICE_TEXT_ITEMS))
    return 0;
  *out = kind == BK_NOTICE_TEXT_OPENING
             ? (BkTextStyle){104, 380, 432, 72, 16, 18, 1, 1, {1, 1, 1}, 1}
             : (BkTextStyle){192, 400, 316, 268, 16, 16, 1, 1, {1, 1, 1}, 1};
  return 1;
}
