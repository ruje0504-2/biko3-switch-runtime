#include "save/file_replace_internal.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static int failure(char error[256], const char *operation, int code) {
  snprintf(error, 256, "file replacement: %s: %s (%d)", operation,
           strerror(code), code);
  return 0;
}
static int backup_path(char out[1320], const char *path, char e[256]) {
  if (!path || !*path || strlen(path) > 1300)
    return failure(e, "invalid path", EINVAL);
  snprintf(out, 1320, "%s.bak", path);
  return 1;
}
static int exists(const char *path, char e[256]) {
  struct stat st;
  if (!stat(path, &st)) {
    if (S_ISREG(st.st_mode))
      return 1;
    failure(e, "destination is not a regular file", EINVAL);
    return -1;
  }
  if (errno == ENOENT)
    return 0;
  failure(e, "inspect destination", errno);
  return -1;
}
int bk_save_file_recover(const char *path, char e[256]) {
  char backup[1320];
  if (!backup_path(backup, path, e))
    return 0;
  int present = exists(path, e);
  if (present)
    return present == 1;
  present = exists(backup, e);
  if (present < 0)
    return 0;
  if (present && rename(backup, path))
    return failure(e, "restore previous file", errno);
  return 1;
}
int bk_save_file_replace(const char *tmp, const char *path, char e[256]) {
  char backup[1320];
  if (!tmp || !backup_path(backup, path, e) || !bk_save_file_recover(path, e))
    return 0;
  if (!rename(tmp, path)) {
    /* A stale backup only occurs after a previously completed installation. */
    remove(backup);
    return 1;
  }
  int code = errno;
  if (code != EEXIST)
    return failure(e, "install temporary file", code);
  /* libnx fsdev forwards fsFsRenameFile and does not replace an existing
   * destination. Never unlink the sole good checkpoint to make room. */
  if (exists(path, e) != 1)
    return 0;
  if (remove(backup) && errno != ENOENT)
    return failure(e, "retire stale backup", errno);
  if (rename(path, backup))
    return failure(e, "preserve previous file", errno);
  if (rename(tmp, path)) {
    code = errno;
    if (rename(backup, path))
      return failure(e, "install failed; previous file retained in .bak", code);
    return failure(e, "install failed; previous file restored", code);
  }
  remove(backup);
  return 1;
}
