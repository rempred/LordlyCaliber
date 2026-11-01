#include "game/effect_commands_e09.h"
#include "game/effect_presentation_p04.h"

typedef struct E09StateDC4C {
    u8 unknown_00[4];
    /* Preserve the observed pointer store before the byte parameter loads. */
    u8 *volatile value_04;
    u8 value_08, value_09;
    s16 value_0A;
} E09StateDC4C;
extern volatile u32 g_func_0025CB60_pointer;
extern volatile u8 g_func_0025CAF0_value_70;
extern volatile u8 g_func_0025CAF0_value_71;
extern void func_00262B68(void);
extern void func_0020BDCC(u32, u32);

s32 func_0025DC4C(u16 command, E09StateDC4C *state)
{
    switch (command) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_04 = (u8 *)g_func_0025CB60_pointer;
        state->value_09 = g_func_0025CAF0_value_71;
        state->value_08 = g_func_0025CAF0_value_70;
        state->value_0A = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        if (state->value_08 == 0) {
            func_00262830(0x1001);
        } else {
            func_00262C64(state->value_08, 0x1001);
        }
        break;
    case 0x1001:
        func_0020BDCC(state->value_04[state->value_0A++], state->value_09);
        if (state->value_04[state->value_0A] == 255) func_00262830(0x1002);
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
