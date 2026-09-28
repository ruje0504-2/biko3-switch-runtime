#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "save/checkpoint_file.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void write_bytes(const char *p, const void *bytes, size_t n) {
  FILE *f = fopen(p, "wb");
  assert(f);
  assert(fwrite(bytes, 1, n, f) == n);
  assert(!fclose(f));
}
int main(void) {
  char e[256], root[] = "/tmp/bk-checkpoint-XXXXXX", p[256], tmp[256];
  assert(mkdtemp(root));
  BkCheckpointFiles *f = bk_checkpoint_files_create(root, e);
  assert(f);
  BkCheckpointBank bank = {0}, out, old;
  memset(&out, 0xa5, sizeof(out));
  old = out;
  assert(bk_checkpoint_file_read(f, 0, &out, e) == BK_RESOURCE_MISSING);
  assert(!memcmp(&old, &out, sizeof(out)));
  BkCheckpointTime t = {2026, 9, 28, 0, 0, 1};
  uint8_t inv[8] = {0, 1, 2, 3, 4, 127, 128, 255};
  uint8_t bytes[593], saved[592], native[560];
  for (unsigned group = 0; group < 5; ++group) {
    for (unsigned slot = 0; slot < 10; ++slot) {
      t.second = slot;
      assert(bk_checkpoint_file_store(f, group, slot, (group + slot) % 9, inv,
                                      &t, (int32_t)slot - 5, &bank, e));
      for (unsigned i = 0; i <= slot; ++i) {
        assert(bank.slots[i].area == (group + i) % 9);
        assert(!memcmp(bank.slots[i].inventory, inv, 8));
      }
      assert(bk_checkpoint_file_read(f, group, &out, e) == BK_RESOURCE_OK);
      assert(!memcmp(&bank, &out, sizeof(bank)));
    }
  }
  /* Restart uses disk data, not process cache. */
  bk_checkpoint_files_destroy(f);
  f = bk_checkpoint_files_create(root, e);
  assert(f);
  assert(bk_checkpoint_file_read(f, 4, &out, e) == BK_RESOURCE_OK);
  assert(!memcmp(&bank, &out, sizeof(bank)));
  assert(bk_checkpoint_encode(&bank, 4, bytes, e));
  memcpy(saved, bytes, 592);
  assert(bk_checkpoint_native_encode(&bank, native, e));
  assert(bk_checkpoint_native_decode(&out, native, 560, e));
  assert(!memcmp(&bank, &out, sizeof(bank)));
  /* Flip every byte, including version/group/CRC/reserved/timestamp suffix. */
  old = out;
  for (unsigned i = 0; i < 592; ++i) {
    bytes[i] ^= 1;
    assert(!bk_checkpoint_decode(&out, 4, bytes, 592, e));
    assert(!memcmp(&out, &old, sizeof(out)));
    bytes[i] ^= 1;
  }
  for (unsigned n = 0; n < 592; ++n) {
    assert(!bk_checkpoint_decode(&out, 4, bytes, n, e));
    assert(!memcmp(&out, &old, sizeof(out)));
  }
  assert(!bk_checkpoint_decode(&out, 4, bytes, 593, e));
  assert(!bk_checkpoint_decode(&out, 3, bytes, 592, e));
  for (unsigned slot = 10; slot < 13; ++slot)
    assert(!bk_checkpoint_file_store(f, 4, slot, 1, inv, &t, 0, &out, e));
  assert(!bk_checkpoint_file_store(f, 5, 0, 1, inv, &t, 0, &out, e));
  assert(!bk_checkpoint_file_store(f, 4, 0, 9, inv, &t, 0, &out, e));
  assert(!memcmp(&old, &out, sizeof(out)));
  /* Original update preserves reserved bytes, old stamp suffix and all other
   * slots. */
  memset(bank.slots[3].opaque, 0xe3, 8);
  memset(bank.slots[3].stamp + 20, 0xf2, 12);
  old = bank;
  assert(bk_checkpoint_update(&bank, 3, 8, inv, &t, INT32_MIN, e));
  assert(bank.slots[3].nonce == 0);
  assert(!memcmp(bank.slots[3].opaque, old.slots[3].opaque, 8));
  assert(!memcmp(bank.slots[3].stamp + 20, old.slots[3].stamp + 20, 12));
  for (unsigned i = 0; i < 10; ++i)
    if (i != 3)
      assert(!memcmp(&bank.slots[i], &old.slots[i], sizeof(BkCheckpoint)));
  char stamp[32], before[32];
  memset(stamp, 0xa5, 32);
  memcpy(before, stamp, 32);
  BkCheckpointTime bad = {1900, 2, 29, 1, 2, 3};
  assert(!bk_checkpoint_stamp(stamp, &bad));
  assert(!memcmp(stamp, before, 32));
  bad.year = 2000;
  assert(bk_checkpoint_stamp(stamp, &bad));
  assert(!strcmp(stamp, "2000/02/29-01:02:03"));
  bad.month = 4;
  bad.day = 31;
  assert(!bk_checkpoint_stamp(stamp, &bad));
  /* Temporary-file failure preserves the bank; a directory destination is
   * rejected during the required read before replacement is attempted. */
  snprintf(p, sizeof(p), "%s/save/checkpoint-4.bks", root);
  snprintf(tmp, sizeof(tmp), "%s.part", p);
  assert(!mkdir(tmp, 0700));
  old = out;
  assert(!bk_checkpoint_file_store(f, 4, 0, 2, inv, &t, 42, &out, e));
  assert(!memcmp(&old, &out, sizeof(out)));
  assert(!rmdir(tmp));
  assert(bk_checkpoint_file_read(f, 4, &bank, e) == BK_RESOURCE_OK);
  assert(!memcmp(&old, &bank, sizeof(old)));
  assert(!remove(p));
  assert(!mkdir(p, 0700));
  assert(!bk_checkpoint_file_store(f, 4, 0, 2, inv, &t, 42, &out, e));
  assert(!memcmp(&old, &out, sizeof(out)));
  assert(!rmdir(p));
  /* Existing corrupt file must not be replaced by an empty-bank fallback. */
  saved[150] ^= 1;
  write_bytes(p, saved, 592);
  assert(bk_checkpoint_file_read(f, 4, &out, e) == BK_RESOURCE_ERROR);
  assert(!memcmp(&out, &old, sizeof(out)));
  assert(!bk_checkpoint_file_store(f, 4, 0, 2, inv, &t, 42, &out, e));
  assert(!memcmp(&out, &old, sizeof(out)));
  FILE *in = fopen(p, "rb");
  assert(in);
  assert(fread(bytes, 1, 592, in) == 592);
  assert(!fclose(in));
  assert(!memcmp(saved, bytes, 592));
  saved[150] ^= 1;
  write_bytes(p, saved, 591);
  assert(bk_checkpoint_file_read(f, 4, &out, e) == BK_RESOURCE_ERROR);
  memcpy(bytes, saved, 592);
  bytes[592] = 0;
  write_bytes(p, bytes, 593);
  assert(bk_checkpoint_file_read(f, 4, &out, e) == BK_RESOURCE_ERROR);
  /* Native format only gets structural validation: no checksum is claimed. */
  memset(native, 0, 560);
  assert(!bk_checkpoint_native_decode(&out, native, 559, e));
  assert(!memcmp(&out, &old, sizeof(out)));
  bk_checkpoint_files_destroy(f);
  for (unsigned g = 0; g < 5; ++g) {
    snprintf(p, sizeof(p), "%s/save/checkpoint-%u.bks", root, g);
    assert(!remove(p));
  }
  snprintf(p, sizeof(p), "%s/save", root);
  assert(!rmdir(p));
  assert(!rmdir(root));
  puts("PASS checkpoint files:50 slots/restart,592 byte corruptions,592 "
       "truncations,failed replacement preserves bank");
}
