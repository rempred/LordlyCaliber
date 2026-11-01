#ifndef GAME_EFFECT_GEOMETRY_E05_INTERNAL_H
#define GAME_EFFECT_GEOMETRY_E05_INTERNAL_H

#include "game/effect_geometry_e05.h"
#include "game/combat_draw.h"
#include "game/combat_draw_commands.h"

/* Numerically scoped record views preserve actual member offsets, widths and
 * signedness. Repeated private E05Object names did not establish one type. */
typedef struct E05DrawObject {
    u16 flags_00;
    u8 unknown_02[14];
    u16 mode_10;
} E05DrawObject;

typedef struct E05Transform { float matrix[4][4]; float value_40; } E05Transform;

typedef struct E05Object263AF0 {
    u32 unknown_00;
    E05Transform *transform_04;
    void *list_08;
    void *segment_0C;
    u32 unknown_10;
    void *segment_14;
} E05Object263AF0;

typedef struct E05Object263F40 {
    u32 unknown_00;
    E05Transform *transform_04;
    void *list_08;
    void *segment_0C;
    u32 unknown_10;
    void *segment_14;
    s16 value_18, value_1A, value_1C, value_1E;
} E05Object263F40;

typedef struct E05OuterObject263F40 { u8 unknown_00[0x18]; E05Object263F40 object_18; } E05OuterObject263F40;

typedef struct E05Object2641B0 {
    u32 unknown_00;
    E05Transform *transform_04;
    void *list_08;
    void *segment_0C;
    u32 unknown_10;
    void *segment_14;
    void *list_18;
    void *segment_1C;
} E05Object2641B0;

typedef struct E05OuterObject2641B0 { u8 unknown_00[0x18]; E05Object2641B0 object_18; } E05OuterObject2641B0;

typedef struct E05Object264770 {
    u32 unknown_00;
    E05Transform *transform_04;
    void *list_08;
    void *segment_0C;
    u32 unknown_10;
    void *segment_14;
    void *list_18;
    void *segment_1C;
    s16 value_20, value_22, value_24, value_26;
    s16 value_28, value_2A, value_2C, value_2E;
} E05Object264770;

typedef struct E05Object264BB0 {
    u32 unknown_00;
    E05Transform *transform_04;
    void *list_08;
    void *segment_0C;
    u32 unknown_10;
    void *segment_14;
    void *segment_18;
    s16 value_1C, value_1E, value_20, value_22;
    s16 value_24, value_26, value_28, value_2A;
} E05Object264BB0;

typedef struct E05Header {
    u8 value_00, value_01, value_02, value_03;
    u16 value_04, value_06;
} E05Header;

typedef struct E05Descriptor {
    void *data_00;
    u16 value_04, value_06, value_08, value_0A;
} E05Descriptor;

/* Unsigned texture-halfword view at 08/0A of a 16-byte CombatDrawVertex.
 * Strip producers retain the shared vertex layout and cast at this boundary. */
typedef struct E05TextureVertex16 {
    u8 unknown_00[8];
    u16 value_08, value_0A;
    u8 unknown_0C[4];
} E05TextureVertex16;

/* Four unsigned halfwords read at an 8-byte stride. */
typedef struct E05Point8U16 { u16 value_00, value_02, value_04, value_06; } E05Point8U16;

/* Eight-halfword reader view of the packed 16-byte storage. */
typedef struct E05Point16U16 {
    u16 value_00, value_02, value_04, value_06;
    u16 value_08, value_0A, value_0C, value_0E;
} E05Point16U16;

/* Signed coordinate/texture producer view at the same 8-byte stride. */
typedef struct E05Point8S16 { s16 x, y, value_04, value_06; } E05Point8S16;

typedef struct E05Geometry2657D0 {
    u8 unknown_00[0x42];
    s16 value_42, value_44;
    u8 unknown_46[4];
    s16 value_4A, value_4C;
} E05Geometry2657D0;

typedef struct E05Geometry265A28 {
    u8 unknown_00[0x42];
    s16 value_42, value_44;
    u16 value_46, value_48;
    s16 value_4A, value_4C;
    u8 unknown_4E[2];
    E05Descriptor descriptor;
} E05Geometry265A28;

/* 65D10 first uses float points, then copies a packed 16-byte record over
 * each element. 660D0 passes that storage through the unsigned reader view
 * to 655A0 after the producer returns. These are separate storage phases. */
typedef struct E05FloatPoint16 { float x, y, z, w; } E05FloatPoint16;

typedef struct E05PackedPoint16 {
    s16 x, y, value_04, value_06;
    s32 value_08;
    u32 value_0C;
} E05PackedPoint16;

typedef struct E05Geometry265D10 {
    u8 unknown_00[0x42];
    s16 value_42, value_44;
    u16 value_46, value_48, value_4A, value_4C, value_4E;
    void *data;
    u16 width, height, format, size;
} E05Geometry265D10;

typedef struct E05Geometry2660D0 {
    u8 unknown_00[0x42];
    s16 value_42, value_44;
    u16 value_46, value_48, value_4A, value_4C, value_4E;
    E05Descriptor descriptor;
} E05Geometry2660D0;

/* Six-byte reader: two unsigned coordinates and two bytes. */
typedef struct E05Point6U16 { u16 x, y; u8 value_04, value_05; } E05Point6U16;

/* Corresponding six-byte producer retains signed coordinate operations. */
typedef struct E05Point6S16 { s16 x, y; u8 value_04, value_05; } E05Point6S16;

typedef struct E05Object2667C0 {
    u8 unknown_00[0x40];
    u8 color[4];
    u16 mode_44, plane_46;
    s16 value_48, value_4A;
} E05Object2667C0;

typedef struct E05Object266DD0 {
    u8 unknown_00[0x40];
    u8 color[4];
    u8 value_44, unknown_45;
    u16 plane_46;
    s16 value_48, value_4A;
} E05Object266DD0;

typedef struct E05Object267038 {
    u8 unknown_00[0x54];
    void *value_54;
} E05Object267038;

typedef struct E05Object267108 {
    u8 unknown_00[0x40];
    float value_40;
    s16 value_44, value_46;
    u16 value_48;
} E05Object267108;

typedef struct E05Object2673F0 {
    u32 unknown_00;
    void *value_04;
    void *value_08;
    void *value_0C;
    u32 unknown_10;
    void *value_14;
} E05Object2673F0;

typedef struct E05Referenced {
    u8 unknown_00[0x40];
    float value_40;
} E05Referenced;

typedef struct E05Object2675B4 {
    u8 unknown_00[0x1C];
    E05Referenced *value_1C;
} E05Object2675B4;

typedef struct E05Object267668 {
    u8 unknown_00[0x1C];
    E05Referenced *value_1C;
    u8 unknown_20[0x10];
    u16 value_30, value_32, value_34;
} E05Object267668;

/* Address aliases are bound to the accepted core providers; these interfaces
 * preserve word-address, floating-argument and word-count ABI observations. */
extern float func_80092DB0(float);
extern float func_800907F0(float);
extern void func_800988A0(void *, void *);
extern void func_80093380(void *, u32);
extern void func_80093060(const void *, void *, u32);
extern void func_80098450(void *, float, float, float, float *, float *, float *);
extern void func_80098B60(void *, float, float, float);
extern void func_800989A0(void *, void *, void *);
extern void func_80098C20(void *, float, float, float);

#endif
