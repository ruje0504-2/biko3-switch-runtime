#include "scene/dialogue_entry.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static uint64_t hash = UINT64_C(1469598103934665603);
static void mix(const void *p, size_t n) {
  const unsigned char *b = p;
  for (size_t i = 0; i < n; i++)
    hash = (hash ^ b[i]) * UINT64_C(1099511628211);
}
static void write_fixture(const char *path, const char *value) {
  FILE *f = fopen(path, "wb");
  assert(f && fwrite(value, 1, strlen(value), f) == strlen(value));
  assert(!fclose(f));
}
static void failure_keeps(BkDialogueAssets *a, BkTextFlow *t,
                          BkDialogueBackdrop *b, BkResourceStore *store,
                          const BkDialogueEntry *entry) {
  BkDialogueAssets saved = *a;
  BkTextFlow text = *t;
  BkDialogueBackdrop backdrop = *b;
  char error[256];
  assert(!bk_dialogue_entry_open(a, t, b, store, entry, error));
  assert(!memcmp(&saved, a, sizeof(saved)));
  assert(!memcmp(&text, t, sizeof(text)));
  assert(!memcmp(&backdrop, b, sizeof(backdrop)));
  assert(a->raw.size && a->raw.data[0] == '#');
}
int main(int argc, char **argv) {
  char error[256], directory[] = "/tmp/biko3-dialogue-entry-XXXXXX", path[512];
  assert(mkdtemp(directory));
  snprintf(path, sizeof(path), "%s/scene.txt", directory);
  write_fixture(path,
                "#SG00001\r\n#bg003\r\n#10000 "
                "#C01#F02#E03#M00\r\nfirst\r\n#10001\r\nlast\r\n#end\r\n");
  BkResourceStore *store = bk_resources_create(error);
  assert(store &&
         bk_resources_mount_directory(store, "bk3_05", directory, 4096, error));
  BkDialogueAssets a = {0};
  a.state.current_label = -1;
  BkTextFlow text = {8, 9, 10, 11, 12};
  BkDialogueBackdrop b = {.saved_expression = -71,
                          .curtain_cycle = 3,
                          .image_kind = 17,
                          .image_wanted = 255};
  BkDialogueEntry e = {"scene.txt", "Type_S.FTT", 10000, 10001};
  assert(bk_dialogue_entry_open(&a, &text, &b, store, &e, error));
  assert(a.state.current_label == 10000 && a.state.code_f == 2);
  assert(text.delay == 0 && text.scroll == 0 && text.target == 0 &&
         text.enabled == 1 && text.started == 0);
  assert(b.image.alpha == 1 && b.image.speed == 2 && b.image.stage == 1 &&
         b.image_wanted == 1 && b.saved_expression == -71 &&
         b.curtain_cycle == 3);
  BkDialogueEntry bad = e;
  strcpy(bad.file, "absent.txt");
  failure_keeps(&a, &text, &b, store, &bad);
  strcpy(bad.font, "absent.ftt");
  failure_keeps(&a, &text, &b, store, &bad);
  memset(bad.file, 'x', sizeof(bad.file));
  failure_keeps(&a, &text, &b, store, &bad);
  /* Valid first marker with truncated body fails after successful loading;
   * the old script and shared metadata must remain owned and usable. */
  write_fixture(path, "#10000\r\nunterminated");
  failure_keeps(&a, &text, &b, store, &e);
  int done;
  assert(bk_dialogue_assets_next(&a, &done, error) && !done &&
         a.state.current_label == 10001);
  assert(bk_dialogue_assets_next(&a, &done, error) && done);
  bk_dialogue_assets_close(&a);
  bk_resources_destroy(store);
  assert(!unlink(path) && !rmdir(directory));
  BkDialogueEntry keep;
  memset(&keep, 0x5a, sizeof(keep));
  bad = keep;
  assert(!bk_dialogue_entry_select(&bad, 2, 0, 8, 0, error) &&
         !memcmp(&bad, &keep, sizeof(bad)));
  assert(!bk_dialogue_entry_select(&bad, 0x38, -1, 0, 0, error) &&
         !memcmp(&bad, &keep, sizeof(bad)));
  if (argc == 2) {
    store = bk_resources_create(error);
    snprintf(path, sizeof(path), "%s/bk3_05.pp", argv[1]);
    assert(store && bk_resources_mount(store, "bk3_05", path, error));
    unsigned entries = 0, pages = 0;
    static const uint8_t previous[] = {0x38, 2, 0x10, 0};
    for (unsigned p = 0; p < 4; p++)
      for (int group = 0; group < 5; group++)
        for (unsigned variant = 0; variant < ((p == 0 || p == 3) ? 1u : 3u);
             variant++) {
          assert(bk_dialogue_entry_select(&e, previous[p], group,
                                          variant ? 8 : 0, variant == 1 ? 4 : 3,
                                          error));
          a.state.current_label = -1;
          if (!bk_dialogue_entry_open(&a, &text, &b, store, &e, error)) {
            fprintf(stderr, "%s\n", error);
            return 1;
          }
          entries++;
          for (unsigned page = 0;; page++) {
            assert(page < 1000);
            mix(a.state.text.bytes, a.state.text.length);
            mix(&a.state.current_label, sizeof(a.state.current_label));
            mix(a.state.sound, strlen(a.state.sound));
            pages++;
            assert(bk_dialogue_assets_next(&a, &done, error));
            if (done)
              break;
          }
        }
    bk_dialogue_assets_close(&a);
    bk_resources_destroy(store);
    printf("PASS dialogue entry actual: %u entries %u pages FNV %016llx\n",
           entries, pages, (unsigned long long)hash);
  }
  puts("PASS dialogue entry bounds, missing/malformed assets, retained old "
       "ownership");
  return 0;
}
