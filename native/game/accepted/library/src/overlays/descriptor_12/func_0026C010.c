#include "game/effect_dispatch_e07.h"

struct Func0026BE30State;

extern void func_00023780(void *target, s32 bytes);


extern volatile u16 g_func_00269470_flags;
extern void * volatile g_func_00269470_value_3C;

void *func_0026C010(s32 argument)
{
    volatile u16 *flags = &g_func_00269470_flags;

    func_00023780((void *)flags, 0x18);
    *flags = 0xA;
    g_func_00269470_value_3C = (void *)argument;
    return func_00262328((void *)func_0026BE30);
}
