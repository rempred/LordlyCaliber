#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

typedef struct P05TableRootState {
    u8 *context;
    u8 *values;
    u8 unknown_08[4];
    u8 value_0C;
    u8 unknown_0D;
    s16 index;
} P05TableRootState;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern P05FactoryRecord D_8020A0E2[];
extern u8 D_8020A15C[];
extern void func_00262B68(void);





extern void *func_0020C478(u32);
extern s32 func_0020C24C(void *);
extern void func_0020BDCC(s16, s32);

s32 func_0025AE64(u16 message, P05TableRootState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Keep the context store before the shared pointer read. */
        *(u8 **)state = p01_context_80220F68;
        state->values = p01_pointer_80220F6C;
        state->value_0C = state->values[0];
        /* Scalar halfword reset follows the byte capture. */
        *(u16 *)((u8 *)state + 0xE) = 0;
        if ((s16)func_0024BCA0(state->value_0C) != 0) {
            state->index++;
        }
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0024DA10(state->value_0C, &D_8020A0E2[2]);
        func_0024E9BC(0, state->context[0], 0xB2);
        func_00262C64(8, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_0C, &D_8020A0E2[3]);
        func_00262C64(4, 0x1002);
        break;
    case 0x1002: {
        P05FactoryRecord *record = &D_8020A0E2[0];

        func_0025D678(D_8020A15C);
        func_0024DA50(state->value_0C, record);
        func_0024DC20(state->value_0C, (struct Dc20InputRecord *)(record + 4), 0xB);
        func_00262C64(2, 0x1003);
        break;
    }
    case 0x1003: {
        u8 value;

        func_0024DA10(state->value_0C, &D_8020A0E2[1]);
        if (func_0020C24C(func_0020C478(state->values[state->index])) == 0) {
            func_0025D394(state->values + state->index);
        }
        value = state->values[state->index];
        if (value == 0xFF) {
            func_0020BDCC(value, -1);
        }
        func_0025CB60((u32)(state->values + state->index), 0x32, 0);
        func_00262C64(0x41, 0x1004);
        break;
    }
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
