#include "core/camera.h"
#include "core/matrix.h"
#include "scene/inspection_camera.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%d: %s\n", __LINE__, #x);                               \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static int near(float a, float b) {
  return fabsf(a - b) <= 2e-5f * fmaxf(1, fabsf(b));
}
int main(void) {
  const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  float world[16] = {2, .2f, 0, 0, 0, 3, .4f, 0, 1, 0, -4, 0, 12, -7, 34, 1};
  float view[16], product[16], point[4];
  CHECK(bk_camera_view(view, world));
  bk_matrix_multiply(product, world, view);
  for (unsigned i = 0; i < 16; i++)
    CHECK(near(product[i], i % 5 == 0 ? 1 : 0));
  bk_matrix_point(point, world + 12, view);
  CHECK(near(point[0], 0) && near(point[1], 0) && near(point[2], 0) &&
        point[3] == 1);
  float alias[16];
  memcpy(alias, world, sizeof(alias));
  CHECK(bk_camera_view(alias, alias) && !memcmp(alias, view, sizeof(alias)));
  float saved[16];
  memcpy(saved, view, sizeof(saved));
  memset(world, 0, sizeof(world));
  world[15] = 1;
  CHECK(!bk_camera_view(view, world) && !memcmp(saved, view, sizeof(saved)));
  world[0] = world[5] = world[10] = 1;
  world[3] = .01f;
  CHECK(!bk_camera_view(view, world));
  world[3] = 0;
  world[12] = NAN;
  CHECK(!bk_camera_view(view, world));
  BkInspectionCamera c;
  bk_inspection_reset(&c);
  CHECK(bk_inspection_world(&c, world) && bk_camera_view(view, world));
  CHECK(bk_matrix_view(product, c.eye, c.yaw, c.pitch));
  for (unsigned i = 0; i < 16; i++)
    CHECK(near(view[i], product[i]));
  c.eye[0] = INFINITY;
  CHECK(!bk_inspection_world(&c, world));
  BkCameraLens lens = {1, .75f, 1, 100000};
  CHECK(bk_camera_projection(product, &lens));
  float p[3] = {0, 0, 1};
  bk_matrix_point(point, p, product);
  CHECK(near(point[2], 0) && product[0] > 0 && product[5] < 0);
  p[2] = 100000;
  bk_matrix_point(point, p, product);
  CHECK(near(point[2] / point[3], 1));
  memcpy(saved, product, sizeof(saved));
  lens.far_z = 1;
  CHECK(!bk_camera_projection(product, &lens) &&
        !memcmp(saved, product, sizeof(saved)));
  lens = (BkCameraLens){1, NAN, 1, 100000};
  CHECK(!bk_camera_projection(product, &lens));
  BkViewport v;
  CHECK(bk_camera_fit(&v, 1280, 720, 4, 3) && v.x == 160 && v.y == 0 &&
        v.width == 960 && v.height == 720);
  CHECK(bk_camera_fit(&v, 720, 1280, 4, 3) && v.x == 0 && v.y == 370 &&
        v.width == 720 && v.height == 540);
  CHECK(bk_camera_fit(&v, 641, 481, 4, 3) && v.x == 0 && v.y == 0 &&
        v.width == 641 && v.height == 480);
  CHECK(bk_camera_fit(&v, UINT32_MAX, UINT32_MAX, 4, 3) &&
        v.width == UINT32_MAX && v.height == 3221225471u);
  BkViewport old = v;
  CHECK(!bk_camera_fit(&v, 0, 720, 4, 3) && !memcmp(&v, &old, sizeof(v)));
  CHECK(!bk_camera_fit(&v, 1, 1, UINT32_MAX, 1));
  memcpy(world, identity, sizeof(world));
  float target[3] = {0, 0, 10};
  CHECK(bk_camera_aim(world, world, target));
  CHECK(!memcmp(world, identity, sizeof(world)));
  memcpy(saved, world, sizeof(world));
  target[2] = 0;
  CHECK(!bk_camera_aim(world, world, target) &&
        !memcmp(saved, world, sizeof(world)));
  target[1] = 10;
  CHECK(!bk_camera_aim(world, world, target));
  target[1] = 0;
  target[2] = 10;
  BkCameraFollowPose follow = {0};
  memcpy(follow.world, identity, sizeof(follow.world));
  const float sampled[3] = {10, 0, 0}, correction[3] = {0, 10, 0};
  CHECK(bk_camera_follow_pose(&follow, sampled, target, NULL, .1f));
  CHECK(follow.position[0] == 1 && follow.world[12] == 1 &&
        follow.world[8] == 0);
  /* An aim from the newly smoothed position would produce nonzero world[8]. */
  CHECK(bk_camera_follow_pose(&follow, sampled, target, correction, .25f));
  CHECK(follow.position[0] == 0 && follow.position[1] == 10 &&
        follow.world[8] < 0);
  BkCameraFollowPose old_follow = follow;
  CHECK(!bk_camera_follow_pose(&follow, sampled, target, NULL, NAN));
  CHECK(!bk_camera_follow_pose(&follow, sampled, target, NULL, -1));
  CHECK(!memcmp(&follow, &old_follow, sizeof(follow)));
  int complete = -7;
  const float destination[] = {40, 20, -10};
  CHECK(!bk_camera_transition_pose(&follow, destination, 90, NAN, &complete));
  CHECK(!bk_camera_transition_pose(&follow, destination, 90, -1, &complete));
  CHECK(!bk_camera_transition_pose(&follow, (float[]){NAN, 1, 2}, 90, .1f,
                                   &complete));
  CHECK(complete == -7 && !memcmp(&follow, &old_follow, sizeof(follow)));
  CHECK(bk_camera_transition_pose(&follow, destination, 90, .5f, &complete));
  CHECK(complete == 1);
  for (unsigned i = 0; i < 3; i++)
    CHECK(follow.position[i] == destination[i] &&
          follow.world[12 + i] == destination[i]);
  memcpy(follow.world, identity, sizeof(follow.world));
  follow.world[12] = .199999f;
  CHECK(bk_camera_transition_pose(&follow, (float[]){0, 0, 0}, 170, 0,
                                  &complete));
  CHECK(complete ==
        1); /* Position threshold does not wait for angle arrival. */
  follow.world[12] = .200001f;
  CHECK(bk_camera_transition_pose(&follow, (float[]){0, 0, 0}, 170, 0,
                                  &complete));
  CHECK(complete == 0);
  BkCameraLens screen_lens = {1, .75f, .5f, 126384};
  BkViewport screen_viewport = {16, 8, 640, 480};
  BkScreenPoint screen_point;
  memcpy(world, identity, sizeof(world));
  world[14] = 10;
  CHECK(bk_camera_project_frame(&screen_point, world, identity, &screen_lens,
                                &screen_viewport));
  CHECK(screen_point.position[0] == 336 && screen_point.position[1] == 248);
  CHECK(screen_point.depth > 0 && screen_point.depth < 1);
  world[13] = 1;
  CHECK(bk_camera_project_frame(&screen_point, world, identity, &screen_lens,
                                &screen_viewport));
  CHECK(screen_point.position[1] < 248);
  world[14] = -10;
  CHECK(bk_camera_project_frame(&screen_point, world, identity, &screen_lens,
                                &screen_viewport));
  CHECK(screen_point.depth > 1 && screen_point.position[1] > 248);
  BkScreenPoint screen_saved = screen_point;
  world[14] = 0;
  CHECK(!bk_camera_project_frame(&screen_point, world, identity, &screen_lens,
                                 &screen_viewport));
  CHECK(!memcmp(&screen_point, &screen_saved, sizeof(screen_point)));
  world[14] = 1e-30f;
  CHECK(!bk_camera_project_frame(&screen_point, world, identity, &screen_lens,
                                 &screen_viewport));
  CHECK(!memcmp(&screen_point, &screen_saved, sizeof(screen_point)));
  puts("PASS: affine inverse/alias/rejection, inspection frame, original lens "
       "depth, 4:3 fitting/overflow guards");
  return 0;
}
