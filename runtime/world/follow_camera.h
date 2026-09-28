#ifndef BK_WORLD_FOLLOW_CAMERA_H
#define BK_WORLD_FOLLOW_CAMERA_H
#include "core/camera.h"
#include "world/actor_pose.h"
#include "world/failure_camera.h"
#include "world/follow_obstacle.h"
#include "world/player_camera.h"
#include "world/player_view.h"

/* CPU implementation of 0x4bdc12's bound track following. Caller supplies
 * actor origin and cached head pose, chooses the game-state branch and owns
 * obstacle queries. Asset names, routes and gameplay state do not live here.
 * Model/XAN are borrowed and must outlive the controller. */
typedef struct BkFollowCamera BkFollowCamera;
BkFollowCamera *bk_follow_camera_create(const BkModel *model,
                                        const BkClipSet *clips, uint32_t root,
                                        uint32_t track_node,
                                        const BkCameraFollowPose *initial,
                                        char error[256]);
void bk_follow_camera_destroy(BkFollowCamera *camera);
/* Advances slot 0 at half speed and computes aim/smoothing using the track
 * pose from the last publish. All failures preserve pose and timeline.
 * Head is already a cached world point; this function never guesses height.
 * correction=NULL means the caller's query found no obstacle correction. */
int bk_follow_camera_step(BkFollowCamera *camera, const float actor_origin[3],
                          const float head[3], const float *correction,
                          float seconds, char error[256]);
/* Same update with optional native+43c result: horizontal origin-to-held-track
 * distance, NOT distance to the smoothed camera. Failure preserves output. */
int bk_follow_camera_step_distance(BkFollowCamera *,
                                   const float actor_origin[3],
                                   const float head[3], const float *correction,
                                   float seconds, float *distance,
                                   char error[256]);
/* Complete follow math with live collision after first smoothing and before
 * correction. Initial distance is body-to-held-track; obstacle query can
 * shorten it and update retained probe. enabled0 preserves probe and skips
 * geometry. Atomic camera, clip and obstacle outputs. */
int bk_follow_camera_step_collision(BkFollowCamera *, const float actor[3],
                                    const float head[3], const BkCollision *,
                                    int enabled, float seconds,
                                    BkFollowObstacle *, char error[256]);
/* Publish the newly evaluated track world pose at the original traversal
 * phase, AFTER every controller has consumed the previous world poses. No
 * timeline advance occurs here. Keeping this explicit also supports frames
 * where local updates happen without a hierarchy traversal. */
void bk_follow_camera_publish(BkFollowCamera *camera);
/* Borrow the actual camera-track model instance for global-tree publication.
 * The follow controller retains ownership and animation/placement control;
 * destroy any borrowing forest before the controller. Once bound, use the
 * forest's visits instead of the independent publish helper above. Track()
 * reads this same published cache; there is no second copy to synchronize. */
BkActorPose *bk_follow_camera_bind_track(BkFollowCamera *camera);
const BkCameraFollowPose *bk_follow_camera_pose(const BkFollowCamera *camera);
const float *bk_follow_camera_track(const BkFollowCamera *camera);
int bk_follow_camera_clip_state(const BkFollowCamera *camera,
                                BkClipState *state);
/* Handover branches preserve the camera XAN timeline and published track.
 * Only rendered pose/smoothing and caller's completion output change. */
int bk_follow_camera_handover(BkFollowCamera *camera,
                              const BkPlayerCameraTarget *player, int head_mode,
                              float seconds, int *complete, char error[256]);
/* Share actual rendered pose/+4a4 with follow/handover while keeping the
 * caller's persistent player-view metadata. TRACK advances real camera XAN
 * at full seconds and consumes held track world; other kinds hold its clock.
 * Input track/timing fields are supplied here. Caller owns shared lean and
 * collision feedback. Failures before commit preserve outputs. */
int bk_follow_camera_player_view(BkFollowCamera *camera, BkPlayerView *state,
                                 BkPlayerViewKind kind,
                                 const BkPlayerViewInput *input,
                                 BkPlayerViewEffects *effects, char error[256]);
/*Failure controller updates only the shared active pose and +420 XYZ.
 * Keeps +4a4 and track animation/caches untouched. */
int bk_follow_camera_failure(BkFollowCamera *, BkFailureCameraKind,
                             const BkFailureCameraInput *,
                             BkFailureCameraEffects *, char error[256]);
#endif
