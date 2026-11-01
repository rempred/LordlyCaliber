#include "game/effect_dispatch_e07.h"

extern void func_00023780(void *destination, u32 bytes);


extern u16 g_func_00269470_flags;
extern void *g_func_00269470_value_3C;
extern u16 g_func_00269470_value_44;
extern u16 g_func_00269470_value_46;
extern void *g_func_00269470_value_48;
extern void *g_func_00269470_value_4C;

void *func_00269C2C(s32 argument, u16 value_44, u32 value_48,
                   u16 value_46, u32 value_4C)
{
    u16 *flags = &g_func_00269470_flags;

    func_00023780(flags, 0x18);
    *flags = 0x14;
    g_func_00269470_value_3C = (void *)argument;
    g_func_00269470_value_44 = value_44;
    g_func_00269470_value_48 = (void *)value_48;
    g_func_00269470_value_46 = value_46;
    g_func_00269470_value_4C = (void *)value_4C;
    return func_00262328((void *)func_00269470);
}
