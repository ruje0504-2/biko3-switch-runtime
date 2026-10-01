/* Actual images/font/audio/file adapter and complete menu input. Game fields
 * are explicit fixtures; this probe does not claim full play-session loading.
 */
#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "app/save_preview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "save-menu line%d: %s (%s)\n", __LINE__, #x, e);         \
      goto done;                                                               \
    }                                                                          \
  } while (0)
static unsigned frames;
typedef struct {
  unsigned release28, release2, resume;
} Events;
static int release(void *p, uint8_t flow, char e[256]) {
  Events *v = p;
  if (flow == 0x28)
    ++v->release28;
  else if (flow == 2)
    ++v->release2;
  else {
    snprintf(e, 256, "unexpected release%u", flow);
    return 0;
  }
  return 1;
}
static int resume(void *p, char e[256]) {
  (void)e;
  ++((Events *)p)->resume;
  return 1;
}
static int submit(void *p, const int16_t *samples, size_t n, char e[256]) {
  (void)e;
  (void)samples;
  *(uint64_t *)p += n;
  return 1;
}
static int poll(void *p, uint64_t *n, char e[256]) {
  (void)e;
  *n = *(uint64_t *)p;
  return 1;
}
static int draw(BkScene *s, BkRenderer *r, char e[256]) {
  return bk_renderer_begin(r, e) && bk_scene_draw(s, &(BkSceneFrame){0}, e) &&
         bk_renderer_end(r, e);
}
static int tick(BkScene *s, BkRenderer *r, BkAudio *a, BkInput in,
                char e[256]) {
  ++frames;
  return bk_audio_poll(a, e) && bk_scene_step(s, .25, &in, e) &&
         bk_audio_fill(a, e) && draw(s, r, e);
}
static BkInput click(float x, float y) {
  return (BkInput){.pointer_active = 1,
                   .pointer_x = x * .5f,
                   .pointer_y = y * .5f,
                   .pressed = BK_BUTTON_CONFIRM};
}
int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "save-menu-probe DATA OUTPUT.rgba\n");
    return 2;
  }
  char e[256] = {0}, path[2048], root[] = "/tmp/bk-save-menu-XXXXXX";
  int result = 1, made_root = 0;
  BkResourceStore *resources = bk_resources_create(e);
  BkRenderer *renderer = NULL;
  BkCheckpointFiles *files = NULL;
  BkAudio *audio = NULL;
  BkScene *scene = NULL;
  uint8_t *pixels = malloc(640 * 480 * 4), *redraw = malloc(640 * 480 * 4);
  uint64_t submitted = 0;
  CHECK(resources && pixels && redraw && mkdtemp(root));
  made_root = 1;
  const char *packs[] = {"bk3_00", "bk3_02"};
  for (unsigned i = 0; i < 2; ++i) {
    snprintf(path, sizeof(path), "%s/%s.pp", argv[1], packs[i]);
    CHECK(bk_resources_mount(resources, packs[i], path, e));
  }
  CHECK(bk_resources_mount_directory(resources, "fonts", argv[1],
                                     16 * 1024 * 1024, e));
  files = bk_checkpoint_files_create(root, e);
  renderer = bk_renderer_create(640, 480, stderr, e);
  BkAudioSink sink = {&submitted, 48000, 240, 960, submit, poll};
  audio = bk_audio_create(&sink, e);
  CHECK(files && renderer && audio);
  BkSceneServices services = {resources, renderer, stderr, audio, NULL, NULL};
  BkSavePreviewState state = {.control.tab = 3};
  BkCommonHudState common = {0};
  BkMenuCursor cursor = {0};
  CHECK(bk_menu_cursor_initialize(&cursor, 640, 480));
  uint8_t overlay = 0, hover = 0, inventory[5] = {0}, tail[3] = {0};
  uint32_t group = 0, area = 0, random = 123;
  BkSavePreviewGame game = {&group, &area, &random, inventory, tail};
  BkFlowTransition flow;
  BkPauseBindings bindings = {&common, &flow, &cursor, &overlay, &hover};
  Events events = {0};
  BkSavePreviewOps ops = {&events, release, resume};
  unsigned saves = 0, loads = 0, empty = 0;
  for (unsigned g = 0; g < 5; ++g) {
    group = g;
    state.control.tab = 3 + (int)g;
    bk_common_hud_initialize(&common);
    flow = (BkFlowTransition){0x28, 0x20, 0, 0};
    scene = bk_save_preview_create(&services, files, &state, &bindings, &game,
                                   &ops, 0, e);
    CHECK(scene);
    for (unsigned i = 0; i < 8; ++i)
      CHECK(tick(scene, renderer, audio, (BkInput){0}, e));
    for (unsigned pass = 0; pass < 2; ++pass)
      for (unsigned slot = 0; slot < 10; ++slot) {
        area = (slot + pass) % 9;
        for (unsigned i = 0; i < 5; ++i)
          inventory[i] = (uint8_t)((i + slot + pass) % 2);
        for (unsigned i = 0; i < 3; ++i)
          tail[i] = (uint8_t)(0x90 + g + slot + i + pass);
        CHECK(tick(scene, renderer, audio, click(750, 250 + 54 * slot), e));
        CHECK(state.control.page == 1 && state.control.hover == (int)slot + 21);
        CHECK(tick(scene, renderer, audio, click(496, 548), e));
        CHECK(state.control.page == 0);
        BkCheckpointBank bank;
        CHECK(bk_checkpoint_file_read(files, g, &bank, e) == BK_RESOURCE_OK);
        BkCheckpoint *c = &bank.slots[slot];
        CHECK(c->area == area && c->stamp[0] &&
              !memcmp(c->inventory, inventory, 5) &&
              !memcmp(c->inventory + 5, tail, 3));
        ++saves;
        for (unsigned i = 0; i < 4; ++i)
          CHECK(tick(scene, renderer, audio, (BkInput){0}, e));
        if (pass == 1 && slot == 5 && g == 0) {
          CHECK(bk_renderer_readback(renderer, pixels, 640 * 480 * 4, e));
          BkSavePreviewState before = state;
          CHECK(draw(scene, renderer, e) &&
                bk_renderer_readback(renderer, redraw, 640 * 480 * 4, e));
          CHECK(!memcmp(pixels, redraw, 640 * 480 * 4) &&
                !memcmp(&before, &state, sizeof(state)));
          FILE *out = fopen(argv[2], "wb");
          CHECK(out);
          size_t n = fwrite(pixels, 1, 640 * 480 * 4, out);
          int closed = fclose(out);
          CHECK(n == 640 * 480 * 4 && !closed);
        }
      }
    CHECK(tick(scene, renderer, audio, click(1100, 908), e));
    for (unsigned i = 0; i < 12 && flow.current == 0x28; ++i)
      CHECK(tick(scene, renderer, audio, (BkInput){0}, e));
    CHECK(flow.current == 0x50 && flow.target == 2 && flow.mode == 3);
    bk_scene_destroy(scene);
    scene = NULL;
  }
  /* Reopen the real adapter before reads; each chosen slot drives the actual
   * load-confirm/curtain/release policy, including pause-owned game release. */
  bk_checkpoint_files_destroy(files);
  files = bk_checkpoint_files_create(root, e);
  CHECK(files);
  for (unsigned g = 0; g < 5; ++g)
    for (unsigned slot = 0; slot < 10; ++slot) {
      bk_common_hud_initialize(&common);
      flow = (BkFlowTransition){0x28, slot % 2 ? 4 : 1, 0, 0};
      scene = bk_save_preview_create(&services, files, &state, &bindings, &game,
                                     &ops, 0, e);
      CHECK(scene);
      CHECK(tick(scene, renderer, audio, click(514 + 168 * g, 170), e));
      CHECK(state.control.tab == (int)g + 3);
      CHECK(tick(scene, renderer, audio, click(750, 250 + 54 * slot), e));
      CHECK(state.control.page == 1);
      CHECK(tick(scene, renderer, audio, click(496, 548), e));
      CHECK(group == g && area == (slot + 1) % 9);
      for (unsigned i = 0; i < 5; ++i)
        CHECK(inventory[i] == (i + slot + 1) % 2);
      for (unsigned i = 0; i < 3; ++i)
        CHECK(tail[i] == 0x91 + g + slot + i);
      for (unsigned i = 0; i < 12 && flow.current == 0x28; ++i)
        CHECK(tick(scene, renderer, audio, (BkInput){0}, e));
      CHECK(flow.current == 0x50 && flow.target == 2 && flow.mode == 1);
      ++loads;
      bk_scene_destroy(scene);
      scene = NULL;
    }
  for (unsigned previous = 1; previous <= 4; previous += 3) {
    bk_common_hud_initialize(&common);
    flow = (BkFlowTransition){0x28, (uint8_t)previous, 0, 0};
    scene = bk_save_preview_create(&services, files, &state, &bindings, &game,
                                   &ops, 0, e);
    CHECK(scene);
    CHECK(tick(scene, renderer, audio, click(1100, 908), e));
    for (unsigned i = 0; i < 12 && flow.current == 0x28; ++i)
      CHECK(tick(scene, renderer, audio, (BkInput){0}, e));
    CHECK(flow.current == 0x50 && flow.target == previous && flow.mode == 0);
    bk_scene_destroy(scene);
    scene = NULL;
  }
  CHECK(events.release28 == 57 && events.release2 == 25 && events.resume == 1);
  /* Empty bank and corrupted file are separate states, never auto-erased. */
  snprintf(path, sizeof(path), "%s/save/checkpoint-0.bks", root);
  CHECK(!unlink(path));
  state.control.tab = 3;
  bk_common_hud_initialize(&common);
  flow = (BkFlowTransition){0x28, 1, 0, 0};
  scene = bk_save_preview_create(&services, files, &state, &bindings, &game,
                                 &ops, 0, e);
  CHECK(scene);
  CHECK(tick(scene, renderer, audio, click(750, 250), e) &&
        state.control.page == 0);
  ++empty;
  bk_scene_destroy(scene);
  scene = NULL;
  FILE *bad = fopen(path, "wb");
  CHECK(bad);
  CHECK(fputs("broken", bad) >= 0);
  CHECK(!fclose(bad));
  scene = bk_save_preview_create(&services, files, &state, &bindings, &game,
                                 &ops, 0, e);
  CHECK(!scene);
  printf("PASS save-menu saves=%u loads=%u empty=%u frames=%u releases=%u "
         "pause-game-releases=%u redraw-identical corrupt-rejected\n",
         saves, loads, empty, frames, events.release28, events.release2);
  result = 0;
done:
  bk_scene_destroy(scene);
  bk_checkpoint_files_destroy(files);
  bk_audio_destroy(audio);
  bk_renderer_destroy(renderer);
  bk_resources_destroy(resources);
  free(pixels);
  free(redraw);
  if (made_root) {
    for (unsigned g = 0; g < 5; ++g) {
      snprintf(path, sizeof(path), "%s/save/checkpoint-%u.bks", root, g);
      unlink(path);
    }
    snprintf(path, sizeof(path), "%s/save", root);
    rmdir(path);
    rmdir(root);
  }
  return result;
}
