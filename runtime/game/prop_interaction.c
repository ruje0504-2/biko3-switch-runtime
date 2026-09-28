#include "game/prop_interaction.h"
#include "world/proximity.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
const char *bk_prop_interaction_wall(int32_t kind) {
  static const char *const cars[] = {
      "Mesh_Kuruma_Atari@H93_00.X", "Mesh_Kuruma_Atari@H93_10.X",
      "Mesh_Kuruma_Atari@H93_11.X", "Mesh_Kuruma_Atari@H93_12.X",
      "Mesh_Kuruma_Atari@H93_13.X", "Mesh_Kuruma_Atari@H93_14.X"};
  if (kind == 0)
    return cars[0];
  if (kind >= 13 && kind <= 17)
    return cars[kind - 12];
  return kind >= 0 && kind < 20 ? "NULL" : NULL;
}
static int vertical(const float *prop, const float *player) {
  return (double)prop[1] >= (double)player[1] - 5 &&
         (double)prop[1] <= (double)player[1] + 5;
}
static int valid_wall(const char *wall) {
  if (wall)
    for (unsigned i = 0; i < 260; ++i)
      if (!wall[i])
        return 1;
  return 0;
}
static void sound(BkPropInteractionCommands *commands, unsigned i, int play,
                  int loop) {
  commands->sounds[commands->count++] = (BkPropInteractionSound){i, play, loop};
}
int bk_prop_interaction_step(const BkPropInteractionActor actors[16],
                             BkPlayerControl *player, int8_t *stimulus,
                             uint8_t *outcome, BkPropInteractionShared *shared,
                             const BkPropInteractionInput *in,
                             BkPropInteractionCommands *out, char error[256]) {
  if (!actors || !player || !stimulus || !outcome || !shared || !in || !out)
    goto invalid;
  BkPlayerControl p = *player;
  BkPropInteractionShared sh = *shared;
  BkPropInteractionCommands commands = {0};
  BkPropState props[16];
  BkPropSoundState sounds[16];
  BkPropInteractionState contacts[16];
  int8_t noise = *stimulus;
  uint8_t result = *outcome;
  sh.available = 0;
  const float *position = p.spatial.movement.position;
  const int32_t *actions = in->player_actions;
  for (unsigned i = 0; i < 16; ++i) {
    const BkPropInteractionActor *actor = &actors[i];
    if (!actor->motion)
      continue;
    if (!actor->sound || !actor->interaction)
      goto invalid;
    props[i] = *actor->motion;
    sounds[i] = *actor->sound;
    contacts[i] = *actor->interaction;
    BkPropState *prop = &props[i];
    int kind = prop->kind, close;
    int32_t action = p.spatial.movement.action;
    const float *source = prop->path.position;
    if (kind == 0 || (kind >= 13 && kind <= 17) || kind == 1 || kind == 2) {
      int car = kind != 1 && kind != 2;
      if (car && action == actions[13])
        continue;
      if (!valid_wall(in->wall))
        goto invalid;
      if (strcmp(in->wall, bk_prop_interaction_wall(kind)))
        continue;
      if (car) {
        if (!bk_proximity_xz(&close, position, source, 50))
          goto invalid;
        if (!close)
          continue;
      }
      if (!isfinite(prop->path.velocity[0]) ||
          !isfinite(prop->path.velocity[2]))
        goto invalid;
      if (!prop->path.velocity[0] && !prop->path.velocity[2])
        continue;
      if (!car) {
        result = 1;
        continue;
      }
      if (in->hud_blocked)
        continue;
      if (!contacts[i].car_hit) {
        if (!isfinite(prop->path.yaw) || !isfinite(position[1]))
          goto invalid;
        float yaw = (float)((double)prop->path.yaw + 180);
        if (yaw >= 360)
          yaw = (float)((double)yaw - 360);
        double radians = (double)prop->path.yaw * 0.01745329238474369f;
        BkPlayerTrigger *t = &p.interaction.trigger;
        memcpy(t->origin, position, 12);
        t->origin[3] = t->target[3] = yaw;
        t->target[0] = (float)(sin(radians) * 100. + position[0]);
        t->target[1] = position[1];
        t->target[2] = (float)(cos(radians) * 100. + position[2]);
        if (!isfinite(t->target[0]) || !isfinite(t->target[2]))
          goto invalid;
        contacts[i].car_hit = 1;
        sounds[i].stage = 4;
      }
      p.spatial.movement.action = actions[10];
      p.completion_mode = p.interaction.script_phase = 1;
      continue;
    }
    if (kind == 6 || kind == 7 || kind < 3 || kind > 19)
      continue;
    float radius = kind == 3 || kind == 4                 ? 40
                   : kind == 5                            ? 50
                   : kind == 8 || kind == 9 || kind == 12 ? 20
                                                          : 100;
    if (!bk_proximity_xz(&close, source, position, radius))
      goto invalid;
    if (close && (!isfinite(position[1]) || !isfinite(source[1])))
      goto invalid;
    if (kind == 3 || kind == 4) {
      if (close && vertical(source, position)) {
        if (action != actions[0] && action != actions[1]) {
          noise = 1;
          prop->action = kind == 3 ? 2 : 10;
          if (!sounds[i].stage) {
            sound(&commands, i, 1, 1);
            sounds[i].stage = 1;
          }
        }
      } else {
        sounds[i].stage = 0;
        prop->action = kind == 3 ? 0 : 8;
        sound(&commands, i, 0, 0);
      }
    } else if (kind == 5) {
      if (!close)
        continue;
      float heading;
      if (!bk_route_heading(&heading, source[0], source[2], position[0],
                            position[2]) ||
          !isfinite(prop->path.yaw))
        goto invalid;
      float rear = (float)((double)prop->path.yaw + 180);
      if (rear >= 360)
        rear = (float)((double)rear - 360);
      heading = (float)((double)heading - rear);
      /* Original deliberately does not normalize this difference. */
      if (heading < -90 || heading > 90 || !vertical(source, position))
        continue;
      if (action == actions[0] || action == actions[1]) {
        contacts[i].alarm.duration = 500;
        contacts[i].alarm.armed = 0;
      } else if (action != actions[3] ||
                 bk_timer_poll(&contacts[i].alarm, in->now_ms)) {
        result = 5;
        sh.selected = (int32_t)i;
      }
    } else if (kind == 8 || kind == 9 || kind == 12) {
      if (close && vertical(source, position) &&
          (action == actions[3] || action == actions[8]) && !prop->action) {
        prop->action = 1;
        sound(&commands, i, 1, 0);
        noise = 2;
      }
    } else if (close && vertical(source, position)) {
      if (!actor->world)
        goto invalid;
      for (unsigned j = 0; j < 16; ++j)
        if (!isfinite(actor->world[j]))
          goto invalid;
      sh.available = 1;
      memcpy(sh.matrix, actor->world, sizeof(sh.matrix));
      sh.matrix[12] = source[0];
      sh.matrix[13] = (float)((double)source[1] + 20);
      sh.matrix[14] = source[2];
    }
  }
  for (unsigned i = 0; i < 16; ++i)
    if (actors[i].motion) {
      *actors[i].motion = props[i];
      *actors[i].sound = sounds[i];
      *actors[i].interaction = contacts[i];
    }
  *player = p;
  *shared = sh;
  *stimulus = noise;
  *outcome = result;
  *out = commands;
  return 1;
invalid:
  if (error)
    snprintf(error, 256, "prop interaction: invalid reached state/geometry");
  return 0;
}
