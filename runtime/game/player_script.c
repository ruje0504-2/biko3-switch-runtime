#include "game/player_script.h"
#include "core/camera.h"
#include <math.h>
#include <stdio.h>
static int is(const BkPlayerMovement *m, const int32_t *a, unsigned slot) {
  return m->action == a[slot];
}
static const BkPlayerClipTiming *timing(const BkPlayerInteractionInput *in,
                                        int32_t action) {
  if (action < 0 || action >= BK_PLAYER_CLIP_SLOTS)
    return NULL;
  const BkPlayerClipTiming *t = &in->clips[action];
  return isfinite(t->start) && isfinite(t->end) && isfinite(t->source) ? t
                                                                       : NULL;
}
int bk_player_script_step(BkPlayerMovement *movement,
                          BkPlayerInteraction *interaction, float seconds,
                          float return_yaw, const BkPlayerInteractionInput *in,
                          BkPlayerScriptEffects *effects, char error[256]) {
  if (!movement || !interaction || !in || !effects || !isfinite(seconds) ||
      seconds < 0)
    goto invalid;
  BkPlayerMovement m = *movement;
  BkPlayerInteraction s = *interaction;
  BkPlayerScriptEffects out = {0};
  const int32_t *a = in->trigger.actions;
  const BkPlayerClipTiming *t = timing(in, m.action);
  if (!t)
    goto invalid;
  if (is(&m, a, 10)) {
    const BkPlayerClipTiming *active = timing(in, in->active_clip);
    if (!active)
      goto invalid;
    out.complete = t->end <= active->source;
  } else
    out.complete = t->end == t->source;
  if (out.complete)
    goto done;
  m.previous[0] = m.position[0];
  m.previous[2] = m.position[2];
  float dx = 0, dz = 0;
  if (s.script_phase == 1 || s.script_phase == 2) {
    const float *target =
        s.script_phase == 1 ? s.trigger.target : s.trigger.origin;
    dx = (float)((double)target[0] - m.position[0]);
    dz = (float)((double)target[2] - m.position[2]);
    dx = (float)((double)seconds * 2 * dx);
    dz = (float)((double)seconds * 2 * dz);
    if (!isfinite(dx) || !isfinite(dz) || !isfinite(target[3]))
      goto invalid;
    m.yaw = target[3];
    if (s.script_phase == 2 && (is(&m, a, 12) || is(&m, a, 14))) {
      double duration = ((double)t->end - t->start) * .7;
      if (!duration)
        goto invalid;
      float weight = (float)(((double)t->source - t->start) / duration);
      if (!isfinite(weight))
        goto invalid;
      if (weight >= 1)
        weight = 1;
      if (!bk_angle_blend_degrees(&m.yaw, return_yaw, target[3], weight))
        goto invalid;
    }
  }
  int allowed;
  if (!bk_player_interaction(&m, &s, &allowed, in, error))
    return 0;
  m.turn[0] = m.turn[1] = 0;
  m.pitch = 20;
  if (!bk_actor_placement(&out.roots[0], m.position, m.yaw))
    goto invalid;
  out.placements = 1;
  int move = 1;
  if (is(&m, a, 11) || is(&m, a, 13) || is(&m, a, 12) || is(&m, a, 14) ||
      is(&m, a, 16) || is(&m, a, 17)) {
    t = timing(in, m.action);
    if (!t)
      goto invalid;
    if (is(&m, a, 11) || is(&m, a, 13) || is(&m, a, 12) || is(&m, a, 14))
      move = t->source > 369 && t->source < 400;
    else if (is(&m, a, 16))
      move = t->source > 453;
    else
      move = t->source < 534;
  }
  if (move) {
    m.velocity[0] = dx;
    m.velocity[2] = dz;
    m.position[0] = (float)((double)m.position[0] + dx);
    m.position[2] = (float)((double)m.position[2] + dz);
    if (!bk_actor_placement(&out.roots[1], m.position, m.yaw))
      goto invalid;
    out.placements = 2;
  }
done:
  *movement = m;
  *interaction = s;
  *effects = out;
  return 1;
invalid:
  if (error)
    snprintf(error, 256, "Invalid scripted player movement");
  return 0;
}
