#include "game/effect_dispatch_e07.h"

typedef void (*Func00269470Callback)(u32 reason, void *fields, void *argument);

extern void func_00023780(void *destination, u32 bytes);


extern u16 g_func_00269470_flags;
extern void *g_func_00269470_value_3C;
extern Func00269470Callback g_func_00269470_callback;

void *func_002698A0(s32 argument, void *callback)
{
    u16 *flags = &g_func_00269470_flags;

    func_00023780(flags, 0x18);
    *flags = 0x21;
    g_func_00269470_value_3C = (void *)argument;
    g_func_00269470_callback = (Func00269470Callback)callback;
    return func_00262328((void *)func_00269470);
}
