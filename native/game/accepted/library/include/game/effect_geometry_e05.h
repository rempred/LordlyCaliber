#ifndef GAME_EFFECT_GEOMETRY_E05_H
#define GAME_EFFECT_GEOMETRY_E05_H

#include "common/types.h"

/* Recovered parameter views are distinct from full allocation extents.
 * Point readers use their observed 6/8/16-byte strides and unsigned loads;
 * producers retain signed coordinate views. Cross-view calls are explicit. */
struct E05DrawObject;
struct E05Object263AF0;
struct E05OuterObject263F40;
struct E05OuterObject2641B0;
struct E05Object264770;
struct E05Object264BB0;
struct E05Header;
struct E05Descriptor;
struct E05TextureVertex16;
struct E05Point8U16;
struct E05Point16U16;
struct E05Geometry2657D0;
struct E05Point8S16;
struct E05Geometry265A28;
struct E05Geometry265D10;
struct E05FloatPoint16;
struct E05Geometry2660D0;
struct E05Point6U16;
struct E05Object2667C0;
struct E05Object266DD0;
struct E05Object267038;
struct E05Object267108;
struct E05Object2673F0;
struct E05Object2675B4;
struct E05Object267668;

void func_002638D0(float x, float y, float distance, float *outX, float *outY);
void func_002639E0(struct E05DrawObject *object);
void func_00263AF0(struct E05Object263AF0 *object);
s32 func_00263CE4(u8 *input);
s32 func_00263E10(u8 *input);
s32 func_00263F40(struct E05OuterObject263F40 *input);
s32 func_002641B0(struct E05OuterObject2641B0 *input);
s32 func_00264490(struct E05OuterObject2641B0 *input);
void func_00264770(struct E05Object264770 *object);
s32 func_00264b1c(u8 *input);
s32 func_00264b50(u8 *input);
void func_00264bb0(struct E05Object264BB0 *object);
s32 func_00264f24(u8 *input);
s32 func_00264f58(u8 *input);
void func_00264fc0(struct E05Header *input, struct E05Descriptor *output);
void func_00265014(struct E05Descriptor *descriptor, struct E05TextureVertex16 *vertices, s32 first);
void func_002653d8(struct E05Descriptor *descriptor, struct E05Point8U16 *first, struct E05Point8U16 *second, s32 count);
void func_002655a0(struct E05Descriptor *descriptor, struct E05Point16U16 *first, struct E05Point16U16 *second, s32 count, s32 emitField);
void func_002657d0(struct E05Geometry2657D0 *geometry, float angle, s32 step, struct E05Point8S16 *points);
void func_00265a28(struct E05Geometry265A28 *geometry);
s32 func_00265cd0(void *argument);
s32 func_00265cf0(void *argument);
void func_00265d10(struct E05Geometry265D10 *geometry, float angle, s32 step, struct E05FloatPoint16 *points, float originX, float originY, void *matrix);
void func_002660d0(struct E05Geometry2660D0 *geometry);
s32 func_00266484(void *argument);
void func_002664b0(struct E05Point6U16 *first, struct E05Point6U16 *points, s32 count, s32 plane);
void func_002667c0(struct E05Object2667C0 *object);
s32 func_002669F0(u8 *object);
s32 func_00266A10(u8 *object);
void func_00266A30(struct E05Object2667C0 *object);
s32 func_00266DB0(u8 *object);
void func_00266DD0(struct E05Object266DD0 *object);
s32 func_00267018(u8 *object);
s32 func_00267038(struct E05Object267038 *object);
void func_00267108(struct E05Object267108 *object);
s32 func_002673C4(u8 *object);
void func_002673F0(struct E05Object2673F0 *object);
s32 func_0026756C(u8 *object);
s32 func_002675B4(struct E05Object2675B4 *object);
s32 func_00267668(struct E05Object267668 *object);

#endif
