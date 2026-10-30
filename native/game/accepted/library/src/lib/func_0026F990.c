#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02VecF990 { float x, y, z; } P02VecF990;
typedef struct P02SlotF990 {
    u8 unknown_00[8];
    float value_08;
    u8 unknown_0C[8];
    float value_14[16];
    u16 unknown_54;
    u16 value_56, value_58, value_5A, value_5C;
    u16 value_5E, value_60, value_62;
    void *value_64;
    u16 value_68, value_6A;
} P02SlotF990;
typedef struct P02SecondF990 {
    u8 unknown_00[0x14];
    float value_14[16];
    u8 value_54, value_55, value_56, value_57;
    u16 value_58, value_5A, value_5C, value_5E;
} P02SecondF990;
typedef struct P02ThirdF990 {
    u8 unknown_00[0x14];
    float value_14[16];
    void *value_54;
} P02ThirdF990;
typedef struct P02OtherF990 {
    u32 unknown_00;
    s32 value_04, value_08, value_0C;
    u8 unknown_10[8];
    float value_18[16];
    u8 value_58, value_59, value_5A, value_5B, value_5C, unknown_5D;
    u16 value_5E, value_60, value_62;
} P02OtherF990;
typedef struct P02RenderF990 {
    u8 unknown_00[0xA];
    u16 flags_0A;
} P02RenderF990;
typedef struct P02StateF990 {
    u8 unknown_00[8];
    s32 value_08;
    u32 unknown_0C;
    P02VecF990 value_10, value_1C;
    P02SlotF990 *value_28;
    P02SecondF990 *value_2C;
    P02ThirdF990 *value_30;
    P02OtherF990 *value_34;
    P02RenderF990 *value_38;
    u8 unknown_3C[0x10];
    s32 value_4C;
    void *value_50;
} P02StateF990;

extern P02RenderF990 *func_00269D7C(s32, s32, u32, s32, u32, void *);
extern P02RenderF990 *func_00269B0C(s32, s32, u32, s32, u32, void *);
extern void *func_00261DA8(void *);
extern void *func_00263120(void *, s32);
extern void func_00267018(void), func_00266484(void);
extern void func_00266DB0(void), func_00267038(void);
extern void func_00264fc0(void *, void *);
extern P02VecF990 p02_data_802209C8;
extern u8 p02_data_80220910[], p02_data_802209D4[];

void  func_0026F990(void *stateView)
{
    /* This helper uses the documented offset view of its caller's payload. */
    P02StateF990 *state = stateView;
    float matrix[16];
    P02SlotF990 *first;
    P02SecondF990 *second;
    P02ThirdF990 *third;
    P02OtherF990 *other;
    float *destination;
    u32 resource;

    if (state->value_08) state->value_10.x = -1.0f;
    else state->value_10.x = 1.0f;
    state->value_10.y = state->value_10.z = 0.0f;
    func_0025F530(&state->value_10, &p02_data_802209C8, matrix);
    func_0025F150(matrix, &state->value_1C);
    resource = func_0025F73C(state->value_4C);
    if (state->value_08) {
        state->value_38 = func_00269D7C(0x27, 8, resource, 8, resource,
                                      (void *)func_0026F7F0);
    } else {
        state->value_38 = func_00269B0C(0x27, 8, resource, 8, resource,
                                      (void *)func_00270F10);
    }
    state->value_38->flags_0A |= 0x4000;
    if (state->value_08) {
        state->value_34 = func_00261DA8((void *)func_00267018);
        state->value_34->value_04 = state->value_34->value_08 =
            state->value_34->value_0C = 100000;
        other = state->value_34;
        other->value_5B = 0xFF;
        other->value_5E = 0;
        other->value_58 = other->value_59 = other->value_5A = 0;
        other->value_5C = 0;
        other->value_60 = 0;
        other->value_62 = 0x20;
        func_0025F058(matrix, other->value_18);
    }
    if (!state->value_08) {
        state->value_28 = func_00263120((void *)func_00266484, 1);
        state->value_28->value_08 = 1000.0f;
        first = state->value_28;
        /* Taking this address before setup preserves the retail argument/load
         * order with the pinned compiler, without an extra saved register. */
        destination = first->value_14;
        func_00264fc0(state->value_50, &first->value_64);
        first->value_5A = 0x20;
        first->value_5C = 2;
        first->value_56 = 0;
        first->value_58 = 0x40;
        first->value_62 = 0;
        first->value_5E = first->value_68 >> 1;
        first->value_60 = first->value_6A >> 1;
        func_0025F058(matrix, destination);
        state->value_2C = second = func_00263120((void *)func_00266DB0, 1);
        second->value_5C = second->value_5A = second->value_58 = 0;
        second->value_5E = 0x20;
        second->value_54 = second->value_55 = second->value_56 = 0;
        second->value_57 = 0xFF;
        func_0025F058(matrix, second->value_14);
    }
    state->value_30 = third = func_00263120((void *)func_00267038, 0);
    third->value_54 = p02_data_80220910;
    func_0025F080(p02_data_802209D4, matrix, third->value_14);
}
