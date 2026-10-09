#include "common/types.h"

extern void *D_80219F38;
extern void func_8017C384(void *value);

void func_0019BAB4(void)
{
    void **slot = &D_80219F38;
    func_8017C384(*slot);
    *slot = 0;
}
