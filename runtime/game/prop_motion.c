#include "game/prop_motion.h"
#include <limits.h>
#include <math.h>
static int car(int32_t kind) { return kind == 0 || (kind >= 13 && kind <= 17); }
int bk_prop_smooth_heading(int32_t kind) {
  return car(kind) || kind == 1 || kind == 2 || kind == 6;
}
int bk_prop_needs_ground(int32_t group, int32_t area) {
  return group == 0 && area >= 4 && area <= 9;
}
static int npc_wait(const BkPropMotionInput *in) {
  return in->npc_last_crossed > 2 &&
         (in->npc_previous_flags[0] == 3 || in->npc_previous_flags[1] == 3);
}
static int valid(const BkPropState *s, const BkPropMotionInput *in) {
  return s && in && s->kind >= 0 && s->kind < 20 && isfinite(in->seconds) &&
         in->seconds >= 0 && (double)in->seconds * 60 < INT32_MAX;
}
int bk_prop_motion_select(BkPropState *state, BkPropShared *shared,
                          const BkPropMotionInput *in,
                          BkPropMotionEffects *effects) {
  if (!valid(state, in) || !shared || !effects || !isfinite(shared->distance) ||
      !isfinite(shared->alpha))
    return 0;
  BkPropState s = *state;
  BkPropShared sh = *shared;
  BkPropMotionEffects out = {.allowed = 1, .material = -2};
  if (in->player_action == in->player_actions[10])
    sh.distance = 0;
  else if (car(s.kind)) {
    if (s.action == s.actions[3])
      sh.distance = (float)(48.0 * in->seconds);
    else {
      if (s.background_wait == 1) {
        if (in->background_clip == 0) {
          s.action = s.actions[3];
          s.background_wait = 0;
        }
        if (npc_wait(in)) {
          s.background_wait = 1;
          s.action = s.actions[0];
        }
      } else
        s.action = s.actions[3];
      sh.distance = 0;
      out.allowed = 0;
    }
  } else if (s.kind == 1)
    sh.distance = (float)(48.0 * in->seconds);
  else if (s.kind == 2 || s.kind == 6 || s.kind == 7) {
    /* Retains previous prop's distance, exactly as native. */
  } else {
    sh.distance = 0;
    out.allowed = 0;
    if (s.kind == 10 || s.kind == 19) {
      if (in->player_action == in->player_actions[11] ||
          in->player_action == in->player_actions[13]) {
        /* 51480b stores dt*.5 but retains its x87 value for subtraction. */
        sh.alpha = (float)((double)sh.alpha - (double)in->seconds * .5);
        if (sh.alpha <= 0)
          sh.alpha = 0;
      } else if (in->player_action == in->player_actions[12] ||
                 in->player_action == in->player_actions[14]) {
        sh.alpha = (float)((double)sh.alpha + in->seconds);
        if (sh.alpha >= 1)
          sh.alpha = 1;
      } else
        sh.alpha = 1;
      out.material = s.kind == 10 ? -1 : 1;
      out.alpha = sh.alpha;
    } else if (s.kind == 11 || s.kind == 18) {
      int hide =
          in->player_action == in->player_actions[13] && in->player_mode == 2;
      if (s.kind == 11)
        s.hidden = (uint8_t)hide;
      else {
        out.material = 0;
        out.alpha = hide ? 0 : 1;
      }
    }
  }
  *state = s;
  *shared = sh;
  *effects = out;
  return 1;
}
int bk_prop_point_apply(BkPropState *state, int8_t flag,
                        const BkPropMotionInput *in) {
  if (!valid(state, in))
    return 0;
  BkPropState s = *state;
  s.route_flag = flag;
  if (flag == 0)
    s.action = s.actions[3];
  else if (flag == 2) {
    s.action = s.actions[0];
    s.wait.duration = 5000;
    s.wait.armed = 0;
  } else if (flag == 3) {
    if (car(s.kind) || s.kind == 2) {
      if (in->background_clip == 0) {
        s.background_wait = 0;
        s.action = s.actions[3];
        if (npc_wait(in)) {
          s.background_wait = 1;
          s.action = s.actions[0];
        }
      } else {
        s.background_wait = 1;
        s.action = s.actions[0];
      }
    } else if (in->background_clip == 2)
      s.action = s.actions[1];
    else {
      s.background_wait = 1;
      s.action = s.actions[0];
    }
  } else if (flag == 5)
    s.action = s.actions[0];
  *state = s;
  return 1;
}
