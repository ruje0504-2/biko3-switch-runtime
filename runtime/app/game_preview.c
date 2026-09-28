#include "app/game_preview.h"
#include "app/capture_output.h"
#include "core/camera.h"
#include "core/matrix.h"
#include "platform/platform.h"
#include "scene/actor_render.h"
#include "scene/area_audio.h"
#include "scene/background_audio.h"
#include "scene/entry_forest.h"
#include "scene/failure_session.h"
#include "scene/game_frame.h"
#include "scene/item_notice_render.h"
#include "scene/lighting_assets.h"
#include "scene/npc_event_audio.h"
#include "scene/player_hud_render.h"
#include "scene/player_hud_session.h"
#include "scene/prop_audio.h"
#include "scene/rain_render.h"
#include "scene/screenshot.h"
#include "scene/system_audio.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {
  BkRenderer *renderer;
  BkResourceStore *resources;
  BkAudio *audio;
  int owns_audio;
  BkAudioSink sink;
  uint64_t submitted;
  BkEntryAssets *entry;
  BkBackgroundAssets *background;
  BkPropAssets *props;
  BkItemAssets *items;
  BkEntryForest *forest;
  BkSystemAudio *system;
  BkSystemAudio *confirm, *click;
  BkSystemAudio *limit;
  BkSystemAudio *shutter;
  BkPlayerAudio *player_audio;
  BkNpcAudio *npc_audio;
  BkNpcEventAudio *npc_events;
  BkAreaAudio *area_audio;
  BkItemFeedback *item_feedback;
  BkBackgroundAudio *background_audio;
  BkPropAudio *prop_audio;
  BkSceneLighting *scene_lighting;
  BkLightSet *lights;
  BkFog fog;
  BkActorRender *background_render;
  BkActorRender *player_render;
  BkActorRender *npc_render;
  BkActorRender *door_render;
  BkActorRender *snow_render;
  BkRainRender *rain_render;
  uint32_t rain_random_before, rain_random_after;
  BkActorRender *player_shadow_render, *npc_shadow_render;
  BkActorRenderVisit *render_visits;
  uint32_t visit_capacity;
  BkActorRender **prop_render;
  BkActorRender **item_render;
  BkActorRenderBatch *world_batch;
  uint32_t prop_count, item_count;
  BkScreenshot *screenshot;
  BkCaptureOutput capture_output;
  BkScreenshotRequest screenshot_request;
  BkItemNoticeRender *notice_render;
  BkPlayerHudRender *hud_render;
  BkItemNoticeState notice;
  BkPulseSprite prompt;
  BkTextFlow text_flow;
  BkDialogueAssets dialogue;
  BkPlayerHudState hud;
  BkPlayerHudFrame hud_frame;
  BkCameraLens lens;
  int hud_ready, opening_ready;
  uint8_t draw_phase;
  BkGameFrameServices services;
  BkGameFrameState local_state, *state;
  BkEntryProgress local_progress;
  const BkEntryProgress *progress;
  uint32_t group, area;
  uint8_t previous_flow, blocked;
  BkGameFrameInput input;
  BkGameFrameResult result;
  float view[16], projection[16];
  BkViewport viewport;
  unsigned width, height;
  uint32_t now_ms;
  double elapsed;
  int ready, failure_loaded, area_suspended;
  BkItemState retained_items[BK_ITEM_LIMIT];
  BkPropSoundState retained_prop_sound[16];
  BkTimer retained_prop_alarm[16];
} GamePreview;
static int submit(void *context, const int16_t *samples, size_t frames,
                  char error[256]) {
  (void)samples;
  (void)error;
  ((GamePreview *)context)->submitted += frames;
  return 1;
}
static int poll(void *context, uint64_t *consumed, char error[256]) {
  (void)error;
  *consumed = ((GamePreview *)context)->submitted;
  return 1;
}
static int capture_none(void *context, int photo, unsigned album,
                        char error[256]) {
  (void)context;
  (void)photo;
  (void)album;
  snprintf(error, 256,
           "game preview: capture root is required for screenshots");
  return 0;
}
static int capture_request(void *context, int photo, unsigned album,
                           char error[256]) {
  GamePreview *g = context;
  if (!g->screenshot)
    return capture_none(context, photo, album, error);
  return bk_screenshot_request_service(&g->screenshot_request, photo, album,
                                       error);
}
static int capture_draw(void *context, char error[256]) {
  GamePreview *g = context;
  if (!g->screenshot)
    return 1;
  BkPlayerHudCapture capture = bk_screenshot_service(g->screenshot);
  return capture.capture(capture.context, error);
}
static int read_clock(void *context, BkCaptureTime *out, char error[256]) {
  (void)context;
  BkCalendarTime t;
  if (!bk_platform_calendar_time(&t, error))
    return 0;
  *out = (BkCaptureTime){t.year,   t.month,  t.day,     t.hour,
                         t.minute, t.second, t.ticks_ms};
  return 1;
}
static void destroy(void *context) {
  GamePreview *g = context;
  bk_player_hud_render_destroy(g->hud_render);
  bk_item_notice_render_destroy(g->notice_render);
  bk_screenshot_destroy(g->screenshot);
  bk_actor_render_batch_destroy(g->world_batch);
  for (uint32_t i = 0; g->prop_render && i < g->prop_count; ++i)
    bk_actor_render_destroy(g->prop_render[i]);
  for (uint32_t i = 0; g->item_render && i < g->item_count; ++i)
    bk_actor_render_destroy(g->item_render[i]);
  free(g->prop_render);
  free(g->item_render);
  bk_dialogue_assets_close(&g->dialogue);
  bk_system_audio_destroy(g->shutter);
  bk_system_audio_destroy(g->limit);
  bk_system_audio_destroy(g->confirm);
  bk_system_audio_destroy(g->click);
  bk_actor_render_destroy(g->npc_render);
  bk_actor_render_destroy(g->player_render);
  bk_actor_render_destroy(g->background_render);
  bk_actor_render_destroy(g->door_render);
  bk_actor_render_destroy(g->snow_render);
  bk_rain_render_destroy(g->rain_render);
  bk_actor_render_destroy(g->player_shadow_render);
  bk_actor_render_destroy(g->npc_shadow_render);
  free(g->render_visits);
  bk_light_set_destroy(g->renderer, g->lights);
  bk_scene_lighting_destroy(g->scene_lighting);
  bk_entry_forest_destroy(g->forest);
  bk_prop_audio_destroy(g->prop_audio);
  bk_background_audio_destroy(g->background_audio);
  bk_item_feedback_destroy(g->item_feedback);
  bk_area_audio_destroy(g->area_audio);
  bk_npc_event_audio_destroy(g->npc_events);
  bk_npc_audio_destroy(g->npc_audio);
  bk_player_audio_destroy(g->player_audio);
  bk_system_audio_destroy(g->system);
  bk_item_assets_destroy(g->items);
  bk_prop_assets_destroy(g->props);
  bk_background_assets_destroy(g->background);
  bk_entry_assets_destroy(g->entry);
  if (g->owns_audio)
    bk_audio_destroy(g->audio);
  free(g);
}
static int world_copy(const BkActorPose *pose, float **out, size_t *floats,
                      char error[256]) {
  const BkModel *model = bk_actor_pose_model(pose);
  float *world = malloc((size_t)model->frame_count * 16 * sizeof(*world));
  if (!world) {
    snprintf(error, 256, "game preview: world allocation failed");
    return 0;
  }
  for (uint32_t i = 0; i < model->frame_count; ++i)
    memcpy(world + (size_t)i * 16, bk_actor_pose_frame(pose, i), 64);
  *out = world;
  *floats = (size_t)model->frame_count * 16;
  return 1;
}
static int refresh_camera(GamePreview *g, char error[256]) {
  const BkCameraFollowPose *active =
      bk_follow_camera_pose(bk_entry_assets_camera(g->entry));
  if (!active || !bk_camera_view(g->view, active->world) ||
      !bk_camera_projection(g->projection, &g->lens) ||
      !bk_camera_fit(&g->viewport, g->width, g->height, 4, 3)) {
    snprintf(error, 256, "game preview: invalid active camera");
    return 0;
  }
  return 1;
}
static int prepare_render(GamePreview *g, const BkFrameVisit *visits,
                          uint32_t visit_count, char error[256]) {
  size_t floats = 0;
  const BkActorPose *bg = bk_background_assets_pose(g->background, 0);
  const float *world = bk_actor_pose_world(bg, &floats);
  if (!world) {
    snprintf(error, 256, "game preview: missing background world cache");
    return 0;
  }
  BkLighting lighting;
  int ok = bk_scene_lighting_values(g->scene_lighting, world, floats, &lighting,
                                    error);
  if (!ok || !refresh_camera(g, error))
    return 0;
  g->screenshot_request.crop = g->viewport;
  const BkCameraFollowPose *active =
      bk_follow_camera_pose(bk_entry_assets_camera(g->entry));
  if (!bk_light_set_update(g->renderer, g->lights, &lighting, error) ||
      !bk_light_set_view(g->renderer, g->lights, active->world + 12, error) ||
      !bk_light_set_fog(g->renderer, g->lights, &g->fog, g->view, error) ||
      !bk_actor_render_prepare(g->background_render, bg,
                               bk_background_assets_materials(g->background, 0),
                               NULL, g->view, g->projection, error) ||
      !bk_actor_render_prepare(g->player_render,
                               bk_entry_assets_player(g->entry),
                               bk_entry_assets_player_materials(g->entry), NULL,
                               g->view, g->projection, error) ||
      !bk_actor_render_prepare(g->npc_render, bk_entry_assets_actor(g->entry),
                               bk_entry_assets_actor_materials(g->entry),
                               bk_entry_assets_face(g->entry), g->view,
                               g->projection, error) ||
      (g->door_render &&
       !bk_actor_render_prepare(
           g->door_render, bk_background_assets_pose(g->background, 1),
           bk_background_assets_materials(g->background, 1), NULL, g->view,
           g->projection, error)))
    return 0;
  if (g->snow_render &&
      !bk_actor_render_prepare(g->snow_render,
                               bk_background_assets_pose(g->background, 2),
                               bk_background_assets_materials(g->background, 2),
                               NULL, g->view, g->projection, error))
    return 0;
  for (uint32_t i = 0; i < g->prop_count; ++i)
    if (!bk_actor_render_prepare(g->prop_render[i],
                                 bk_prop_assets_pose(g->props, i),
                                 bk_prop_assets_materials(g->props, i), NULL,
                                 g->view, g->projection, error))
      return 0;
  for (uint32_t i = 0; i < g->item_count; ++i)
    if (!bk_actor_render_prepare(g->item_render[i],
                                 bk_item_assets_pose(g->items, i), NULL, NULL,
                                 g->view, g->projection, error))
      return 0;
  if (!bk_actor_render_prepare(
          g->player_shadow_render,
          bk_npc_shadow_pose(bk_entry_assets_player_shadow(g->entry)), NULL,
          NULL, g->view, g->projection, error) ||
      !bk_actor_render_prepare(
          g->npc_shadow_render,
          bk_npc_shadow_pose(bk_entry_assets_shadow(g->entry)), NULL, NULL,
          g->view, g->projection, error))
    return 0;
  if (visit_count > g->visit_capacity) {
    snprintf(error, 256, "game preview: forest visit capacity exceeded");
    return 0;
  }
  uint32_t count = 0;
  for (uint32_t i = 0; i < visit_count; ++i) {
    uint32_t object, frame;
    if (!visits[i].submit ||
        !bk_actor_forest_binding(bk_entry_forest_frames(g->forest),
                                 visits[i].node, &object, &frame))
      continue;
    const BkEntryTreeObject *o = bk_entry_forest_object(g->forest, object);
    BkActorRender *actor = NULL;
    switch (o->kind) {
    case BK_ENTRY_TREE_PLAYER:
      actor = g->player_render;
      break;
    case BK_ENTRY_TREE_NPC:
      actor = g->npc_render;
      break;
    case BK_ENTRY_TREE_PLAYER_SHADOW:
      actor = g->player_shadow_render;
      break;
    case BK_ENTRY_TREE_NPC_SHADOW:
      actor = g->npc_shadow_render;
      break;
    case BK_ENTRY_TREE_BACKGROUND:
      actor = g->background_render;
      break;
    case BK_ENTRY_TREE_DOOR:
      actor = g->door_render;
      break;
    case BK_ENTRY_TREE_SNOW:
      actor = g->snow_render;
      break;
    case BK_ENTRY_TREE_PROP:
      actor = g->prop_render[o->index];
      break;
    case BK_ENTRY_TREE_ITEM:
      actor = g->item_render[o->index];
      break;
    case BK_ENTRY_TREE_TRACK:
      continue; /* Authored camera contains no meshes. */
    default:
      snprintf(error, 256, "game preview: unbound render object kind%u",
               o->kind);
      return 0;
    }
    g->render_visits[count++] = (BkActorRenderVisit){actor, frame};
  }
  if (!bk_actor_render_batch_prepare_visits(g->world_batch, g->render_visits,
                                            count, error))
    return 0;
  return 1;
}
/*4fac10 at the overlay boundary, once per simulated native frame. Draw-only
 * presents reuse the snapshot and cannot consume the gameplay RNG. */
