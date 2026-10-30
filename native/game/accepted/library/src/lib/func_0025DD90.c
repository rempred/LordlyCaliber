#include "common/types.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_math_e04.h"

typedef struct P04TwoByteState {
    u8 value_00;
    u8 value_01;
} P04TwoByteState;

typedef struct P04PresentationSpec {
    u16 value_00;
    u8 value_02;
    u8 unknown_03;
    u16 value_04;
} P04PresentationSpec;

typedef struct P04PresentationObject P04PresentationObject;

extern void func_00262B68(void);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern P04PresentationSpec D_8020A1E0;

s32 func_0025DD90(u16 message, P04TwoByteState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_00 = *p01_context_80220F68;
        state->value_01 = *p01_pointer_80220F6C;
        func_00262830(0x1000);
        break;
    case 0x1000: {
        u32 value = func_0025F73C(func_0025F74C(state->value_00));

        /* The existing interface carries the returned word as an address view. */
        func_0024DF7C(state->value_01, &D_8020A1E0, (void *)value);
        func_00262C64(0xF, 0x1001);
        break;
    }
    case 0x1001:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
