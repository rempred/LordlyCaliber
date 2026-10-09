#include "game/combat_types.h"

extern void func_800712C4(void *);
extern unsigned char *D_801CFCC0;
extern unsigned char *D_801D06D0;

void func_00210930(void)
{
    func_800712C4(D_801CFCC0);
    D_801CFCC0 = 0;
    func_800712C4(D_801D06D0);
    D_801D06D0 = 0;
}