static int prepare_rain(GamePreview *g, char error[256]) {
  g->rain_random_before = g->state->random;
  if (!bk_rain_render_prepare(g->rain_render, &g->state->random, 1,
                              g->viewport.width, g->viewport.height, error))
    return 0;
  g->rain_random_after = g->state->random;
  return 1;
}
static uint32_t movement(const BkInput *input) {
  uint32_t buttons = 0;
  if ((input->held & BK_BUTTON_UP) || input->move_y > .25f)
    buttons |= BK_PLAYER_FORWARD;
  if ((input->held & BK_BUTTON_DOWN) || input->move_y < -.25f)
    buttons |= BK_PLAYER_BACKWARD;
  if ((input->held & BK_BUTTON_LEFT) || input->move_x < -.25f)
    buttons |= BK_PLAYER_LEFT;
  if ((input->held & BK_BUTTON_RIGHT) || input->move_x > .25f)
    buttons |= BK_PLAYER_RIGHT;
  if (input->held & BK_BUTTON_SLOW)
    buttons |= BK_PLAYER_SLOW;
  return buttons;
}
static uint32_t hotkeys(const BkInput *input) {
  uint32_t buttons = 0;
  if (input->pressed & BK_BUTTON_PAUSE)
    buttons |= BK_PLAYER_PAUSE;
  if (input->pressed & BK_BUTTON_GAME_CAMERA)
    buttons |= BK_PLAYER_CAMERA;
  if (input->pressed & BK_BUTTON_PHOTO)
    buttons |= BK_PLAYER_PHOTO;
  return buttons;
}
static int step(void *context, double seconds, const BkInput *device,
                char error[256]) {
  GamePreview *g = context;
  if (!g->ready || seconds <= 0 || seconds > 1) {
    snprintf(error, 256, "game preview: invalid step");
    return 0;
  }
  g->elapsed += seconds;
  g->now_ms = (uint32_t)(uint64_t)(g->elapsed * 1000);
  memset(&g->input, 0, sizeof(g->input));
  g->input.seconds = (float)seconds;
  g->input.now_ms = g->now_ms;
  g->input.face_clocks[0] = g->input.face_clocks[1] = g->input.face_clocks[2] =
      g->input.face_clocks[3] = g->now_ms;
  g->input.music_volume = -900;
  g->input.effect_volume = -600;
  g->input.interface_mode = 2;
  g->input.weather_enabled = 1;
  g->input.hud_blocked = g->blocked;
  g->input.movement_buttons = movement(device);
  g->input.hotkey_buttons = hotkeys(device);
  g->input.interaction_buttons =
      ((device->pressed & BK_BUTTON_INTERACT) ? BK_PLAYER_INTERACT : 0) |
      ((device->pressed & BK_BUTTON_STANCE) ? BK_PLAYER_STANCE : 0);
  g->input.cover_buttons =
      ((g->input.movement_buttons & BK_PLAYER_LEFT) ? 1u : 0u) |
      ((g->input.movement_buttons & BK_PLAYER_RIGHT) ? 2u : 0u);
  /* Joy-Con axes are normalized; map full deflection to60 degrees/second. */
  g->input.look[0] = fabsf(device->look_x) > .15f ? device->look_x * 60 : 0;
  g->input.look[1] = fabsf(device->look_y) > .15f ? -device->look_y * 60 : 0;
  BkViewport local = {0, 0, g->viewport.width, g->viewport.height};
  BkScreenPoint projected;
  if (!bk_entry_assets_project_actor_head(g->entry, &projected, g->view,
                                          &g->lens, &local)) {
    snprintf(error, 256,
             "game preview: cached NPC head projection is undefined");
    return 0;
  }
  g->input.player_scene.head_distance = g->state->npc.head.sight.distance;
  g->input.player_scene.projected_depth = projected.depth;
  g->input.player_scene.screen_scale[0] = local.width / 1024.f;
  g->input.player_scene.screen_scale[1] = local.height / 768.f;
  memcpy(g->input.player_scene.screen_position, projected.position,
         sizeof(projected.position));
  if (!bk_player_view_collision_query(&g->state->player_view,
                                      &g->input.player_scene.wall)) {
    snprintf(error, 256, "game preview: invalid retained camera probe");
    return 0;
  }
  if (g->owns_audio && !bk_audio_poll(g->audio, error))
    return 0;
  uint8_t update_phase = g->state->camera.phase;
  if (!bk_scene_game_frame(&g->services, g->state, &g->input, &g->result,
                           error))
    return 0;
  /* Original51a682 invokes4cc320 only for the latched phase0/2 branch.
   * HUD state belongs to this owner; no later collision cleanup changes it. */
  if (update_phase == 0 || update_phase == 2)
    bk_player_hud_reset_reserve(&g->hud);
  const BkFrameVisit *visits;
  uint32_t count;
  if (!bk_entry_forest_draw(g->forest, 0, &visits, &count, error) ||
      !prepare_render(g, visits, count, error))
    return 0;
  if (!prepare_rain(g, error))
    return 0;
  uint8_t old_phase = g->state->camera.phase;
  g->draw_phase = old_phase;
  if (g->opening_ready &&
      (old_phase == 0 || old_phase == 2 || old_phase == 3) &&
      !bk_opening_session_step(
          &(BkOpeningServices){
              g->resources, g->entry, &g->dialogue, g->item_feedback, g->click,
              bk_item_notice_render_text_ops(g->notice_render)},
          g->state, &g->notice, &g->text_flow,
          (device->pressed & BK_BUTTON_CONFIRM) != 0, error))
    return 0;
  if (g->opening_ready &&
      (old_phase == 0 || old_phase == 2 || old_phase == 3) &&
      !bk_item_notice_render_prepare_opening(
          g->notice_render, &g->notice, &g->prompt, old_phase,
          g->state->pickup.notice_visible, &g->text_flow, (float)seconds,
          g->viewport.width, g->viewport.height, error))
    return 0;
  if (g->hud_ready && old_phase == 1 && g->state->camera.phase == 1 &&
      !bk_player_hud_session_step(&g->hud, g->state, 0, g->view, &g->lens,
                                  g->viewport.width, g->viewport.height,
                                  (float)seconds, g->now_ms, &g->hud_frame,
                                  &(int){0}, error))
    return 0;
  if (g->hud_ready && g->state->camera.phase != 1)
    g->hud_frame = (BkPlayerHudFrame){0};
  if (g->hud_ready && old_phase == 1 && g->state->camera.phase == 1 &&
      !bk_player_hud_render_prepare(g->hud_render, &g->hud_frame,
                                    g->viewport.width, g->viewport.height,
                                    error))
    return 0;
  if (g->draw_phase == 1 && !bk_item_notice_render_prepare(
                                g->notice_render, &g->notice, &g->state->pickup,
                                bk_item_feedback_message(g->item_feedback),
                                &g->text_flow, (float)seconds, g->now_ms,
                                g->viewport.width, g->viewport.height, error))
    return 0;
  if (g->owns_audio && !bk_audio_fill(g->audio, error))
    return 0;
  return 1;
}
static int draw(void *context, const BkSceneFrame *frame, char error[256]) {
  (void)frame;
  GamePreview *g = context;
  if (!bk_renderer_viewport(g->renderer, &g->viewport, error) ||
      !bk_actor_render_batch_draw(g->world_batch, g->lights, error) ||
      !bk_rain_render_draw(g->rain_render, error) ||
      (g->failure_loaded &&
       !bk_item_notice_render_draw(g->notice_render, error)) ||
      (!g->failure_loaded && g->opening_ready &&
       (g->draw_phase == 0 || g->draw_phase == 2 || g->draw_phase == 3) &&
       !bk_item_notice_render_draw(g->notice_render, error)) ||
      (!g->failure_loaded && g->hud_ready && g->draw_phase == 1 &&
       !bk_player_hud_render_draw(
           g->hud_render, &(BkPlayerHudCapture){g, capture_draw}, error)) ||
      (!g->failure_loaded && g->draw_phase == 1 &&
       !bk_item_notice_render_draw(g->notice_render, error)) ||
      !bk_renderer_viewport(g->renderer, NULL, error))
    return 0;
  return 1;
}
static int create_world_graphics(GamePreview *g, BkResourceStore *resources,
                                 char error[256]) {
  const BkModel *model =
      bk_actor_pose_model(bk_background_assets_pose(g->background, 0));
  float *world = NULL;
  size_t floats = 0;
  if (!world_copy(bk_background_assets_pose(g->background, 0), &world, &floats,
                  error))
    return 0;
  g->scene_lighting = bk_scene_lighting_create(model, world, floats, error);
  free(world);
  if (!g->scene_lighting)
    return 0;
  /* Main flow2 uses native mode0: one whole-tree flush, all non-ambient
   * lights, and mesh shadows under quality1. Event passes are separate. */
  BkLightingPassInput lighting_input = {.mode = 0, .shadow_mode = 1};
  BkLightingPass pass;
  if (!bk_scene_lighting_input(g->scene_lighting, &lighting_input) ||
      !bk_lighting_pass(&lighting_input, &pass))
    return 0;
  for (unsigned i = 0; i < pass.count; ++i) {
    const BkLightingCommand *command = &pass.commands[i];
    if ((command->kind == BK_PASS_AMBIENT ||
         command->kind == BK_PASS_LIGHT_ENABLE) &&
        !bk_scene_lighting_command(g->scene_lighting, command))
      return 0;
  }
  BkFogState fog_state = {.end = 1, .density = 1};
  const BkModelEnvironment *environment =
      bk_scene_lighting_environment(g->scene_lighting);
  if (!bk_fog_enable(&fog_state, 0, 0, 1, 1) ||
      !bk_fog_load(&fog_state, &environment->fog, 1, 1, error) ||
      !bk_fog_resolve(&fog_state, &g->fog, error))
    return 0;
  BkLighting lighting;
  if (!world_copy(bk_background_assets_pose(g->background, 0), &world, &floats,
                  error))
    return 0;
  int values = bk_scene_lighting_values(g->scene_lighting, world, floats,
                                        &lighting, error);
  free(world);
  if (!values ||
      !(g->lights = bk_light_set_create(g->renderer, &lighting, error)) ||
      !(g->background_render = bk_actor_render_create(
            g->renderer, resources, "bk3_03", model, NULL, error)))
    return 0;
  g->world_batch = bk_actor_render_batch_create(g->renderer, 16384, error);
  if (!g->world_batch)
    return 0;
  g->visit_capacity = bk_frame_tree_count(
      bk_actor_forest_tree(bk_entry_forest_frames(g->forest)));
  g->render_visits = calloc(g->visit_capacity, sizeof(*g->render_visits));
  if (!g->render_visits)
    return 0;
  const BkActorPose *door = bk_background_assets_pose(g->background, 1);
  if (door && !(g->door_render = bk_actor_render_create(
                    g->renderer, resources, "bk3_03", bk_actor_pose_model(door),
                    NULL, error)))
    return 0;
  const BkActorPose *snow = bk_background_assets_pose(g->background, 2);
  if ((snow && !(g->snow_render = bk_actor_render_create(
                     g->renderer, resources, "bk3_20",
                     bk_actor_pose_model(snow), NULL, error))) ||
      !(g->rain_render = bk_rain_render_create(
            g->renderer, resources, g->state->group, g->state->area, 1, error)))
    return 0;
  g->prop_count = bk_prop_assets_count(g->props);
  g->item_count = bk_item_assets_count(g->items);
  g->prop_render =
      calloc(g->prop_count ? g->prop_count : 1, sizeof(*g->prop_render));
  g->item_render =
      calloc(g->item_count ? g->item_count : 1, sizeof(*g->item_render));
  if (!g->prop_render || !g->item_render) {
    snprintf(error, 256, "game preview: world render allocation failed");
    return 0;
  }
  for (uint32_t i = 0; i < g->prop_count; ++i)
    if (!(g->prop_render[i] = bk_actor_render_create(
              g->renderer, resources, "bk3_07",
              bk_actor_pose_model(bk_prop_assets_pose(g->props, i)), NULL,
              error)))
      return 0;
  for (uint32_t i = 0; i < g->item_count; ++i)
    if (!(g->item_render[i] = bk_actor_render_create(
              g->renderer, resources, "bk3_16",
              bk_actor_pose_model(bk_item_assets_pose(g->items, i)), NULL,
              error)))
      return 0;
  return 1;
}
static int load(GamePreview *g, const BkSceneServices *services,
                char error[256]) {
  BkEntryRequest request;
  uint32_t route_start;
  if (!bk_game_entry_resolve(&request, &route_start, g->progress, g->group,
                             g->area, g->previous_flow) ||
      !(g->entry =
            bk_entry_assets_create(services->resources, &request, error)))
    return 0;
  uint32_t now = (uint32_t)(uint64_t)(g->elapsed * 1000);
  uint32_t clocks[4] = {now, now, now, now};
  if (!bk_scene_game_frame_initialize_entry(g->entry, g->state, route_start,
                                            clocks, error) ||
      !bk_entry_assets_load_player_shadow(g->entry, services->resources,
                                          error) ||
      !bk_entry_assets_load_mesh_shadow(g->entry, services->resources, error) ||
      !(g->background = bk_background_assets_create(
            services->resources, request.group, request.area, 1, 1, error)))
    return 0;
  bk_background_assets_publish(g->background);
  float *world = NULL;
  size_t floats = 0;
  if (!world_copy(bk_background_assets_pose(g->background, 0), &world, &floats,
                  error))
    return 0;
  const BkModel *model =
      bk_actor_pose_model(bk_background_assets_pose(g->background, 0));
  g->props = bk_prop_assets_create(services->resources, request.group,
                                   request.area, model, world, floats, error);
  free(world);
  if (!g->props ||
      !(g->items =
            bk_item_assets_create(services->resources, request.group,
                                  request.area, g->state->pickup.collected,
                                  (BkItemState[BK_ITEM_LIMIT]){0}, error)) ||
      !(g->forest = bk_entry_forest_create(g->entry, g->background, g->props,
                                           g->items, error)))
    return 0;
  g->audio = services->audio;
  if (!g->audio) {
    g->sink = (BkAudioSink){g, 48000, 240, 960, submit, poll};
    g->audio = bk_audio_create(&g->sink, error);
    g->owns_audio = 1;
  }
  if (!g->audio ||
      !(g->system = bk_system_audio_create(services->resources, g->audio, 0,
                                           -600, error)) ||
      !(g->limit = bk_system_audio_create_slot(services->resources, g->audio,
                                               34, 5, -600, error)) ||
      !(g->shutter = bk_system_audio_create_slot(services->resources, g->audio,
                                                 35, 7, -600, error)) ||
      !(g->confirm = bk_system_audio_create_slot(services->resources, g->audio,
                                                 33, 0, -600, error)) ||
      !(g->click = bk_system_audio_create_slot(services->resources, g->audio,
                                               36, 4, -600, error)) ||
      !(g->player_audio =
            bk_player_audio_create(services->resources, g->audio, 1, error)) ||
      !(g->npc_audio =
            bk_npc_audio_create(services->resources, g->audio, 2, 3, error)) ||
      !(g->area_audio = bk_area_audio_create(services->resources, g->audio, 4,
                                             g->player_audio, error)) ||
      !(g->npc_events =
            bk_npc_event_audio_create(services->resources, g->audio, 5, 6, -600,
                                      g->area_audio, g->system, error)) ||
      !(g->item_feedback = bk_item_feedback_create(services->resources,
                                                   g->audio, 7, -600, error)) ||
      !(g->background_audio = bk_background_audio_create(
            services->resources, g->audio, 8,
            bk_background_assets_config(g->background), &g->state->background,
            -600, error)) ||
      !(g->prop_audio = bk_prop_audio_create(services->resources, g->audio, 17,
                                             g->props, NULL, 0, -600, error)))
    return 0;
  g->services = (BkGameFrameServices){
      .entry = g->entry,
      .background = g->background,
      .props = g->props,
      .items = g->items,
      .background_audio = g->background_audio,
      .player_audio = g->player_audio,
      .npc_audio = g->npc_audio,
      .npc_events = g->npc_events,
      .prop_audio = g->prop_audio,
      .area_audio = g->area_audio,
      .item_feedback = g->item_feedback,
      .hotkeys = {g->confirm, g->limit, g->shutter, g, capture_request}};
  if (!create_world_graphics(g, services->resources, error) ||
      !(g->player_render = bk_actor_render_create(
            g->renderer, services->resources, "bk3_01",
            bk_actor_pose_model(bk_entry_assets_player(g->entry)), NULL,
            error)) ||
      !(g->npc_render = bk_actor_render_create(
            g->renderer, services->resources, "bk3_01",
            bk_actor_pose_model(bk_entry_assets_actor(g->entry)),
            bk_entry_assets_eyes(g->entry), error)))
    return 0;
  bk_renderer_extent(g->renderer, &g->width, &g->height);
  if (!bk_camera_fit(&g->viewport, g->width, g->height, 4, 3))
    return 0;
  if (!(g->player_shadow_render = bk_actor_render_create(
            g->renderer, services->resources, "bk3_01",
            bk_actor_pose_model(
                bk_npc_shadow_pose(bk_entry_assets_player_shadow(g->entry))),
            NULL, error)) ||
      !(g->npc_shadow_render =
            bk_actor_render_create(g->renderer, services->resources, "bk3_01",
                                   bk_actor_pose_model(bk_npc_shadow_pose(
                                       bk_entry_assets_shadow(g->entry))),
                                   NULL, error)))
    return 0;
  if (services->capture_files) {
    g->capture_output = (BkCaptureOutput){services->capture_files,
                                          (BkCaptureClock){g, read_clock}};
    BkScreenshotOutput output = bk_capture_output_service(&g->capture_output);
    g->screenshot =
        bk_screenshot_create(g->renderer, services->resources, &output, error);
    if (!g->screenshot)
      return 0;
  }
  if (!(g->notice_render = bk_item_notice_render_create(
            g->renderer, services->resources, request.group, error)) ||
      !(g->hud_render = bk_player_hud_render_create(
            g->renderer, services->resources, request.group, 0, error)) ||
      !bk_item_notice_initialize(&g->notice, 0))
    return 0;
  bk_pulse_sprite_initialize(&g->prompt);
  g->lens = (BkCameraLens){1, .75f, .5f, 126384};
  if (!bk_player_hud_initialize(&g->hud, request.group, 0, g->viewport.width) ||
      !bk_opening_session_initialize(
          &(BkOpeningServices){
              g->resources, g->entry, &g->dialogue, g->item_feedback, g->click,
              bk_item_notice_render_text_ops(g->notice_render)},
          g->state, &g->notice, &g->text_flow, error))
    return 0;
  g->screenshot_request.screenshot = g->screenshot;
  g->hud_ready = 1;
  g->opening_ready = 1;
  if (!refresh_camera(g, error))
    return 0;
  g->ready = 1;
  return 1;
}
BkScene *bk_game_preview_create_entry(const BkSceneServices *services,
                                      BkGameFrameState *retained,
                                      const BkEntryProgress *progress,
                                      uint32_t group, uint32_t area,
                                      uint8_t previous_flow, double elapsed,
                                      char error[256]) {
  if (!services || !services->renderer || !services->resources ||
      !isfinite(elapsed) || elapsed < 0 || elapsed > 1e12) {
    snprintf(error, 256, "game preview: incomplete services");
    return NULL;
  }
  GamePreview *g = calloc(1, sizeof(*g));
  if (!g) {
    snprintf(error, 256, "game preview: allocation failed");
    return NULL;
  }
  g->state = retained ? retained : &g->local_state;
  if (!retained && !bk_scene_game_frame_boot_state(g->state, &g->local_progress,
                                                   (uint32_t)time(NULL))) {
    destroy(g);
    return NULL;
  }
  if (retained && !progress) {
    snprintf(error, 256, "game preview: retained entry requires progress");
    destroy(g);
    return NULL;
  }
  g->progress = retained ? progress : &g->local_progress;
  g->group = group;
  g->area = area;
  g->previous_flow = previous_flow;
  g->elapsed = elapsed;
  g->renderer = services->renderer;
  g->resources = services->resources;
  if (!load(g, services, error)) {
    destroy(g);
    return NULL;
  }
  BkInput initial = {0};
  if (!retained && !step(g, 1.0 / 60, &initial, error)) {
    destroy(g);
    return NULL;
  }
  BkScene *scene =
      bk_scene_custom_create(g, (BkSceneCustomOps){step, draw, destroy}, error);
  if (!scene)
    destroy(g);
  return scene;
}
const BkGameFrameState *bk_game_preview_state(BkScene *scene) {
  const GamePreview *g = bk_scene_custom_context(scene);
  return g ? g->state : NULL;
}
BkAudio *bk_game_preview_audio(BkScene *scene) {
  GamePreview *g = bk_scene_custom_context(scene);
  return g ? g->audio : NULL;
}
int bk_game_preview_resume(BkScene *scene, char error[256]) {
  GamePreview *g = bk_scene_custom_context(scene);
  if (!g || !g->ready || g->state->hotkeys.menu_request != 1) {
    snprintf(error, 256, "game preview: no paused game to resume");
    return 0;
  }
  g->state->hotkeys.menu_request = 0;
  return 1;
}

