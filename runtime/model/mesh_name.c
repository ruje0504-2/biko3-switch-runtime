#include "model/model.h"
#include <stdio.h>
#include <string.h>
static int fail(char *error, const char *reason) {
  snprintf(error, 256, "mesh name: %s", reason);
  return 0;
}
int bk_model_submesh_name(const BkModel *m, uint32_t index,
                          const char *filename, char out[256],
                          char error[256]) {
  if (!m || !m->submeshes || index >= m->submesh_count || !filename || !out)
    return fail(error, "invalid arguments");
  size_t n = strlen(filename);
  if (!n || n >= 128 || strchr(filename, '/') || strchr(filename, '\\'))
    return fail(error, "invalid model basename");
  char upper[128];
  for (size_t i = 0; i <= n; i++) {
    unsigned char c = (unsigned char)filename[i];
    upper[i] = (char)(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
  }
  uint32_t mi = m->submeshes[index].mesh_index;
  if (!m->meshes || mi >= m->mesh_count)
    return fail(error, "invalid parent mesh");
  const BkModelMesh *p = &m->meshes[mi];
  if (!memchr(p->name, 0, sizeof(p->name)) || index < p->first_submesh ||
      index - p->first_submesh >= p->submesh_count)
    return fail(error, "invalid parent name/range");
  char next[256];
  int length = p->submesh_count == 1
                   ? snprintf(next, sizeof(next), "%s@%s", p->name, upper)
                   : snprintf(next, sizeof(next), "%s_%u@%s", p->name,
                              index - p->first_submesh, upper);
  if (length < 0 || (size_t)length >= sizeof(next))
    return fail(error, "name too long");
  memcpy(out, next, (size_t)length + 1);
  return 1;
}
int bk_model_find_submesh(const BkModel *m, const char *filename,
                          const char *name, uint32_t *index, char error[256]) {
  if (!m || !name || !index)
    return fail(error, "invalid lookup");
  uint32_t found = BK_MODEL_NONE;
  for (uint32_t i = 0; i < m->submesh_count; i++) {
    char candidate[256];
    if (!bk_model_submesh_name(m, i, filename, candidate, error))
      return 0;
    if (!strcmp(candidate, name)) {
      if (found != BK_MODEL_NONE)
        return fail(error, "ambiguous submesh");
      found = i;
    }
  }
  if (found == BK_MODEL_NONE)
    return fail(error, "submesh not found");
  *index = found;
  return 1;
}
