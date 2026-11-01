#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
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

struct P01TransformState;
extern s32 func_002522E0(u16, struct P01TransformState *);
extern void func_00262B68(void);
extern void func_00262830(s32);

extern void func_00262C64(s16, s16);
extern void *func_00262AB0(void *);




extern void func_002628E4(s32);



extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_data_80209A56[];
extern u8 p01_data_80209A62[];
extern u8 p01_data_80209A8C[];

s32 func_002526CC(u16 message, P01ExtendedRootState *state)
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
        func_00262AB0((void *)func_002522E0);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_08, p01_data_80209A56);
        func_0024DA10(state->value_08, p01_data_80209A56 + 6);
        func_0024E9BC(0, state->context[0], 0x5E);
        func_0025CAF0(state->value_08, 0xA, 5);
        func_00262C64(0x14, 0x1003);
        break;
    case 0x1003:
        func_0024C57C(state->value_08, positions, 0);
        func_0024DFCC(positions, p01_data_80209A62, 4);
        func_0025CB60(state->value_04 + 1, 5, 2);
        func_0025DAB0(state->value_08, p01_data_80209A8C);
        func_00262C64(0x13, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
