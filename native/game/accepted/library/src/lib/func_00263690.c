#include "common/types.h"

typedef struct ResourceEntry16 {
    s16 unknown00;
    u16 code02;
    s32 key04;
    void *data08;
    u16 next0C;
    u16 unknown0E;
} ResourceEntry16;

extern ResourceEntry16 *D_80220E14;
extern u16 D_80220E10;
extern void *func_80070F30(s32 size);
extern void func_80093380(void *destination, s32 size);

void func_00263690(void)
{
    s32 i;

    D_80220E14 = func_80070F30(1024);
    func_80093380(D_80220E14, 1024);
    for (i = 0; i < 63; i++) {
        D_80220E14[i].next0C = i + 1;
    }
    D_80220E10 = 0;
    D_80220E14[i].next0C = 0xFFFF;
}
