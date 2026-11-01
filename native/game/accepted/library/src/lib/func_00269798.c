#include "game/effect_dispatch_e07.h"

extern void func_00023780(void *destination, u32 bytes);


extern u16 g_func_00269470_flags;
extern void *g_func_00269470_value_3C;

void *func_00269798(s32 argument)
{
    func_00023780(&g_func_00269470_flags, 0x18);
    g_func_00269470_value_3C = (void *)argument;
    return func_00262328((void *)func_00269470);
}
