#include "common/types.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

typedef struct P01ExtendedRootState {
    u8 *context;
    u8 *value_04;
    u8 value_08;
} P01ExtendedRootState;

struct P01ChildState;
extern s32 func_0024EA70(u16, struct P01ChildState *);
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_0025C8A4(void *);
extern void func_00262C64(s16, s16);
extern void func_00262AB0(void *);
extern void *func_0024DA10(u32, void *);
extern void func_0024E9BC(s32, u8, s32);
extern void func_0025CAF0(u8, s32, s32);
extern void func_0025DAB0(u8, void *);
extern void func_002628E4(s32);
extern void func_0024C57C(u8, P01Vec3 *, s32);
extern void func_0024DFCC(P01Vec3 *, void *, s32);
extern void func_0025CB60(u8 *, u8, u8);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_data_80209786[];
extern u8 p01_data_80209798[];
extern u8 p01_data_8020979E[];
extern u8 p01_data_802097D8[];

s32 func_0024EF74(u16 message, P01ExtendedRootState *state)
{
    P01Vec3 positions[4];
    u8 *value;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* As in the root at 0024EE08, preserve the first store/read order. */
        *(u8 **)state = p01_context_80220F68;
        value = p01_pointer_80220F6C;
        state->value_04 = value;
        state->value_08 = *value;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        p01_context_80220F68 = state->context;
        *(u8 *)&p01_pointer_80220F6C = state->value_08;
        func_00262AB0((void *)func_0024EA70);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_08, p01_data_80209786);
        func_0024E9BC(0, state->context[0], 0x2A);
        func_0025CAF0(state->value_08, 8, 5);
        func_00262C64(0xE, 0x1003);
        break;
    case 0x1003:
        func_0024DA10(state->value_08, p01_data_8020979E);
        func_00262C64(0x12, 0x1004);
        break;
    case 0x1004:
        func_0024C57C(state->value_08, positions, 0);
        func_0024DFCC(positions, p01_data_80209798, 4);
        func_0025CB60(state->value_04 + 1, 3, 5);
        func_0025DAB0(state->value_08, p01_data_802097D8);
        func_00262C64(0x13, 0x1005);
        break;
    case 0x1005:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
