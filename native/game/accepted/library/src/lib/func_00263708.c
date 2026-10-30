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
extern void func_800712C4(void *data);

void func_00263708(void)
{
    func_800712C4(D_80220E14);
}