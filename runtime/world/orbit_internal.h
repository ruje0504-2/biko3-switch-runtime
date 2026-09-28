#ifndef BK_WORLD_ORBIT_INTERNAL_H
#define BK_WORLD_ORBIT_INTERNAL_H
/* Shared native orbit matrix arithmetic. Offset NULL omits the additional
 * translation product used by4bb0a4/4df411. Does not wrap/clamp parameters. */
int bk_orbit_matrix(float out[16], float yaw, float pitch, float radius,
                    float height, const float *offset);
#endif
