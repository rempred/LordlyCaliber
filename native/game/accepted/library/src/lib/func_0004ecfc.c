#include "common/types.h"

void func_0025FBE0(void);
extern u8 D_8018FC18;

void func_0004ecfc(void)
{
    if (D_8018FC18 == 0) {
        func_0025FBE0();
    }
    D_8018FC18 = 1;
}
