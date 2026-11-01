#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04Value0CState {
    u8 *context;
    u8 *pointer;
    u8 unknown_08[4];
    u8 value_0C;
} P04Value0CState;

extern void func_00262B68(void);


extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_8020A100[];
extern u8 D_8020A106[];
extern u8 D_8020A10C[];
extern u8 D_8020A118[];

s32 func_0025B0A4(u16 message, P04Value0CState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Retain the observed context store before the shared-pointer load. */
        *(u8 **)state = p01_context_80220F68;
        state->pointer = p01_pointer_80220F6C;
        state->value_0C = *state->pointer;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0024DA10(state->value_0C, D_8020A100);
        func_0024DC20(state->value_0C, (struct Dc20InputRecord *)(D_8020A100 + 0x12), 0xC);
        func_0024E9BC(0, state->context[0], 0xB3);
        func_00262C64(5, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_0C, D_8020A106);
        func_00262C64(5, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_0C, D_8020A118);
        func_00262C64(4, 0x1003);
        break;
    case 0x1003:
        func_0024DC20(state->value_0C, (struct Dc20InputRecord *)(D_8020A10C), 0xD);
        func_0025CAF0(state->value_0C, 0xA, 0xA);
        func_0025CB60((u32)(state->pointer + 1), 0x28, 3);
        func_00262C64(0x41, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
