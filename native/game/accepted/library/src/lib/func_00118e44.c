#include "common/types.h"

typedef struct Q7Unit Q7Unit;
#define FIELD(p, type, offset) (*(type *)((u8 *)(p) + (offset)))
extern void func_801B2D2C(Q7Unit *);
extern void func_800EA604(void *, s32);
extern void func_801C45CC(Q7Unit *);
extern void func_801D6218(Q7Unit *);
extern void func_801B2B78(Q7Unit *);
extern s32 func_801DA228(Q7Unit *);
extern u8 D_800EB1F0[];
extern s32 D_801F0E04;
extern u8 D_801969BA[];
extern Q7Unit *D_801F0CB0[];

void func_00118E44(Q7Unit *first, Q7Unit *second)
{
    Q7Unit *selected;
    s32 mask;

    func_801B2D2C(second);
    mask = ~0x10;
    FIELD(second, u32, 0) &= mask;
    func_800EA604(D_800EB1F0, 0x289);
    func_801C45CC(second);
    func_801D6218(second);
    if (FIELD(first, u32, 0x78) & 0x10) {
        FIELD(first, u32, 0x78) &= mask;
        FIELD(second, u32, 0x78) &= mask;
    }
    if (!(FIELD(first, u32, 0) & 0x40)) {
        if ((FIELD(first, u32, 0) & 0x204) == 4 &&
            FIELD(first, s32, 0x24) == FIELD(first, u8, 0x20) &&
            FIELD(first, s32, 0x80) == D_801F0E04) {
            FIELD(first, u32, 0x78) |= 0x80;
            func_801B2D2C(first);
        }
        else if (FIELD(first, u8, 0x91) == 1) {
            func_801B2B78(first);
            FIELD(first, s32, 0x84) = -1;
        }
    } else {
        selected = D_801F0CB0[D_801969BA[func_801DA228(first) * 11]];
        if ((FIELD(selected, u32, 0) & 0x204) == 4 &&
            FIELD(selected, s32, 0x24) == FIELD(selected, u8, 0x20) &&
            FIELD(selected, s32, 0x80) == D_801F0E04) {
            FIELD(selected, u32, 0x78) |= 0x80;
            func_801B2D2C(selected);
        }
    }
}
