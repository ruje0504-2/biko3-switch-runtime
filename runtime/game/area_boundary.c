#include "game/area_boundary.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
static int inside(double x, double z, double left, double top, double right,
                  double bottom) {
  return x >= left && x <= right && z >= bottom && z <= top;
}
int bk_area_npc_outside(uint8_t *out, const float p[3], const BkAreaBounds *b) {
  if (!out || !p || !b || !isfinite(p[0]) || !isfinite(p[2]))
    return 0;
  *out = (uint8_t)!inside(p[0], p[2], b->left, b->top, b->right, b->bottom);
  return 1;
}
int bk_area_prop_boundary(BkPropState *p, uint8_t *gate, unsigned group,
                          unsigned area, const BkAreaBounds *b) {
  if (!p || !gate || !b || group >= 5 || area >= 9 ||
      !isfinite(p->path.position[0]) || !isfinite(p->path.position[2]))
    return 0;
  double left = b->left, top = b->top, right = b->right, bottom = b->bottom;
  int special = 1;
  if ((group == 0 && area == 2) || (group == 1 && area == 3) ||
      (group == 2 && area == 3) || (group == 3 && area == 0) ||
      (group == 4 && area == 2))
    bottom = -300;
  else if ((group == 0 && area == 3) || (group == 1 && area == 0) ||
           (group == 2 && area == 2) || (group == 3 && area == 3) ||
           (group == 4 && area == 1))
    top = 300;
  else if (group == 3 && area == 6) {
    left = -500;
    right = 400;
  } else
    special = 0;
  double x = p->path.position[0], z = p->path.position[2];
  if (special) {
    p->hidden = (uint8_t)!inside(x, z, left, top, right, bottom);
    return 1;
  }
  uint8_t next_gate = 0;
  int visible;
  if (p->kind == 1) {
    if (!isfinite(p->path.yaw))
      return 0;
    double angle = (double)p->path.yaw * .01745329238474369f;
    /* Offsets are stored asfloat; adding the body location before boundary
     * comparisons stays in x87 precision (no intermediate float position). */
    float dx = (float)(sin(angle) * 400), dz = (float)(cos(angle) * 400);
    float rx = (float)(sin(angle) * -400), rz = (float)(cos(angle) * -400);
    visible = inside(x + dx, z + dz, left, top, right, bottom) ||
              inside(x, z, left, top, right, bottom) ||
              inside(x + rx, z + rz, left, top, right, bottom);
    next_gate = (uint8_t)visible;
  } else
    visible = inside(x, z, left, top, right, bottom);
  p->hidden = (uint8_t)!visible;
  *gate = next_gate;
  return 1;
}
int bk_area_sound_trigger(int32_t group, int32_t area, int32_t cursor,
                          uint8_t played) {
  if (played == 1)
    return 0;
  switch (group) {
  case 0:
    return (area == 6 && cursor == 234) || (area == 7 && cursor == 55);
  case 1:
    return (area == 6 && cursor == 132) || (area == 7 && cursor == 26);
  case 2:
    return (area == 4 && cursor == 165) || (area == 5 && cursor == 280) ||
           (area == 6 && cursor == 230);
  case 3:
    return (area == 6 && cursor == 276) || (area == 7 && cursor == 241);
  case 4:
    return area == 7 && cursor == 329;
  default:
    return 0;
  }
}
const char *bk_area_sound_file(int32_t group, int32_t area) {
  if ((group == 0 && (area == 6 || area == 7)) ||
      ((group == 1 || group == 3) && area == 7))
    return "se304.wav";
  if ((group == 2 && area >= 4 && area <= 6) || (group == 4 && area == 7))
    return "se155.wav";
  if ((group == 1 || group == 3) && area == 6)
    return "se156.wav";
  return NULL;
}
static int valid_name(const char *name) {
  if (!name)
    return 0;
  for (unsigned i = 0; i < 260; ++i)
    if (!name[i])
      return 1;
  return 0;
}
int bk_area_boundary_step(BkAreaBoundaryState *state, BkPropState props[16],
                          BkNpcSpatialState *npc,
                          BkNpcInteractionState *interaction,
                          const BkAreaBoundaryInput *in,
                          BkAreaBoundaryCommands *out) {
  if (!state || !props || !npc || !interaction || !in || !out ||
      in->group >= 5 || in->area >= 9 || in->props_present >> 16)
    return 0;
  BkPropState p[16];
  memcpy(p, props, sizeof(p));
  BkNpcSpatialState n = *npc;
  BkNpcInteractionState contact = *interaction;
  BkAreaBoundaryState s = *state;
  BkAreaBoundaryCommands commands = {0};
  for (unsigned i = 0; i < 16; ++i)
    if ((in->props_present & (1u << i)) &&
        !bk_area_prop_boundary(&p[i], &s.ambient_gate, in->group, in->area,
                               &in->bounds))
      return 0;
  const char *file = bk_area_sound_file((int32_t)in->group, (int32_t)in->area);
  if (in->npc_present) {
    if (!bk_area_npc_outside(&n.ai.point.motion.hidden, n.path.position,
                             &in->bounds))
      return 0;
    if (bk_area_sound_trigger(in->npc_group, (int32_t)in->area,
                              (int32_t)n.path.cursor, s.npc_sound_played) &&
        file) {
      BkSpatialAudio gain;
      if (!bk_spatial_audio(&gain, n.path.position, in->player_position,
                            in->player_yaw, in->effect_volume, 6))
        return 0;
      commands.commands[commands.count++] =
          (BkAreaSoundCommand){BK_AREA_SOUND_NPC, file, gain};
      s.npc_sound_played = 1;
    }
  }
  if (n.ai.point.motion.hidden == 1) {
    if (!valid_name(in->player_wall) || !valid_name(in->boundary_wall))
      return 0;
    if (!strcmp(in->player_wall, in->boundary_wall)) {
      contact.response = 1;
      if (s.player_sound_played == 0 && file &&
          in->player_sound_suppressed == 0) {
        if (in->effect_volume < -10000 || in->effect_volume > 0)
          return 0;
        commands.commands[commands.count++] = (BkAreaSoundCommand){
            BK_AREA_SOUND_PLAYER, file, {in->effect_volume, 0}};
        s.player_sound_played = 1;
      }
    }
  }
  memcpy(props, p, sizeof(p));
  *npc = n;
  *interaction = contact;
  *state = s;
  *out = commands;
  return 1;
}
