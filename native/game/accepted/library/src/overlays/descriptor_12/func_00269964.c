#include "game/effect_dispatch_e07.h"

extern void func_00023780(void *destination, u32 bytes);


extern u16 g_func_00269470_flags;
extern void *g_func_00269470_value_3C;

void *func_00269964(s32 argument)
{
    u16 *flags = &g_func_00269470_flags;

    func_00023780(flags, 0x18);
    *flags = 0x22;
    g_func_00269470_value_3C = (void *)argument;
    return func_00262328((void *)func_00269470);
}