BkScene *bk_game_preview_create(const BkSceneServices *services,
                                char error[256]) {
  return bk_game_preview_create_entry(services, NULL, NULL, 0, 0, 8, 1, error);
}
void bk_game_preview_block(BkScene *scene, uint8_t blocked) {
  GamePreview *g = bk_scene_custom_context(scene);
  if (g)
    g->blocked = blocked;
}
uint32_t bk_game_preview_now(BkScene *scene) {
  GamePreview *g = bk_scene_custom_context(scene);
  return g ? g->now_ms : 0;
}

void bk_game_preview_clock(BkScene *scene, double elapsed) {
  GamePreview *g = bk_scene_custom_context(scene);
  if (g)
    g->elapsed = elapsed;
}

int bk_game_preview_background_step(BkScene *scene, double seconds,
                                    double elapsed, char error[256]) {
  GamePreview *g = bk_scene_custom_context(scene);
  if (!g || !g->ready || !isfinite(seconds) || seconds <= 0 || seconds > 1 ||
      !isfinite(elapsed) || elapsed < 0 || elapsed > 1e12) {
    snprintf(error, 256, "game preview: invalid background-only step");
    return 0;
  }
  g->elapsed = elapsed;
  g->now_ms = (uint32_t)(uint64_t)(elapsed * 1000);
  BkGameFrameState *s = g->state;
  BkBackgroundInput input = {.now = g->now_ms,
                             .seconds = (float)seconds,
                             .group = (int32_t)s->group,
                             .area = (int32_t)s->area,
                             .music_master = -900,
                             .effect_master = -600,
                             .player_yaw = s->player.spatial.movement.yaw,
                             .weather_enabled = 1,
                             .ambient_gate = (int8_t)s->boundary.ambient_gate};
  memcpy(input.player, s->player.spatial.movement.position,
         sizeof(input.player));
  memcpy(input.npc, s->npc.path.position, sizeof(input.npc));
  return bk_background_audio_pause_step(g->background_audio, &s->background,
                                        &input, error);
}

