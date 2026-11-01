#include "game/effect_dispatch_e07.h"

struct Func0026B360State;

extern volatile u32 g_func_0026B360_value_38;
extern volatile u32 g_func_0026B360_count;



void *func_0026B7E4(u32 value, u32 count)
{
    g_func_0026B360_value_38 = value;
    g_func_0026B360_count = count;
    return func_00262328((void *)func_0026B360);
}
