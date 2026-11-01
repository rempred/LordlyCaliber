#include "game/effect_dispatch_e07.h"

extern volatile u32 g_func_0026B820_value_38;



void *func_0026C070(u32 value)
{
    g_func_0026B820_value_38 = value;
    return func_00262328((void *)func_0026C09C);
}