int bk_game_preview_restore_item_text(BkScene *scene, char error[256]) {
  GamePreview *g = bk_scene_custom_context(scene);
  if (!g || !g->ready) {
    snprintf(error, 256, "game preview: missing retained item services");
    return 0;
  }
  BkNoticeTextOps text = bk_item_notice_render_text_ops(g->notice_render);
  if (!bk_item_feedback_reload_message(g->item_feedback, g->resources, error) ||
      !text.recreate(text.context, BK_NOTICE_TEXT_ITEMS, error))
    return 0;
  g->text_flow.started = 0;
  g->text_flow.enabled = 1;
  return 1;
}
static BkFailureServices failure_services(GamePreview *g) {
  return (BkFailureServices){g->resources,
                             g->entry,
                             g->npc_audio,
                             g->player_audio,
                             &g->dialogue,
                             g->item_feedback,
                             bk_item_notice_render_text_ops(g->notice_render),
                             -600};
}
int bk_game_preview_load_failure(BkScene *scene, char error[256]) {
  GamePreview *g = bk_scene_custom_context(scene);
  if (!g || !g->ready || g->failure_loaded) {
    snprintf(error, 256, "game preview: invalid failure reload");
    return 0;
  }
  /* Drop the batch before replacing any mesh and detach borrowing tree before
   * rebuilding the player CPU pose. The old immutable model is still owned. */
  bk_actor_render_batch_destroy(g->world_batch);
  g->world_batch = NULL;
  bk_entry_forest_destroy(g->forest);
  g->forest = NULL;
  bk_actor_render_destroy(g->player_render);
  g->player_render = NULL;
  bk_actor_render_destroy(g->player_shadow_render);
  g->player_shadow_render = NULL;
  BkFailureServices services = failure_services(g);
  if (!bk_failure_session_load(&services, g->state, &g->text_flow, error))
    return 0;
  g->forest = bk_entry_forest_create_failure(g->entry, g->background, g->props,
                                             g->items, error);
  if (!g->forest)
    return 0;
  g->player_render = bk_actor_render_create(
      g->renderer, g->resources, "bk3_01",
      bk_actor_pose_model(bk_entry_assets_player(g->entry)), NULL, error);
  g->player_shadow_render =
      bk_actor_render_create(g->renderer, g->resources, "bk3_01",
                             bk_actor_pose_model(bk_npc_shadow_pose(
                                 bk_entry_assets_player_shadow(g->entry))),
                             NULL, error);
  g->world_batch = bk_actor_render_batch_create(g->renderer, 16384, error);
  if (!g->player_render || !g->player_shadow_render || !g->world_batch)
    return 0;
  g->failure_loaded = 1;
  return 1;
}
int bk_game_preview_release_failure_audio(BkScene *scene, char error[256]) {
  GamePreview *g = bk_scene_custom_context(scene);
  if (!g || !g->failure_loaded) {
    snprintf(error, 256, "game preview: no failure resources");
    return 0;
  }
  /*4eb851. Font/dialogue remain valid for the already prepared snapshot and
   * are destroyed together with flow2 at the next frame boundary. */
  return bk_npc_audio_release_speech(g->npc_audio, error);
}
typedef struct {
  GamePreview *game;
  const BkFailureHudOps *outer;
} FailureCallbacks;
static int failure_click(void *context, char error[256]) {
  return bk_system_audio_restart(((FailureCallbacks *)context)->game->click,
                                 error);
}
static int failure_message(void *context, uint32_t label, char error[256]) {
  BkFailureServices services =
      failure_services(((FailureCallbacks *)context)->game);
  return bk_failure_session_message(&services, label, error);
}
static int failure_release(void *context, uint8_t flow, char error[256]) {
  const BkFailureHudOps *o = ((FailureCallbacks *)context)->outer;
  return o->release(o->context, flow, error);
}
static int failure_schedule(void *context, uint8_t target, uint8_t mode,
                            char error[256]) {
  const BkFailureHudOps *o = ((FailureCallbacks *)context)->outer;
  return o->schedule(o->context, target, mode, error);
}
int bk_game_preview_failure_step(BkScene *scene, BkFailureHudState *failure,
                                 BkCommonHudState *common, uint8_t *overlay,
                                 const BkFailureHudOps *flow_ops,
                                 double seconds, double elapsed,
                                 const BkInput *device, BkFailureHudFrame *out,
                                 char error[256]) {
  GamePreview *g = bk_scene_custom_context(scene);
  if (!g || !g->ready || !g->failure_loaded || !failure || !common ||
      !overlay || !flow_ops || !flow_ops->release || !flow_ops->schedule ||
      !device || !out || !isfinite(seconds) || seconds <= 0 || seconds > 1 ||
      !isfinite(elapsed) || elapsed < 0 || elapsed > 1e12) {
    snprintf(error, 256, "game preview: invalid failure step");
    return 0;
  }
  g->elapsed = elapsed;
  g->now_ms = (uint32_t)(uint64_t)(elapsed * 1000);
  BkGameFrameInput input = {.seconds = (float)seconds,
                            .now_ms = g->now_ms,
                            .music_volume = -900,
                            .effect_volume = -600,
                            .interface_mode = 0x40,
                            .weather_enabled = 1};
  for (unsigned i = 0; i < 4; ++i)
    input.face_clocks[i] = g->now_ms;
  if (!bk_scene_failure_frame(&g->services, g->state, &input, &failure->visible,
                              &g->lens, &g->result, error))
    return 0;
  const BkFrameVisit *visits;
  uint32_t count;
  if (!bk_entry_forest_draw(g->forest, 0, &visits, &count, error) ||
      !prepare_render(g, visits, count, error))
    return 0;
  BkGameFrameState *s = g->state;
  BkFailureHudBindings bindings = {common,
                                   &g->notice.panel,
                                   &g->prompt.fade,
                                   &g->text_flow,
                                   &s->group,
                                   &s->interaction.outcome,
                                   overlay,
                                   &s->npc.visible,
                                   &s->player.interaction.script_phase,
                                   &s->player.completion_mode,
                                   &s->npc.ai.stimulus,
                                   &s->npc.ai.point.motion.behavior,
                                   &s->npc.ai.point.action_wait.duration};
  FailureCallbacks context = {g, flow_ops};
  BkFailureHudOps ops = {&context, failure_click, failure_message,
                         failure_release, failure_schedule};
  if (!bk_failure_hud_step(failure, &bindings, &ops,
                           !!(device->pressed & BK_BUTTON_CONFIRM),
                           (float)seconds, out, error))
    return 0;
  s->player.completion_requested = (int8_t)s->interaction.outcome;
  if (!prepare_rain(g, error))
    return 0;
  return bk_item_notice_render_prepare_failure(
      g->notice_render, &g->notice, &g->prompt, out, &g->text_flow,
      (float)seconds, g->viewport.width, g->viewport.height, error);
}

