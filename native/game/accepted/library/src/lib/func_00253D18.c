#include "common/types.h"

typedef struct P01RepeatedRootState {
    u8 *context;
    u8 *value_04;
    u8 value_08;
    u8 unknown_09;
    s16 counter_0A;
} P01RepeatedRootState;

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

struct P01BytePairState;
extern s32 func_00253928(u16, struct P01BytePairState *);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_byte_80220F69;
extern u8 p01_data_80209B56[];
extern u8 p01_data_80209B50[];
extern u8 p01_data_80209B94[];
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_0025C8A4(void *);
extern void func_00262C64(s16, s16);
extern void func_00262AB0(void *);
extern void func_002628E4(s32);
extern void func_0024DC20(u8, void *, s32);
extern void func_0025CAF0(u8, s32, s32);
extern void func_0024E9BC(s32, u8, s32);
extern void *func_0024DA10(u32, void *);
extern void func_0025CB60(u8 *, u8, u8);
extern void func_0025DAB0(u8, void *);
extern void func_0024C57C(u8, P01Vec3 *, s32);
extern s16 func_0024C2F8(P01Vec3 *);

s32 func_00253D18(u16 message, P01RepeatedRootState *state)
{
    u8 *value;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Preserve the context store before the following shared load. */
        *(u8 **)state = p01_context_80220F68;
        value = p01_pointer_80220F6C;
        state->value_04 = value;
        state->value_08 = *value;
        state->counter_0A = 1;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        *(u8 *)&p01_context_80220F68 = state->context[0];
        p01_byte_80220F69 = state->value_08;
        func_00262AB0((void *)func_00253928);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002: {
        s16 count;

        func_0024DC20(state->value_08, p01_data_80209B56, 2);
        func_0025CAF0(state->value_08, 0xF, 5);
        for (count = 1; state->value_04[count] != 0xFF; count++) {
        }
        if (count >= 2) {
            func_0024E9BC(0, state->context[0], 0x76);
        }
        func_00262C64(4, 0x1003);
        break;
    }
    case 0x1003: {
        P01Vec3 positions[4];
        s16 positionIndex;

        func_0024C57C(state->value_08, positions, state->counter_0A != 1);
        for (positionIndex = 0; positionIndex < 4; positionIndex++) {
            s16 value = func_0024C2F8(&positions[positionIndex]);

            if (value != -1) {
                s16 index;

                for (index = 1; state->value_04[index] != (u8)value; index++) {
                }
                func_0024DA10(state->value_04[index], p01_data_80209B50);
                func_0025CAF0(state->value_04[index], 8, 2);
            }
        }
        if (state->counter_0A++ < 3) {
            func_00262C64(8, 0x1003);
        } else {
            func_0025DAB0(state->value_08, p01_data_80209B94);
            func_00262C64(0x1A, 0x1004);
        }
        break;
    }
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
