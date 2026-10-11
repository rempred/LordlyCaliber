#include "game/combat_types.h"

extern u32 func_8009DAF4(u32);
extern void *func_80070F30(u32);
extern void func_8009DBB8(void *, u32);
extern unsigned char *D_801CFCC0;
extern unsigned char *D_801D06D0;
extern unsigned char *D_801D06D4;
extern unsigned char *D_801D06D8;
extern unsigned char *D_801D06DC;

void func_0021088C(void)
{
    unsigned char *base, *first, *second, *third;
    /* Reusing the widened offset preserves the measured load/add lifetimes. */
    u32 offset;
    D_801CFCC0 = func_80070F30(func_8009DAF4(0x31823A));
    func_8009DBB8(D_801CFCC0, 0x31823A);
    base = D_801CFCC0;
    offset = ((u16 *)base)[0];
    first = base + offset;
    offset = ((u16 *)base)[1];
    second = base + offset;
    offset = ((u16 *)base)[2];
    third = base + offset;
    D_801D06D4 = first;
    D_801D06D8 = second;
    D_801D06DC = third;
    D_801D06D0 = func_80070F30(func_8009DAF4(0x3188AE));
    func_8009DBB8(D_801D06D0, 0x3188AE);
}
