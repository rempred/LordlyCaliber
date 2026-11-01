#include "game/effect_adapters_e08.h"

extern u32 func_00022e60(void);
extern void func_001FF830(s16 first, s16 second, s16 third, s16 fourth);

void func_0024E7AC(u8 value, u32 flags)
{
    s16 first = 0;
    s16 second = 0;

    if (value != 0xFF) {
        s16 parity = ((s32)func_00022e60() % 50) % 2;
        s16 opposite = parity ^ 1;

        /* Define the first result after the call, using the other zero value. */
        first = second;

        if (flags & 1) {
            first = value;
        }
        if (flags & 2) {
            second = value;
        }
        if (parity) {
            first = -first;
        }
        if (opposite) {
            second = -second;
        }
    }
    func_001FF830(first, second, 0, 0);
}
