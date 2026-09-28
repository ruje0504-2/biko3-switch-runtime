#include "game/player_interaction.h"
#include "world/placement.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int is(const BkPlayerMovement *m, const int32_t *a, unsigned slot) {
  return m->action == a[slot];
}
static int transition(BkPlayerMovement *m, BkPlayerInteraction *s,
                      const int32_t *a, unsigned slot) {
  if (is(m, a, 0)) {
    m->action = a[slot];
    s->script_phase = 1;
  } else if (is(m, a, slot)) {
    m->action = a[slot + 1];
    s->script_phase = 2;
    m->interaction_mode = 0;
  }
  return 0;
}
int bk_player_interaction(BkPlayerMovement *movement,
                          BkPlayerInteraction *state, int *controls_allowed,
                          const BkPlayerInteractionInput *in, char error[256]) {
  if (!movement || !state || !controls_allowed || !in ||
      (in->buttons & ~(BK_PLAYER_STANCE | BK_PLAYER_INTERACT)))
    goto invalid;
  BkPlayerMovement m = *movement;
  BkPlayerInteraction s = *state;
  const int32_t *a = in->trigger.actions;
  int allowed = 1;
  if (in->buttons & BK_PLAYER_STANCE) {
    if (m.interaction_mode == 0 || m.interaction_mode == 1) {
      int ordinary = 0;
      for (unsigned i = 0; i < 7; ++i)
        ordinary |= is(&m, a, i);
      if (ordinary) {
        m.action = a[8];
        m.interaction_mode = 1;
      } else if (is(&m, a, 8)) {
        m.action = a[9];
        m.interaction_mode = 0;
      }
    }
  } else if ((in->buttons & BK_PLAYER_INTERACT) &&
             (is(&m, a, 0) || is(&m, a, 11) || is(&m, a, 13) || is(&m, a, 16) ||
              is(&m, a, 20))) {
    BkPlayerTriggerInput query = in->trigger;
    memcpy(query.position, m.position, sizeof(query.position));
    query.action = m.action;
    int hit;
    if (!bk_player_trigger_prop(&s.trigger, &hit, &query, 0, error))
      return 0;
    if (hit) {
      allowed = transition(&m, &s, a, 11);
      goto done;
    }
    if (!bk_player_trigger_prop(&s.trigger, &hit, &query, 1, error))
      return 0;
    if (hit) {
      allowed = transition(&m, &s, a, 13);
      goto done;
    }
    if (!bk_player_trigger_wall(&s.trigger, &hit, &query, error))
      return 0;
    if (hit) {
      allowed = transition(&m, &s, a, 16);
      goto done;
    }
    if (!bk_player_trigger_cover(&hit, &query))
      goto invalid;
    if (hit) {
      if (is(&m, a, 0)) {
        m.action = a[18];
        if (!bk_route_heading(&m.yaw, in->normal[0], in->normal[2], 0, 0))
          goto invalid;
      }
      if (m.interaction_mode == 5 && is(&m, a, 20))
        m.action = a[19];
      allowed = 0;
      goto done;
    }
  }
  if (is(&m, a, 9)) {
    if (in->active_clip == 0)
      m.action = a[0];
  } else if (is(&m, a, 11) || is(&m, a, 13) || is(&m, a, 16)) {
    if (m.action < 0 || m.action >= BK_PLAYER_CLIP_SLOTS)
      goto invalid;
    const BkPlayerClipTiming *clip = &in->clips[m.action];
    if (!isfinite(clip->source) || !isfinite(clip->end))
      goto invalid;
    if (clip->source >= clip->end) {
      m.interaction_mode = 2;
      s.script_phase = 0;
    }
  } else if (is(&m, a, 12) || is(&m, a, 13) || is(&m, a, 17)) {
    /* Native second branch also tests slot13, not14. Preserve aliases/order. */
    m.interaction_mode = 0;
    if (in->active_clip == 0) {
      m.action = a[0];
      s.script_phase = 0;
    }
  } else if (is(&m, a, 18)) {
    if (in->active_clip == a[20]) {
      m.action = a[20];
      m.interaction_mode = 5;
    }
    allowed = 0;
  } else if (is(&m, a, 19)) {
    if (in->active_clip == a[0]) {
      m.interaction_mode = 0;
      m.action = a[0];
    }
    allowed = 0;
  }
done:
  *movement = m;
  *state = s;
  *controls_allowed = allowed;
  return 1;
invalid:
  if (error)
    snprintf(error, 256, "Invalid player interaction state");
  return 0;
}
