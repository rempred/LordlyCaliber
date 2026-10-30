#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04PointersState {
    u8 *context;
    u8 *pointer;
} P04PointersState;

extern void func_00262B68(void);
extern void func_0024E9BC(s32, u8, s32);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_80225772[];

s32 func_00277BAC(u16 message, P04PointersState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Retain the observed context store before the shared-pointer load. */
        *(u8 **)state = p01_context_80220F68;
        state->pointer = p01_pointer_80220F6C;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_0024E9BC(0, state->context[0], 0x41);
        func_0024DA10(state->pointer[0], D_80225772);
        func_0025CAF0(state->pointer[0], 0x23, 0x19);
        func_00262C64(0x37, 0x1003);
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
