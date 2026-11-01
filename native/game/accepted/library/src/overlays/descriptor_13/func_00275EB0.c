#include "game/effect_presentation_p06_internal.h"

typedef struct P06State75EB0 {
    u8 unknown_00[0x1C];
    void *value_1C;
} P06State75EB0;

void func_00275EB0(P06State75EB0 *state)
{
    if (state->value_1C != 0) {
        func_00260C24(state->value_1C);
    }
}
