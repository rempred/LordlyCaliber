#include "game/chapter_card.h"

extern u16 D_8018F1A0;
extern void *D_8022A974;
extern void resource_free(void *buffer);
extern void func_002879F8(void);
extern void func_00289504(void);
extern void func_00284184(void);
extern void func_0028364C(void);

void func_0028a6e4(void)
{
    u8 i;
    s32 variant;

    for (i = 0; i < 5; ++i) {
        resource_free(D_8023DE20->images[i]);
        resource_free(D_8023DE20->secondary[i]);
    }
    resource_free(D_8023DE20);
    variant = D_8018F1A0 - 1073;
    D_8023DE20 = 0;

    if (variant == 5) {
        func_002879F8();
        func_00289504();
    }

    func_00284184();
    resource_free(D_8022A974);
    D_8022A974 = 0;
    func_0028364C();
}
