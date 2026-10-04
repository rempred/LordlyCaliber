/* Portable reconstruction of ROM 22E90 (RPY), 28CA0 (packing), 28DA0
 * (concatenation), 28F60/29100 (scale/translation), and mode-zero ROM 2AFA68.
 * Reference: Decomp 62cc7ef4 original assembly. Not accepted matching C.
 * Matrix operation order and float widths are retained. Host sin/cos/tan
 * replace the target math library; this is not RSP rasterization. */
#include "scene_matrix.h"
#include <math.h>
#include <string.h>
static float read_real(const uint8_t *p) {
    const uint32_t bits = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    float value;
    memcpy(&value, &bits, 4);
    return value;
}
void lordly_matrix_identity(float out[16]) {
    memset(out, 0, 16 * sizeof(float));
    out[0] = out[5] = out[10] = out[15] = 1.f;
}
void lordly_matrix_rotation(float out[16], float x, float y, float z, float radians) {
    const float sx = sinf(x * radians), cx = cosf(x * radians);
    const float sy = sinf(y * radians), cy = cosf(y * radians);
    const float sz = sinf(z * radians), cz = cosf(z * radians);
    const float cxsy = cx * sy, sxsy = sx * sy;
    lordly_matrix_identity(out);
    out[0] = cy * cz;
    out[1] = cy * sz;
    out[2] = -sy;
    out[4] = sxsy * cz - cx * sz;
    out[5] = sxsy * sz + cx * cz;
    out[6] = sx * cy;
    out[8] = cxsy * cz + sx * sz;
    out[9] = cxsy * sz - sx * cz;
    out[10] = cx * cy;
}
void lordly_matrix_translate(float out[16], float x, float y, float z) {
    lordly_matrix_identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}
void lordly_matrix_scale(float out[16], float x, float y, float z) {
    lordly_matrix_identity(out);
    out[0] = x;
    out[5] = y;
    out[10] = z;
}
void lordly_matrix_multiply(float out[16], const float left[16], const float right[16]) {
    float result[16];
    int row, column;
    for (row = 0; row < 4; ++row)
        for (column = 0; column < 4; ++column) {
            float value = left[row * 4] * right[column];
            value += left[row * 4 + 1] * right[4 + column];
            value += left[row * 4 + 2] * right[8 + column];
            value += left[row * 4 + 3] * right[12 + column];
            result[row * 4 + column] = value;
        }
    memcpy(out, result, sizeof(result));
}
int lordly_matrix_pack(uint8_t out[64], const float matrix[16]) {
    uint8_t result[64];
    int i;
    for (i = 0; i < 16; ++i) {
        const float scaled = matrix[i] * 65536.f;
        uint32_t fixed;
        if (!isfinite(scaled) || scaled < -2147483648.f || scaled >= 2147483648.f)
            return 0;
        fixed = (uint32_t)(int32_t)scaled;
        result[i * 2] = (uint8_t)(fixed >> 24);
        result[i * 2 + 1] = (uint8_t)(fixed >> 16);
        result[32 + i * 2] = (uint8_t)(fixed >> 8);
        result[33 + i * 2] = (uint8_t)fixed;
    }
    memcpy(out, result, sizeof(result));
    return 1;
}
void lordly_matrix_unpack(float out[16], const uint8_t matrix[64]) {
    int i;
    for (i = 0; i < 16; ++i) {
        const uint32_t bits = (uint32_t)matrix[i * 2] << 24 | (uint32_t)matrix[i * 2 + 1] << 16 |
                              (uint32_t)matrix[32 + i * 2] << 8 | matrix[33 + i * 2];
        const int32_t signed_value = bits <= 0x7fffffffU ? (int32_t)bits : -1 - (int32_t)~bits;
        out[i] = (float)signed_value / 65536.f;
    }
}
int lordly_channel_layer_matrix(uint8_t out[64], const uint8_t record[88]) {
    float matrix[16], scale[16];
    uint8_t packed_scale[64];
    const float uniform = read_real(record + 0x54);
    lordly_matrix_scale(scale, uniform, uniform, uniform);
    if (!lordly_matrix_pack(packed_scale, scale))
        return 0;
    // ROM 2B04FC calls guScale then the packed concatenate helper at 28740.
    lordly_matrix_unpack(scale, packed_scale);
    lordly_matrix_unpack(matrix, record);
    lordly_matrix_multiply(matrix, matrix, scale);
    return lordly_matrix_pack(out, matrix);
}
int lordly_channel_matrices(uint8_t records[20][88], int count, const float camera[14],
                            const double constants[4]) {
    uint8_t packed[20][64];
    float angle, tangent, dx, dy, dz, distance, scale;
    int i;
    if (count < 0 || count > 20)
        return 0;
    for (i = 0; i < 14; ++i)
        if (!isfinite(camera[i]))
            return 0;
    for (i = 0; i < 4; ++i)
        if (!isfinite(constants[i]))
            return 0;
    if (constants[1] == 0 || constants[2] == 0)
        return 0;
    angle = (float)((double)camera[0] * constants[0]);
    angle = (float)((double)angle / constants[1]);
    tangent = (float)tan((double)angle);
    dx = camera[5] - camera[8];
    dy = camera[6] - camera[9];
    dz = camera[7] - camera[10];
    distance = dx == 0.f && dy == 0.f ? dz : sqrtf((dz * dz + dy * dy) + dx * dx);
    scale = tangent * distance;
    scale = (float)((double)scale / constants[2]);
    if (!isfinite(scale))
        return 0;
    for (i = 0; i < count; ++i) {
        float matrix[16], temporary[16];
        lordly_matrix_rotation(matrix, read_real(records[i] + 0x48), read_real(records[i] + 0x4c),
                               0.f, (float)constants[3]);
        lordly_matrix_translate(temporary, read_real(records[i] + 0x40),
                                read_real(records[i] + 0x44), read_real(records[i] + 0x50));
        lordly_matrix_multiply(matrix, matrix, temporary);
        lordly_matrix_scale(temporary, scale, scale, scale);
        lordly_matrix_multiply(matrix, matrix, temporary);
        if (!lordly_matrix_pack(packed[i], matrix))
            return 0;
    }
    for (i = 0; i < count; ++i)
        memcpy(records[i], packed[i], 64);
    return 1;
}
