#ifndef BK_SAVE_FILE_REPLACE_INTERNAL_H
#define BK_SAVE_FILE_REPLACE_INTERNAL_H
/* Only save/capture owners use these paths. A completed temporary file is
 * renamed atomically where supported. Filesystems rejecting replacement
 * retain the old file as .bak until the new name is installed. On restart a
 * missing primary is restored from .bak; an existing primary is never erased
 * during recovery. Caller validates existing checkpoint contents first. */
int bk_save_file_recover(const char *path, char error[256]);
int bk_save_file_replace(const char *temporary, const char *path,
                         char error[256]);
#endif
