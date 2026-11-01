#include "game/effect_presentation_p06_internal.h"

typedef struct P06State75E68 {
    u8 unknown_00[0x1C];
    void *value_1C;
    u16 value_20;
    s16 value_22;
    s32 value_24;
    s16 value_28;
} P06State75E68;

void func_00275E68(P06State75E68 *state)
{
    state->value_28 = 0;
    state->value_1C = 0;
    state->value_24 = 0;
    state->value_22 = 0;
    /* Preserve the observed halfword read and XOR reset. A plain zero store
     * drops the retail read; the same reset form occurs in func_0027F75C. */
    state->value_20 ^= state->value_20;
    state->value_1C = func_00261560(4);
}
