#include "game/effect_dispatch_e07.h"

struct Func0026A6E4State;

extern void func_00023780(void *target, u32 bytes);


extern volatile u16 g_func_00269470_flags;
extern void * volatile g_func_00269470_value_3C;
extern volatile u16 g_func_00269470_value_44;
extern volatile u16 g_func_00269470_value_46;
extern void * volatile g_func_00269470_value_48;
extern void * volatile g_func_00269470_value_4C;

void *func_0026ad28(s32 argument, u16 value_44, u32 value_48,
                   u16 value_46, u32 value_4C)
{
    volatile u16 *flags = &g_func_00269470_flags;

    func_00023780((void *)flags, 0x18);
    *flags = 4;
    g_func_00269470_value_3C = (void *)argument;
    g_func_00269470_value_44 = value_44;
    g_func_00269470_value_48 = (void *)value_48;
    g_func_00269470_value_46 = value_46;
    g_func_00269470_value_4C = (void *)value_4C;
    return func_00262328((void *)func_0026a6e4);
}
