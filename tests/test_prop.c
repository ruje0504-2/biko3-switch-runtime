#include "game/prop_interaction.h"
#include "game/prop_motion.h"
#include "model/animation.h"
#include "model/clip.h"
#include "world/proximity.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void word(unsigned char *p, uint32_t x) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (unsigned char)(x >> (i * 8));
}
static void number(unsigned char *p, float x) {
  uint32_t bits;
  memcpy(&bits, &x, 4);
  word(p, bits);
}
static void interactions(void) {
  BkPropState props[3] = {{.kind = 0}, {.kind = 8}, {.kind = 10}};
  BkPropSoundState sounds[3] = {0};
  BkPropInteractionState contacts[3] = {0};
  float world[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  BkPropInteractionActor actors[16] = {0};
  for (unsigned i = 0; i < 3; ++i)
    actors[i] =
        (BkPropInteractionActor){&props[i], &sounds[i], &contacts[i], world};
  BkPlayerControl p = {0};
  BkPropInteractionShared shared = {.selected = 12, .available = 255};
  BkPropInteractionInput in = {.wall = bk_prop_interaction_wall(0)};
  BkPropInteractionCommands out = {.count = 13};
  int8_t stimulus = -1;
  uint8_t outcome = 7;
  char error[256];
  for (unsigned i = 0; i < 21; ++i)
    in.player_actions[i] = (int32_t)i + 100;
  in.player_actions[10] =
      in.player_actions[3]; /* car changes action -> can trigger slot1 */
  props[0].path.velocity[0] = 1;
  props[2].hidden = 255; /* hidden root still participates */
  props[2].path.position[0] = 10;
  props[2].path.position[1] = 5;
  world[2] =
      NAN; /* Last reached slot invalid: no earlier car/sound mutations */
  BkPlayerControl before = p;
  assert(!bk_prop_interaction_step(actors, &p, &stimulus, &outcome, &shared,
                                   &in, &out, error));
  assert(!memcmp(&p, &before, sizeof(p)) && !contacts[0].car_hit &&
         !sounds[0].stage && !props[1].action && stimulus == -1 &&
         outcome == 7 && shared.available == 255 && out.count == 13);
  world[2] = 3;
  assert(bk_prop_interaction_step(actors, &p, &stimulus, &outcome, &shared, &in,
                                  &out, error));
  assert(p.spatial.movement.action == in.player_actions[3] &&
         p.interaction.script_phase == 1 && p.completion_mode == 1 &&
         p.interaction.trigger.target[2] == 100 && contacts[0].car_hit == 1 &&
         sounds[0].stage == 4 && stimulus == 2 && props[1].action == 1);
  assert(out.count == 1 && out.sounds[0].index == 1 && out.sounds[0].play &&
         !out.sounds[0].loop && shared.available == 1 &&
         shared.selected == 12 && shared.matrix[12] == 10 &&
         shared.matrix[13] == 25 && shared.matrix[15] == 16);
  /* Unavailable prompts retain the last matrix. Existing car latch preserves
   * script endpoints but still writes the reaction action/phase. */
  props[0].path.yaw = 90;
  actors[2].motion = NULL;
  assert(bk_prop_interaction_step(actors, &p, &stimulus, &outcome, &shared, &in,
                                  &out, error));
  assert(!out.count && !shared.available && shared.matrix[13] == 25 &&
         p.interaction.trigger.target[2] == 100);
  actors[0].motion = NULL;
  props[1].kind = 5;
  props[1].path.position[2] = -1;
  props[1].path.yaw = 180;
  contacts[1].alarm = (BkTimer){500, 1000, 1};
  in.now_ms = 999;
  assert(bk_prop_interaction_step(actors, &p, &stimulus, &outcome, &shared, &in,
                                  &out, error) &&
         outcome == 7);
  in.now_ms = 1000;
  assert(bk_prop_interaction_step(actors, &p, &stimulus, &outcome, &shared, &in,
                                  &out, error) &&
         outcome == 5 && shared.selected == 1 && !contacts[1].alarm.armed);
}
int main(void) {
  interactions();
  char error[256];
  unsigned char route_data[60] = {0};
  number(route_data, 7);
  number(route_data + 8, 11);
  memcpy(route_data + 20, route_data, 20);
  BkRoute *route = bk_route_decode(route_data, sizeof(route_data), error);
  assert(route);
  BkPropRoute state = {
      .position = {7, 2, 11}, .velocity = {1, 2, 3}, .cursor = 0, .last = 1};
  BkPropRoute before = state;
  BkPropRouteEffects effects = {9, 8, 7}, saved_effects = effects;
  assert(!bk_prop_route_step(&state, route, 3, NULL, 0, &effects, error));
  assert(!memcmp(&state, &before, sizeof(state)) &&
         !memcmp(&effects, &saved_effects, sizeof(effects)));
  state.last = 64;
  before = state;
  assert(!bk_prop_route_step(&state, route, 1, NULL, 0, &effects, error));
  assert(!memcmp(&state, &before, sizeof(state)));
  bk_route_destroy(route);
  int hit = 42;
  const float a[3] = {4231.72f, 0, -9897.14f}, b[3] = {-9211.88f, 0, 7823.25f};
  assert(bk_segments_intersect_xz(&hit, a, b, a, b) && !hit);
  BkPropState prop = {.kind = 10};
  BkPropShared shared = {.alpha = .75f, .distance = 13};
  BkPropMotionInput input = {.seconds = NAN};
  BkPropMotionEffects out = {.material = 9};
  BkPropState old_prop = prop;
  BkPropShared old_shared = shared;
  assert(!bk_prop_motion_select(&prop, &shared, &input, &out));
  assert(!memcmp(&prop, &old_prop, sizeof(prop)) &&
         !memcmp(&shared, &old_shared, sizeof(shared)) && out.material == 9);
  /* Static XANs are real descriptors, while the strict authored constructor
   * retains its existing active-only contract. No fake duration or clip. */
  unsigned char xan[0x5190] = {0};
  memcpy(xan, "a.x", 4);
  memcpy(xan + 256, "a.x", 4);
  BkClipSet *clips = bk_clip_set_decode(xan, sizeof(xan), error);
  assert(clips);
  assert(!bk_clip_player_create_authored(clips, error));
  BkClipPlayer *player = bk_clip_player_create_loaded(clips, error);
  assert(player);
  BkClipState clock;
  BkClipSample sample;
  assert(bk_clip_state(player, &clock) && clock.slot == 0 && clock.source == 0);
  assert(bk_clip_request(player, 0, error));
  assert(bk_clip_advance(player, 1, &sample, error) && sample.from == 0 &&
         !sample.blend);
  assert(bk_clip_request(player, 1, error));
  assert(bk_clip_state(player, &clock) && clock.rate == 0 &&
         clock.blend_to == 0);
  assert(bk_clip_advance(player, 0, &sample, error) && sample.blend);
  assert(!bk_clip_request(player, 128, error));
  bk_clip_player_destroy(player);
  bk_clip_set_destroy(clips);
  number(xan + 512 + 0x190 + 0x60, NAN);
  clips = bk_clip_set_decode(xan, sizeof(xan), error);
  assert(clips);
  assert(!bk_clip_player_create_loaded(clips, error));
  bk_clip_set_destroy(clips);
  /* A present zero-track ANIM must be exactly its complete72-byte header. */
  unsigned char bytes[76] = {0};
  BkModelChunk chunk = {"ANIM", 0, 72};
  BkModelFrame frame = {.parent_index = BK_MODEL_NONE};
  frame.local[0] = frame.local[5] = frame.local[10] = frame.local[15] = 1;
  BkModel model = {.source = bytes,
                   .source_size = sizeof(bytes),
                   .chunks = &chunk,
                   .chunk_count = 1,
                   .frames = &frame,
                   .frame_count = 1};
  BkModelAnimation *anim = bk_model_animation_create(&model, error);
  assert(anim && !bk_model_animation_track_count(anim));
  float world[16];
  assert(bk_model_animation_sample(anim, 10, 1, world, 16, error));
  assert(!memcmp(world, frame.local, 64));
  bk_model_animation_destroy(anim);
  chunk.size = 76;
  assert(!bk_model_animation_create(&model, error));
  chunk.size = 71;
  assert(!bk_model_animation_create(&model, error));
  puts("PASS prop zero-cycle/bounds atomicity, parallel segments, shared state "
       "rejection, static XAN and empty ANIM");
  return 0;
}
