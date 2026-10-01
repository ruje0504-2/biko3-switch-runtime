#ifndef BK_SAVE_FILE_COMMIT_H
#define BK_SAVE_FILE_COMMIT_H
/* The application installs its platform's transaction commit service before
 * creating save owners and clears it after releasing them. */
typedef int (*BkSaveCommit)(const char *path, char error[256]);
void bk_save_set_commit(BkSaveCommit commit);
#endif
