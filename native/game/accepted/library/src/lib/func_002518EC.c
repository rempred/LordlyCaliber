#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

typedef struct P05CountdownState {
    u8 value_00;
    u8 unknown_01[5];
    s16 counter_06;
} P05CountdownState;

extern void func_00262B68(void);


s32 func_002518EC(u16 message, P05CountdownState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->counter_06 = 0x3E;
        state->value_00 = 3;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_00262C64(8, 0x1004);
        break;
    case 0x1002:
        if (state->counter_06 & 1) {
            state->value_00--;
        }
        func_0024E7AC(state->value_00, 2);
        state->counter_06++;
        if (state->counter_06 == 0x14) {
            func_00262830(0x1005);
        }
        break;
    case 0x1004:
        if (state->counter_06-- > 0) {
            func_0024E7AC(state->value_00, 2);
        } else {
            state->value_00 = 0x1E;
            func_00262830(0x1002);
        }
        break;
    case 0x1005:
        func_0024E7AC(0xFF, 0xFF);
        func_002628E4(0x101);
        break;
    }
    return 0;
}
