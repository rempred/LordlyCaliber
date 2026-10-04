#ifndef LORDLY_SCENE_MATRIX_H
#define LORDLY_SCENE_MATRIX_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Row-vector matrices. Host trigonometry replaces the N64 math library. */
void lordly_matrix_identity(float out[16]);
void lordly_matrix_rotation(float out[16], float x, float y, float z, float radians);
void lordly_matrix_translate(float out[16], float x, float y, float z);
void lordly_matrix_scale(float out[16], float x, float y, float z);
void lordly_matrix_multiply(float out[16], const float left[16], const float right[16]);
int lordly_matrix_pack(uint8_t out[64], const float matrix[16]);
void lordly_matrix_unpack(float out[16], const uint8_t matrix[64]);
int lordly_channel_layer_matrix(uint8_t out[64], const uint8_t record[88]);
int lordly_channel_matrices(uint8_t records[20][88], int count, const float camera[14],
                            const double constants[4]);
#ifdef __cplusplus
}
#endif
#endif
