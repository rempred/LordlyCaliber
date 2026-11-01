#include "game/effect_dispatch_e07.h"

typedef void (*Func00269470Callback)(u32 reason, void *fields, void *argument);
struct Func0026A204State;



extern u16 g_func_00269470_flags;
extern void *g_func_00269470_value_3C;
extern Func00269470Callback g_func_00269470_callback;

void *func_0026a4d0(s32 argument)
{
    g_func_00269470_value_3C = (void *)argument;
    g_func_00269470_flags = 2;
    g_func_00269470_callback = 0;
    return func_00262328((void *)func_0026a204);
}
