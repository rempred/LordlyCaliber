#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04Context0CState {
    u8 *context;
    u8 unknown_04[8];
    u8 value_0C;
} P04Context0CState;

extern void func_00262B68(void);


extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_80209EA8[];
extern u8 D_80209EB4[];
extern u8 D_80209EBA[];

s32 func_00257C6C(u16 message, P04Context0CState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Retain the observed context store before the shared-pointer load. */
        *(u8 **)state = p01_context_80220F68;
        state->value_0C = *p01_pointer_80220F6C;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0024DA10(state->context[0], D_80209EA8);
        func_0024DA50(state->context[0], D_80209EA8 + 6);
        func_0024E9BC(0, state->context[0], 0x9B);
        func_00262C64(0x17, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_0C, D_80209EB4);
        func_0024E9BC(0, state->context[0], 0x9C);
        func_00262C64(7, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_0C, D_80209EBA);
        func_0025CAF0(state->value_0C, 0xA, 0x19);
        func_00262C64(0x29, 0x1003);
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
