/* Host-only fsdev contract fixture, used by save-flow-switch-fs-probe. */
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#undef rename
int rename(const char *, const char *);
int bk_test_rename(const char *from, const char *to) {
  struct stat st;
  if (!stat(to, &st)) {
    errno = EEXIST;
    return -1;
  }
  if (errno != ENOENT)
    return -1;
  return rename(from, to);
}
