#include "game/effect_commands_e09.h"
#include "game/effect_presentation_p04.h"
extern volatile u8 g_func_0025CAF0_value_68;
extern volatile u8 g_func_0025CAF0_value_70;
extern volatile u8 g_func_0025CAF0_value_71;
extern void func_00262B68(void);
extern void func_0020BDCC(u32, u32);

s32 func_0025DB50(u16 command, u8 *state)
{
    switch (command) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state[0] = g_func_0025CAF0_value_68;
        state[9] = g_func_0025CAF0_value_71;
        state[8] = g_func_0025CAF0_value_70;
        func_00262830(0x1000);
        break;
    case 0x1000:
        if (state[8] == 0) {
            func_00262830(0x1001);
        } else {
            func_00262C64(state[8], 0x1001);
        }
        break;
    case 0x1001:
        func_0020BDCC(state[0], state[9]);
        func_00262830(0x1002);
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
