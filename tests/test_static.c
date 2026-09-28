#include "core/matrix.h"
#include "scene/inspection_camera.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                  \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static int near(float a, float b) { return fabsf(a - b) < 0.0001f; }
int main(void) {
  float view[16], projection[16], world[16], combined[16], point[4];
  float eye[] = {3, 4, 5};
  CHECK(bk_matrix_view(view, eye, 0, 0));
  bk_matrix_point(point, eye, view);
  CHECK(near(point[0], 0) && near(point[1], 0) && near(point[2], 0) &&
        point[3] == 1);
  CHECK(bk_matrix_projection(projection, 1.570796327f, 1, 1, 10));
  float p[] = {1, 1, 1};
  bk_matrix_point(point, p, projection);
  CHECK(near(point[0], 1) && near(point[1], -1) && near(point[2], 0) &&
        point[3] == 1);
  p[2] = 10;
  bk_matrix_point(point, p, projection);
  CHECK(near(point[2] / point[3], 1));
  CHECK(!bk_matrix_projection(projection, 1, 0, 1, 10));
  CHECK(!bk_matrix_projection(projection, 1, 1, 10, 1));
  CHECK(!bk_matrix_projection(projection, NAN, 1, 1, 10));
  CHECK(!bk_matrix_view(view, eye, INFINITY, 0));
  CHECK(bk_matrix_view(view, eye, 0, 0));
  CHECK(bk_matrix_projection(projection, 1.570796327f, 1, 1, 10));
  memset(world, 0, sizeof(world));
  world[0] = 2;
  world[5] = world[10] = world[15] = 1;
  world[12] = 3;
  world[13] = 4;
  world[14] = 7;
  bk_matrix_multiply(combined, world, view);
  bk_matrix_multiply(combined, combined, projection); /* Alias-safe W*V*P. */
  float local[] = {1, 1, 0};
  bk_matrix_point(point, local, combined);
  CHECK(near(point[0] / point[3], 1) && near(point[1] / point[3], -.5f));
  CHECK(near(point[2] / point[3], 5.0f / 9));
  BkInspectionCamera camera, initial, split;
  bk_inspection_reset(&camera);
  initial = split = camera;
  BkInput input = {.move_x = .1f, .move_y = -.15f, .look_x = .14f};
  char error[256];
  CHECK(bk_inspection_step(&camera, 1, &input, error));
  CHECK(!memcmp(&camera, &initial, sizeof(camera)));
  input = (BkInput){.move_x = 1, .move_y = 1, .held = BK_BUTTON_UP};
  CHECK(bk_inspection_step(&camera, 1, &input, error));
  for (unsigned i = 0; i < 60; i++)
    CHECK(bk_inspection_step(&split, 1.0 / 60, &input, error));
  CHECK(near(
      hypotf(camera.eye[0] - initial.eye[0], camera.eye[2] - initial.eye[2]),
      30));
  for (unsigned i = 0; i < 3; i++)
    CHECK(near(camera.eye[i], split.eye[i]));
  CHECK(near(camera.eye[1] - initial.eye[1], 20));
  input = (BkInput){.look_x = 1, .look_y = 1};
  for (unsigned i = 0; i < 10; i++)
    CHECK(bk_inspection_step(&camera, 1, &input, error));
  CHECK(camera.pitch == 1.4f && fabsf(camera.yaw) <= 3.141593f);
  input = (BkInput){.pressed = BK_BUTTON_CONFIRM};
  CHECK(bk_inspection_step(&camera, 1.0 / 60, &input, error));
  CHECK(!memcmp(&camera, &initial, sizeof(camera)));
  CHECK(!bk_inspection_step(&camera, NAN, &input, error));
  input.move_x = NAN;
  CHECK(!bk_inspection_step(&camera, .1, &input, error));
  puts("PASS: W*V*P order, clip depth/Y, invalid matrices, camera "
       "deadzone/speed/reset");
  return 0;
}
