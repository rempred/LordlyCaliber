#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"

typedef struct P01ExtendedRootState {
    u8 *context;
    u8 *value_04;
    u8 value_08;
} P01ExtendedRootState;

typedef struct P01ResponseObject {
    u8 unknown_00[0xC];
    s32 value_0C;
} P01ResponseObject;

struct P01TrailState;
extern s32 func_00252A9C(u16, struct P01TrailState *);
extern void func_00262B68(void);
extern void func_00262830(s32);

extern void func_00262C64(s16, s16);
extern void *func_00262AB0(void *);





extern void func_002628E4(s32);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_data_80209AB0[];
extern u8 p01_data_80209AE4[];

s32 func_00252ED0(u16 message, P01ExtendedRootState *state)
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
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        p01_context_80220F68 = state->context;
        *(u8 *)&p01_pointer_80220F6C = state->value_08;
        func_00262AB0((void *)func_00252A9C);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_08, p01_data_80209AB0 + 18);
        func_0025CAF0(state->value_08, 5, 0xF);
        func_00262C64(3, 0x1003);
        break;
    case 0x1003:
        ((P01ResponseObject *)func_0024DA10(state->value_08, p01_data_80209AB0 + 12))->value_0C = -300;
        ((P01ResponseObject *)func_0024DA10(state->value_08, p01_data_80209AB0 + 6))->value_0C = -400;
        func_0024DA10(state->value_08, p01_data_80209AB0 + 24);
        func_0024E9BC(0, state->context[0], 0x67);
        func_0025CB60(state->value_04 + 1, 0xA, 5);
        func_0025DAB0(state->value_08, p01_data_80209AE4);
        func_00262C64(0x2D, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
