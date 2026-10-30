#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04Halfword0AObject {
    u8 unknown_00[0xA];
    u16 value_0A;
} P04Halfword0AObject;

typedef struct P04ObjectPointerState {
    u8 *context;
    u8 *pointer;
    u8 unknown_08[4];
    u8 value_0C;
    u8 unknown_0D[3];
    P04Halfword0AObject *object_10;
} P04ObjectPointerState;

extern void func_00262B68(void);
extern void func_0024E9BC(s32, u8, s32);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_8020A03A[];

s32 func_0025A450(u16 message, P04ObjectPointerState *state)
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
        func_0024DA10(state->value_0C, D_8020A03A);
        state->object_10 = func_0024DA10(state->value_0C, D_8020A03A + 6);
        func_0024E9BC(0, state->context[0], 0xAC);
        func_0025CAF0(state->value_0C, 8, 5);
        func_0025CB60((u32)(state->pointer + 1), 0x1C, 0);
        func_00262C64(0x1A, 0x1001);
        break;
    case 0x1001:
        state->object_10->value_0A &= ~0x1000;
        func_00262C64(0xD, 0x1002);
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
