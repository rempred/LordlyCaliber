#include "common/types.h"

void bcopy(const void *source, void *destination, s32 bytes);
extern u16 D_80220E60[6];

void func_002603EC(s32 value, const void *values)
{
    u16 *state = D_80220E60;

    state[0] = value;
    bcopy(values, state + 1, 10);
}
