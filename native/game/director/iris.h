#ifndef LORDLY_IRIS_H
#define LORDLY_IRIS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Rev0 ROM2B64AC,2B6410,2B5D58,2B616C and2B5D8C control/geometry.
   External frame/layer ownership stays with the host. */
int lordly_iris_create(uint8_t record[24], const uint32_t args[9], int count);
int lordly_iris_query(const uint8_t record[24]);
/* 0=ordinary/idle,1=close completion,2=release; host applies ownership changes. */
int lordly_iris_advance(uint8_t record[24]);
/* terms: radiusX,radiusY,centerX,centerY,alpha. Returns1 for a full black quad. */
int lordly_iris_geometry(const uint8_t record[24], int32_t terms[5]);
/* Emits93 packed16-byte native vertices, or4 for a full quad. */
int lordly_iris_mesh(uint8_t vertices[1488], const int32_t terms[5], int quad,
                     const double constants[3]);
#ifdef __cplusplus
}
#endif
#endif
