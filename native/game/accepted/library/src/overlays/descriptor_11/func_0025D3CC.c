#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "game/effect_presentation_p04.h"

typedef struct E09StateD3CC {
    u8 value_00, value_01, value_02, value_03, value_04, value_05;
    s16 value_06;
} E09StateD3CC;
extern u8 e09_byte_80220F68, e09_byte_80220F69;
extern u8 e09_byte_80220F6A, e09_byte_80220F6B;
extern void func_00262B68(void);


s32 func_0025D3CC(u16 command, E09StateD3CC *state)
{
    s16 next;
    u8 value;
    switch (command) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_00 = e09_byte_80220F68;
        state->value_02 = e09_byte_80220F69;
        state->value_03 = e09_byte_80220F6A;
        state->value_04 = e09_byte_80220F6B & 3;
        state->value_05 = e09_byte_80220F6B & 0xF0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        if (state->value_02) {
            func_00262C64(state->value_02, 0x1001);
            break;
        }
        func_00262830(0x1001);
        /* fall through */
    case 0x1001:
        state->value_06 = 0;
        value = 0;
        if (state->value_05 & 0x20) value = state->value_00;
        state->value_01 = value;
        /* fall through */
    case 0x1002:
        next = 0x1002;
        if (state->value_05 & 0x20) {
            if (state->value_06++ & 1) state->value_01--;
            func_0024E7AC(state->value_01, state->value_04);
            if (state->value_01 == 0) next = 0x1005;
        } else {
            if (state->value_05 & 0x80) {
                if (state->value_06 % state->value_03 == 0) state->value_01++;
            } else if (state->value_06 & 1) {
                state->value_01++;
            }
            func_0024E7AC(state->value_01, state->value_04);
            state->value_06++;
            if (state->value_01 == state->value_00) {
                next = 0x1005;
                if (state->value_05 & 0x40) next = 0x1003;
            }
        }
        func_00262830(next);
        break;
    case 0x1003:
        state->value_06 = 0;
        func_00262830(0x1004);
        /* fall through */
    case 0x1004:
        next = 0x1004;
        if (state->value_06++ < state->value_03) {
            func_0024E7AC(state->value_01, state->value_04);
        } else {
            state->value_05 = 0x20;
            next = 0x1001;
        }
        func_00262830(next);
        break;
    case 0x1005:
        func_0024E7AC(255, 255);
        func_002628E4(0x101);
        break;
    }
    return 0;
}
