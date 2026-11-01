#include "game/effect_commands_e09.h"
#include "game/effect_callbacks_p02.h"
extern volatile u32 g_func_0025CB60_pointer;
extern volatile u8 g_func_0025CB60_byte_71;
extern volatile u8 g_func_0025CAF0_value_70;


void func_0025CB60(u32 source, u8 byte70, u8 byte71)
{
    if (*(u8 *)source != 255) {
        g_func_0025CB60_pointer = source;
        g_func_0025CB60_byte_71 = byte71;
        g_func_0025CAF0_value_70 = byte70;
        func_00262AB0((void *)func_0025DC4C);
    }
}
