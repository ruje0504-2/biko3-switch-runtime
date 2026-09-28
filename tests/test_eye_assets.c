#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "scene/eye_assets.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
  char path[] = "/tmp/biko3-eye-XXXXXX", error[256], file[256];
  assert(mkdtemp(path));
  snprintf(file, sizeof(file), "%s/eye.TGA", path);
  const uint8_t tga[22] = {0, 0, 2, 0, 0, 0,  0,    0, 0, 0, 0,
                           0, 1, 0, 1, 0, 32, 0x28, 3, 2, 1, 129};
  FILE *f = fopen(file, "wb");
  assert(f && fwrite(tga, 1, sizeof(tga), f) == sizeof(tga) && !fclose(f));
  BkResourceStore *s = bk_resources_create(error);
  assert(s && bk_resources_mount_directory(s, "test", path, 1024, error));
  BkModelFrame frames[2] = {{.name = "Frame1 Left"}, {.name = "Frame2 Right"}};
  BkModelMesh mesh = {.name = "Eye", .submesh_count = 1};
  BkModelSubmesh sub = {.texture_count = 1, .texture_indices = {0}};
  BkModel model = {.frames = frames,
                   .frame_count = 2,
                   .meshes = &mesh,
                   .mesh_count = 1,
                   .submeshes = &sub,
                   .submesh_count = 1};
  BkFaceConfig c = {.texture_mode = 1};
  strcpy(c.eye_materials[0], "Left");
  strcpy(c.eye_materials[1], "Right");
  strcpy(c.slots[0][0].target, "Eye@A.X");
  strcpy(c.eye_textures[0], "never-load-this.bmp");
  strcpy(c.eye_textures[1], "eye.TGA");
  BkEyeAssets *e = bk_eye_assets_create(s, "test", &model, "a.x", &c, error);
  assert(e);
  const BkEyeBinding *b = bk_eye_assets_binding(e);
  assert(b->frames[0] == 0 && b->frames[1] == 1 && b->target_submesh == 0 &&
         b->texture_mode == 1);
  int alpha = 0;
  const BkImage *image = bk_eye_assets_image(e, 1, &alpha);
  assert(image && alpha == 1 && image->width == 1 && image->height == 1 &&
         !memcmp(image->rgba, "\1\2\3\201", 4));
  assert(!bk_eye_assets_image(e, 0, NULL));
  assert(bk_eye_assets_select(e, 1, error) && bk_eye_assets_selected(e) == 1);
  for (unsigned i = 2; i < 5; i++)
    assert(bk_eye_assets_select(e, i, error) && bk_eye_assets_selected(e) == 1);
  assert(!bk_eye_assets_select(e, 5, error) && bk_eye_assets_selected(e) == 1);
  assert(!bk_eye_assets_select(e, UINT32_MAX, error));
  assert(bk_eye_assets_select(e, 0, error) && !bk_eye_assets_selected(e));
  assert(!bk_eye_assets_select(NULL, 0, error));
  /* Missing frame disables gaze only; texture target is independent. */
  strcpy(c.eye_materials[0], "missing");
  BkEyeAssets *missing =
      bk_eye_assets_create(s, "test", &model, "a.x", &c, error);
  assert(missing && bk_eye_assets_binding(missing)->frames[0] == BK_MODEL_NONE);
  assert(bk_eye_assets_image(missing, 1, NULL));
  bk_eye_assets_destroy(missing);
  strcpy(c.eye_textures[1], "absent.bmp");
  missing = bk_eye_assets_create(s, "test", &model, "a.x", &c, error);
  assert(missing && !bk_eye_assets_image(missing, 1, NULL));
  assert(bk_eye_assets_select(missing, 1, error) &&
         !bk_eye_assets_selected(missing));
  bk_eye_assets_destroy(missing);
  strcpy(c.eye_textures[1], "eye.TGA");
  f = fopen(file, "wb");
  assert(f && !fclose(f));
  assert(!bk_eye_assets_create(s, "test", &model, "a.x", &c, error));
  strcpy(c.slots[0][0].target, "missing");
  missing = bk_eye_assets_create(s, "test", &model, "a.x", &c, error);
  assert(missing &&
         bk_eye_assets_binding(missing)->target_submesh == BK_MODEL_NONE);
  assert(bk_eye_assets_select(missing, 1, error) &&
         !bk_eye_assets_selected(missing));
  bk_eye_assets_destroy(missing);
  memset(c.eye_materials[0], 'A', 256);
  assert(!bk_eye_assets_create(s, "test", &model, "a.x", &c, error));
  bk_resources_destroy(s);
  memset(&model, 0, sizeof(model));
  memset(&c, 0, sizeof(c));
  assert(!memcmp(bk_eye_assets_image(e, 1, NULL)->rgba, "\1\2\3\201", 4));
  assert(bk_eye_assets_select(e, 1, error));
  bk_eye_assets_destroy(e);
  bk_eye_assets_destroy(NULL);
  assert(!unlink(file) && !rmdir(path));
  puts("PASS eye resource ownership, optional slots, selection and corrupt "
       "inputs");
  return 0;
}
