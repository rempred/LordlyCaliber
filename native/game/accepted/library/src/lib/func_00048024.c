#include "common/types.h"

extern u16 D_80196F60[];
extern u8 D_80197B00;
extern u8 D_80197B01;
extern volatile u8 D_80197B02;
extern u8 D_80197B03;
extern volatile u16 D_80197B04;
extern s16 D_80197B06;

extern void func_00023780(void *destination, u32 size);
extern u32 *func_8009DBB8(u32 *destination, u32 resource);
extern void *func_0002e138(u32 resource);
extern s32 resource_free(u8 *resource);
extern s32 func_00046854(s32 program);
/* The retail calls leave a0 unspecified. Both callees ignore the incoming value. */
extern s32 func_00047a94();
extern s32 func_00047dd8();

s32 func_00048024(void)
{
    u32 *resources;
    u32 resource;
    u8 *programs;
    s32 phase;
    volatile u8 *request;

    func_00023780(D_80196F60, 0x200);
    request = &D_80197B02;
    if (*request != 0) {
        u16 cursor = D_80197B04;
        *request = 0;
        D_80197B04 = cursor + 1;
    }

    phase = D_80197B03;
    switch (phase) {
    case 0:
    case 1:
        if (D_80197B06 != 0) {
            return func_00047a94();
        }
        return func_00047dd8();

    case 2:
        resources = func_8009DBB8(0, 0x3A5668);
        resource = resources[D_80197B00];
        resource_free((u8 *)resources);
        programs = func_0002e138(resource);
        func_00046854((s32)(programs + ((u16 *)programs)[D_80197B01]));
        resource_free(programs);
        return *(s32 *)0x80197B14;

    default:
        return 0;
    }
}
