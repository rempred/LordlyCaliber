#ifndef LORDLY_GRAPHICS_MATH_H
#define LORDLY_GRAPHICS_MATH_H
#ifdef __cplusplus
extern "C" {
#endif
/* Portable adaptations of accepted Rev 0 library C. Row-vector matrices;
 * invalid camera inputs leave the destination unchanged. */
float lordly_graphics_sin(float radians);
float lordly_graphics_cos(float radians);
int lordly_graphics_look_at(float out[16], const float eye[3], const float target[3],
                            const float up[3]);
int lordly_graphics_perspective(float out[16], float fov, float aspect, float near_plane,
                                float far_plane, float scale);
#ifdef __cplusplus
}
#endif
#endif
