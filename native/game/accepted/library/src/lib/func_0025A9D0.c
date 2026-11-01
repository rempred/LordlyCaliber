#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

/* Only the fields read or written by this callback are named. */
typedef struct P05ThreeStepState {
    u8 unknown_00;
    u8 value_01;
    s16 index;
} P05ThreeStepState;

extern u8 p01_byte_80220F69;
extern u8 D_8020A094[];
extern s16 D_8020A148[];
extern void func_00262B68(void);

s32 func_0025A9D0(u16 message, P05ThreeStepState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_01 = p01_byte_80220F69;
        state->index = 0;
        func_00262830(0x1000);
        break;
    case 0x1000: {
        s16 nextMessage = state->index == 2 ? 0x1001 : 0x1000;

        func_0024DA10(state->value_01, D_8020A094);
        func_00262C64(D_8020A148[state->index++], nextMessage);
        break;
    }
    case 0x1001:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
