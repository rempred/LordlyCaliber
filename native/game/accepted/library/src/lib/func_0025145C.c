#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

typedef struct P05StepState {
    u8 value_00;
    u8 unknown_01[3];
    u8 phase_04;
    u8 unknown_05;
    s16 counter_06;
} P05StepState;

extern void func_00262B68(void);

/* Two signed halfwords in original data_0025e318; ownership stays there. */
extern s16 D_802099F0[];

s32 func_0025145C(u16 message, P05StepState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->phase_04 = 0;
        state->value_00 = 12;
        state->counter_06 = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_00262C64(0x15, 0x1002);
        break;
    case 0x1002:
        if (state->counter_06 & 1) {
            state->value_00--;
        }
        func_0024E7AC(state->value_00, 2);
        state->counter_06++;
        if (state->phase_04 < 2 &&
            D_802099F0[state->phase_04] < state->counter_06) {
            state->counter_06 = 0;
            state->value_00 = 15;
            state->phase_04++;
        }
        if (state->value_00 == 0) {
            func_00262830(0x1005);
        }
        break;
    case 0x1005:
        func_0024E7AC(0xFF, 0xFF);
        func_002628E4(0x101);
        break;
    }
    return 0;
}
