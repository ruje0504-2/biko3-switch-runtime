#include "scene/actor_render.h"
#include "scene/entry_assets.h"
#include "scene/inspection_camera.h"
#include "scene/npc_audio.h"
#include "scene/scene_internal.h"
#include "ui/debug_overlay.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  BkScene base;
  BkRenderer *renderer;
  BkEntryAssets *entry;
  BkActorRender *actor;
  BkLightSet *lights;
  BkTexture *overlay;
  BkInspectionCamera camera, initial_camera;
  BkFaceState face;
  BkNpcSpatialState npc;
  BkNpcAudio *audio;
  BkVoiceEnvelope envelope;
  uint32_t random;
  double elapsed;
} ActorPreview;
static void destroy(BkScene *base) {
  ActorPreview *s = (ActorPreview *)base;
  bk_npc_audio_destroy(s->audio);
  bk_actor_render_destroy(s->actor);
  bk_entry_assets_destroy(s->entry);
  bk_light_set_destroy(s->renderer, s->lights);
  bk_texture_destroy(s->renderer, s->overlay);
  free(s);
}
static int prepare(ActorPreview *s, char error[256]) {
  float camera[16], view[16], projection[16];
  BkCameraLens lens = {1, .75f, .5f, 126384};
  if (!bk_inspection_world(&s->camera, camera) ||
      !bk_camera_view(view, camera) ||
      !bk_camera_projection(projection, &lens)) {
    snprintf(error, 256, "actor preview camera is invalid");
    return 0;
  }
  return bk_light_set_view(s->renderer, s->lights, camera + 12, error) &&
         bk_actor_render_prepare(s->actor, bk_entry_assets_actor(s->entry),
                                 bk_entry_assets_actor_materials(s->entry),
                                 bk_entry_assets_face(s->entry), view,
                                 projection, error);
}
static int step(BkScene *base, double seconds, const BkInput *input,
                char error[256]) {
  ActorPreview *s = (ActorPreview *)base;
  if (!isfinite(seconds) || seconds <= 0 || seconds > 1) {
    snprintf(error, 256, "actor preview step is invalid");
    return 0;
  }
  BkInput camera_input = *input;
  if (input->pressed & BK_BUTTON_CONFIRM)
    s->camera = s->initial_camera;
  camera_input.pressed &= ~BK_BUTTON_CONFIRM;
  if (!bk_inspection_step(&s->camera, seconds, &camera_input, error))
    return 0;
  BkActorPose *pose = bk_entry_assets_actor(s->entry);
  s->elapsed += seconds;
  uint32_t now = (uint32_t)fmod(1000 + s->elapsed * 1000, 4294967296.0);
  if (input->pressed & BK_BUTTON_AUDIO_PREVIEW) {
    if (!s->audio) {
      snprintf(error, 256, "voice audition requires an audio sink");
      return 0;
    }
    /* Explicit resource audition, not a game dialogue selection. */
    if (!bk_npc_audio_speak(s->audio, "bk3_06", "PH10101.wav", 0, 0, error))
      return 0;
  }
  float voice = 0;
  if (s->audio && !bk_npc_audio_voice(s->audio, &s->envelope, (float)seconds,
                                      &voice, error))
    return 0;
  BkEntryNpcPresentation input_frame = {.seconds = (float)seconds,
                                        .voice_level = voice,
                                        .interface_mode = 2,
                                        .phase = 0,
                                        .timestamp_ms = now,
                                        .request_clock_ms = now,
                                        .mouth_clock_ms = now,
                                        .blink_clock_ms = now};
  if (!bk_entry_assets_step_npc_presentation(s->entry, &s->npc, &s->face,
                                             &s->random, &input_frame, error))
    return 0;
  bk_actor_pose_publish(pose);
  return prepare(s, error);
}
static int draw(BkScene *base, const BkSceneFrame *frame, char error[256]) {
  (void)frame;
  ActorPreview *s = (ActorPreview *)base;
  unsigned width, height;
  BkViewport viewport;
  bk_renderer_extent(s->renderer, &width, &height);
  if (!bk_camera_fit(&viewport, width, height, 4, 3)) {
    snprintf(error, 256, "actor preview viewport is invalid");
    return 0;
  }
  if (!bk_renderer_viewport(s->renderer, &viewport, error) ||
      !bk_actor_render_draw(s->actor, s->lights, error) ||
      !bk_renderer_viewport(s->renderer, NULL, error))
    return 0;
  const float y = 656.0f / 360 - 1;
  const BkVertex v[] = {
      {-1, y, 0, 0, 0, 1, 1, 1, 1}, {1, y, 0, 1, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},  {-1, y, 0, 0, 0, 1, 1, 1, 1},
      {1, 1, 0, 1, 1, 1, 1, 1, 1},  {-1, 1, 0, 0, 1, 1, 1, 1, 1}};
  return bk_renderer_draw(s->renderer, s->overlay, v, 6, bk_identity, error);
}
BkScene *bk_actor_preview_create(const BkSceneServices *services,
                                 char error[256]) {
  ActorPreview *s = calloc(1, sizeof(*s));
  BkImage banner = {0};
  if (!s) {
    snprintf(error, 256, "actor preview allocation failed");
    return NULL;
  }
  s->base = (BkScene){step, draw, destroy};
  s->renderer = services->renderer;
  BkEntryRequest request = {0, 8, 0, 8};
  s->entry = bk_entry_assets_create(services->resources, &request, error);
  if (!s->entry)
    goto fail;
  if (services->audio) {
    s->audio =
        bk_npc_audio_create(services->resources, services->audio, 0, 1, error);
    if (!s->audio)
      goto fail;
  }
  BkActorPose *pose = bk_entry_assets_actor(s->entry);
  s->actor = bk_actor_render_create(s->renderer, services->resources, "bk3_01",
                                    bk_actor_pose_model(pose),
                                    bk_entry_assets_eyes(s->entry), error);
  if (!s->actor)
    goto fail;
  s->npc.alpha = 1;
  s->npc.ai.point.motion.action = 1;
  if (!bk_entry_assets_step_npc_visibility(s->entry, &s->npc, 2, 0, error))
    goto fail;
  uint32_t clocks[] = {1000, 1000, 1000, 1000};
  s->random = 1;
  if (!bk_face_assets_initialize(bk_entry_assets_face(s->entry), &s->face,
                                 clocks, &s->random, error))
    goto fail;
  bk_actor_pose_publish(pose);
  const BkActorPlacement *placement = bk_actor_pose_placement(pose);
  float height = bk_actor_pose_head(pose)[1] - placement->position[1];
  if (!isfinite(height) || height < 1) {
    snprintf(error, 256, "actor preview height is invalid");
    goto fail;
  }
  memcpy(s->camera.eye, placement->position, sizeof(s->camera.eye));
  s->camera.eye[1] += height * .5f;
  s->camera.eye[2] -= height * 1.6f;
  s->initial_camera = s->camera;
  /* Explicit inspection light rig. Game pass selection remains separate. */
  BkLighting lighting = {.ambient = {.6f, .6f, .6f}, .point_count = 1};
  lighting.points[0] = (BkPointLight){.range = height * 10,
                                      .attenuation0 = 1,
                                      .diffuse = {.4f, .4f, .4f},
                                      .specular = {.25f, .25f, .25f}};
  memcpy(lighting.points[0].position, s->camera.eye, sizeof(s->camera.eye));
  s->lights = bk_light_set_create(s->renderer, &lighting, error);
  if (!s->lights || !prepare(s, error))
    goto fail;
  if (!bk_debug_banner_lines(
          &banner, "BIKO3 - ACTOR ANIMATION / SKIN / FACE - ASSET DIAGNOSTIC",
          s->audio
              ? "L MOVE  R STICK LOOK  R VOICE  A RESET  Y OFFICE  B TITLE"
              : "L MOVE  R LOOK  A CAMERA RESET  Y OFFICE  B TITLE - SILENT",
          error))
    goto fail;
  s->overlay = bk_texture_create(s->renderer, &banner, error);
  bk_image_free(&banner);
  if (!s->overlay)
    goto fail;
  FILE *log = services->log ? services->log : stderr;
  fprintf(log,
          "Actor diagnostic: entry(0,8), authored NPC clip, half-speed "
          "scheduler, ENVL world vertices, FAM/MORP face. "
          "Inspection camera/light rig; no gameplay. Audio: %s.\n",
          s->audio ? "R auditions bk3_06/PH10101.wav with consumed-cursor mouth"
                   : "explicit silent host diagnostic");
  fflush(log);
  return &s->base;
fail:
  bk_image_free(&banner);
  destroy(&s->base);
  return NULL;
}
