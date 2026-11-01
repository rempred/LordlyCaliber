#include "game/effect_commands_e09.h"
#include "game/effect_callbacks_p02.h"
#include "game/effect_adapters_e08.h"


extern volatile u8 g_func_0025CAF0_value_68;
extern volatile u8 g_func_0025CAF0_value_70;
extern volatile u8 g_func_0025CAF0_value_71;
/* Retail materializes the accepted DB50 callback entry, 0x80208F90. */

void func_0025CAF0(u32 value_arg, u32 value_70, u32 value_71)
{
    /* The byte local retains KMC save order with the existing word interface. */
    u8 value = value_arg;
    if ((s16)func_0024BCA0(value & 255) == 0) {
        g_func_0025CAF0_value_68 = value;
        g_func_0025CAF0_value_71 = value_71;
        g_func_0025CAF0_value_70 = value_70;
        func_00262AB0((void *)func_0025DB50);
    }
}
