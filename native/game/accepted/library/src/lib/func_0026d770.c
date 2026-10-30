#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02Vec3 { float x, y, z; } P02Vec3;
typedef struct P02SlotD770 {
    u8 unknown_00[8];
    float value_08;
    u8 unknown_0C[8];
    float value_14[16];
    u16 unknown_54;
    u16 value_56, value_58, value_5A, value_5C;
    u16 value_5E, value_60, value_62;
    void *value_64;
    u16 value_68, value_6A;
} P02SlotD770;
typedef struct P02SecondD770 {
    u8 unknown_00[0x14];
    float value_14[16];
    u8 value_54, value_55, value_56, value_57;
    u16 value_58, value_5A, value_5C, value_5E;
} P02SecondD770;
typedef struct P02ThirdD770 {
    u8 unknown_00[0x14];
    float value_14[16];
    void *value_54;
} P02ThirdD770;
typedef struct P02RenderD770 {
    u8 unknown_00[0xA];
    u16 flags_0A;
    u8 unknown_0C[0x10];
    u8 value_1C[0x40];
} P02RenderD770;
typedef struct P02StateD770 {
    u32 value_00;
    u32 unknown_04;
    P02Vec3 value_08;
    P02SlotD770 *value_14;
    P02SecondD770 *value_18;
    P02ThirdD770 *value_1C;
    P02RenderD770 *value_20;
    u32 unknown_24;
    P02Vec3 value_28;
    s32 value_34;
    u32 unknown_38;
    u32 value_3C;
    void *value_40;
} P02StateD770;

extern void *func_00263120(void *, s32);
extern void func_00266484(void), func_00266DB0(void), func_00267038(void);
extern void func_00264fc0(void *, void *);
extern void guTranslateF(void *, float, float, float);
extern P02RenderD770 *func_00269A64(s32, s32, u32, s32, u32);
extern void func_00023780(void *, s32);
extern P02Vec3 p02_data_802206DC, p02_data_802206D0;
extern u8 p02_data_802206A0[];

void  func_0026d770(void *stateView)
{
    /* This helper uses the documented offset view of its caller's payload. */
    P02StateD770 *state = stateView;
    P02SlotD770 *first;
    P02SecondD770 *second;
    P02ThirdD770 *third;
    float *matrix;
    u32 value;
    u32 depth;

    state->value_14 = func_00263120((void *)func_00266484, 1);
    state->value_14->value_08 = 1000.0f;
    first = state->value_14;
    func_00264fc0(state->value_40, &first->value_64);
    first->value_5A = 0x20;
    first->value_5C = 2;
    first->value_5E = first->value_68 >> 1;
    first->value_60 = first->value_6A >> 1;
    first->value_56 = state->value_00;
    first->value_58 = state->value_3C;
    first->value_62 = 1;
    matrix = first->value_14;
    func_0025F530(&p02_data_802206DC, &p02_data_802206D0, matrix);
    func_0025F150(matrix, &state->value_08);
    state->value_18 = second = func_00263120((void *)func_00266DB0, 1);
    second->value_58 = 0;
    second->value_5A = 0;
    depth = first->value_58;
    second->value_5E = 0x20;
    second->value_56 = 0;
    second->value_55 = 0;
    second->value_54 = 0;
    second->value_57 = 0xFF;
    second->value_5C = depth - 2;
    func_0025F058(matrix, second->value_14);
    state->value_1C = third = func_00263120((void *)func_00267038, 0);
    guTranslateF(third->value_14, state->value_08.x, state->value_08.y, state->value_08.z);
    third->value_54 = p02_data_802206A0;
    value = func_0025F73C(state->value_34);
    state->value_20 = func_00269A64(0x22, 8, value, 8, value);
    state->value_20->flags_0A |= 0x4000;
    func_00023780(state->value_20->value_1C, 0x10);
    state->value_28.x = state->value_28.y = state->value_28.z = 0.0f;
}

