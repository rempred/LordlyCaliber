#include "game/effect_dispatch_e07.h"

struct Func0026A6E4State;

typedef void (*Func00269470Callback)(u32 reason, void *fields, void *argument);

extern void func_00023780(void *target, u32 bytes);


extern volatile u16 g_func_00269470_flags;
extern volatile Func00269470Callback g_func_00269470_callback;
extern void * volatile g_func_00269470_value_3C;

void *func_0026ac64(s32 argument, void *callback)
{
    volatile u16 *flags = &g_func_00269470_flags;

    func_00023780((void *)flags, 0x18);
    *flags = 1;
    g_func_00269470_value_3C = (void *)argument;
    g_func_00269470_callback = (Func00269470Callback)callback;
    return func_00262328((void *)func_0026a6e4);
}