float *bk_game_preview_hud_reserve(BkScene *scene) {
  GamePreview *g = bk_scene_custom_context(scene);
  return g ? &g->hud.reserve : NULL;
}
int bk_game_preview_suspend_area(BkScene *scene, char error[256]) {
  GamePreview *g = bk_scene_custom_context(scene);
  if (!g || !g->ready || g->area_suspended || g->failure_loaded) {
    snprintf(error, 256, "game area: invalid suspension state");
    return 0;
  }
  if (!bk_item_assets_snapshot(g->items, g->retained_items))
    return 0;
  for (uint32_t i = 0; i < g->prop_count; ++i) {
    BkPropInteractionActor actor;
    const BkPropSoundState *sound = bk_prop_audio_state(g->prop_audio, i);
    if (!sound || !bk_prop_assets_bind_interaction(g->props, i, &actor))
      return 0;
    g->retained_prop_sound[i] = *sound;
    g->retained_prop_alarm[i] = actor.interaction->alarm;
  }
  /* Last game snapshot was already presented before flow50 loaded20. */
  g->ready = 0;
  bk_actor_render_batch_destroy(g->world_batch);
  g->world_batch = NULL;
  bk_entry_forest_destroy(g->forest);
  g->forest = NULL;
  free(g->render_visits);
  g->render_visits = NULL;
  g->visit_capacity = 0;
  BkNoticeTextOps text = bk_item_notice_render_text_ops(g->notice_render);
  if (!text.clear(text.context, error))
    return 0;
  bk_dialogue_assets_close(&g->dialogue);
  bk_item_feedback_close_message(g->item_feedback);
  if (!bk_item_feedback_release_sound(g->item_feedback, error))
    return 0;
  for (uint32_t i = 0; i < g->item_count; ++i)
    bk_actor_render_destroy(g->item_render[i]);
  free(g->item_render);
  g->item_render = NULL;
  g->item_count = 0;
  bk_item_assets_destroy(g->items);
  g->items = NULL;
  if (!bk_prop_audio_stop(g->prop_audio, error))
    return 0;
  bk_prop_audio_destroy(g->prop_audio);
  g->prop_audio = NULL;
  for (uint32_t i = 0; i < g->prop_count; ++i)
    bk_actor_render_destroy(g->prop_render[i]);
  free(g->prop_render);
  g->prop_render = NULL;
  g->prop_count = 0;
  bk_prop_assets_destroy(g->props);
  g->props = NULL;
  if (!bk_background_audio_stop(g->background_audio, error))
    return 0;
  bk_background_audio_destroy(g->background_audio);
  g->background_audio = NULL;
  bk_actor_render_destroy(g->background_render);
  g->background_render = NULL;
  bk_actor_render_destroy(g->door_render);
  g->door_render = NULL;
  bk_actor_render_destroy(g->snow_render);
  g->snow_render = NULL;
  bk_rain_render_destroy(g->rain_render);
  g->rain_render = NULL;
  bk_light_set_destroy(g->renderer, g->lights);
  g->lights = NULL;
  bk_scene_lighting_destroy(g->scene_lighting);
  g->scene_lighting = NULL;
  bk_background_assets_destroy(g->background);
  g->background = NULL;
  bk_entry_assets_unload_track(g->entry);
  g->services.background = NULL;
  g->services.props = NULL;
  g->services.items = NULL;
  g->services.background_audio = NULL;
  g->services.prop_audio = NULL;
  g->area_suspended = 1;
  return 1;
}
int bk_game_preview_advance_area(BkScene *scene, uint8_t previous_flow,
                                 char error[256]) {
  GamePreview *g = bk_scene_custom_context(scene);
  if (!g || !g->area_suspended || g->ready || g->state->area >= 8) {
    snprintf(error, 256, "game area: invalid retained entry");
    return 0;
  }
  BkGameFrameState *s = g->state;
  BkEntryRequest next = {s->group, s->area + 1, 0, previous_flow};
  if (!bk_entry_assets_advance_area(g->entry, g->resources, &next, &s->player,
                                    &s->npc, &s->npc_vertical, &s->player_view,
                                    &s->camera, &s->npc_entry_route,
                                    &s->npc_actions, error))
    return 0;
  s->area = next.area;
  g->area = next.area;
  g->previous_flow = previous_flow;
  g->background = bk_background_assets_create(g->resources, next.group,
                                              next.area, 1, 1, error);
  if (!g->background)
    return 0;
  bk_background_assets_publish(g->background);
  float *world = NULL;
  size_t floats = 0;
  const BkModel *model =
      bk_actor_pose_model(bk_background_assets_pose(g->background, 0));
  if (!world_copy(bk_background_assets_pose(g->background, 0), &world, &floats,
                  error))
    return 0;
  g->props = bk_prop_assets_create(g->resources, next.group, next.area, model,
                                   world, floats, error);
  free(world);
  if (!g->props)
    return 0;
  for (uint32_t i = 0; i < bk_prop_assets_count(g->props); ++i) {
    BkPropInteractionActor actor;
    if (!bk_prop_assets_bind_interaction(g->props, i, &actor))
      return 0;
    actor.interaction->alarm = g->retained_prop_alarm[i];
  }
  g->items =
      bk_item_assets_create(g->resources, next.group, next.area,
                            s->pickup.collected, g->retained_items, error);
  if (!g->items ||
      !bk_item_feedback_reload_sound(g->item_feedback, g->resources, error) ||
      !bk_item_feedback_reload_message(g->item_feedback, g->resources, error) ||
      !bk_item_notice_render_reload(g->notice_render, g->resources, next.group,
                                    0, error) ||
      !bk_item_notice_initialize(&g->notice, 0))
    return 0;
  g->background_audio = bk_background_audio_create(
      g->resources, g->audio, 8, bk_background_assets_config(g->background),
      &s->background, -600, error);
  g->prop_audio = bk_prop_audio_create(g->resources, g->audio, 17, g->props,
                                       g->retained_prop_sound, 16, -600, error);
  g->forest = bk_entry_forest_create(g->entry, g->background, g->props,
                                     g->items, error);
  if (!g->background_audio || !g->prop_audio || !g->forest ||
      !create_world_graphics(g, g->resources, error))
    return 0;
  g->services.background = g->background;
  g->services.props = g->props;
  g->services.items = g->items;
  g->services.background_audio = g->background_audio;
  g->services.prop_audio = g->prop_audio;
  if (!bk_opening_session_initialize(
          &(BkOpeningServices){
              g->resources, g->entry, &g->dialogue, g->item_feedback, g->click,
              bk_item_notice_render_text_ops(g->notice_render)},
          s, &g->notice, &g->text_flow, error) ||
      !refresh_camera(g, error))
    return 0;
  g->area_suspended = 0;
  g->ready = 1;
  return 1;
}

int bk_game_preview_weather(BkScene *scene, BkGameWeatherSnapshot *out) {
  GamePreview *g = bk_scene_custom_context(scene);
  if (!g || !out || !g->ready || !g->rain_render)
    return 0;
  *out = (BkGameWeatherSnapshot){0};
  out->rain = *bk_rain_render_state(g->rain_render);
  const BkRainDraw *draw = bk_rain_render_snapshot(g->rain_render);
  if (draw)
    out->rain_draw = *draw;
  out->random_before = g->rain_random_before;
  out->random_after = g->rain_random_after;
  const BkActorPose *snow = bk_background_assets_pose(g->background, 2);
  out->snow_present = snow != NULL;
  if (snow) {
    BkActorRenderStats stats;
    if (!bk_actor_pose_state(snow, &out->snow_clock) ||
        !bk_actor_render_stats(g->snow_render, &stats))
      return 0;
    out->snow_instances = stats.submitted_instances;
  }
  return 1;
}
