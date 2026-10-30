#include "game/effect_callbacks_p02.h"

typedef struct P02State2702A0 {
    u8 unknown_00[0x1B];
    u8 value_1B;
    u8 unknown_1C[3];
    u8 value_1F;
    u32 value_20;
    u8 unknown_24[0x50];
    u8 *value_74;
} P02State2702A0;

extern s16 func_0024BCE8(u8);
extern s16 func_0024C3EC(u8);
/* Neutral address views, without a claim of distinct objects or extents. */
extern u8 p02_data_80220A10[], p02_data_80220A14[], p02_data_80220A16[];
extern u8 p02_data_80220A1C[], p02_data_80220A22[], p02_data_80220A28[];
extern u8 p02_data_80220A2E[], p02_data_80220A3A[], p02_data_80220A40[];
extern u8 p02_data_80220A46[], p02_data_80220A4C[];
extern s32 p02_word_80220A58[], p02_word_80220A70[];

void  func_002702A0(void *stateView)
{
    /* This helper uses the documented offset view of its caller's payload. */
    P02State2702A0 *state = stateView;
    s32 first, code;
    u32 i;

    first = func_0024BCE8(state->value_1B);
    if (first == func_0024BCE8(state->value_1F)) {
        state->value_74 = p02_data_80220A14;
        return;
    }
    code = func_0024C3EC(state->value_1F);
    /* Keep constant-address exits after both scans to preserve the retail
     * layout with the pinned compiler. */
    i = 0;
    for (; i < 6; i++) {
        if (p02_word_80220A58[i] == code) {
            if (first) {
                if (!(state->value_20 & 1)) {
                    state->value_74 = p02_data_80220A10;
                    return;
                }
                goto select_A16;
            }
            if (!(state->value_20 & 1)) {
                state->value_74 = p02_data_80220A10 + 0x24;
                return;
            }
            goto select_A3A;
        }
    }
    /* Keep constant-address exits after both scans to preserve the retail
     * layout with the pinned compiler. */
    i = 0;
    for (; i < 6; i++) {
        if (p02_word_80220A70[i] == code) {
            if (first) {
                if (!(state->value_20 & 1)) {
                    state->value_74 = p02_data_80220A2E;
                    return;
                }
                goto select_A28;
            }
            if (!(state->value_20 & 1)) {
                state->value_74 = p02_data_80220A2E + 0x24;
                return;
            }
            goto select_A4C;
        }
    }
    if (first) {
        if (state->value_20 & 1) {
            state->value_74 = p02_data_80220A1C;
            return;
        }
        state->value_74 = p02_data_80220A22;
        return;
    }
    if (state->value_20 & 1) {
        state->value_74 = p02_data_80220A40;
        return;
    }
    goto select_A46;
select_A16:
    state->value_74 = p02_data_80220A16;
    return;
select_A3A:
    state->value_74 = p02_data_80220A3A;
    return;
select_A28:
    state->value_74 = p02_data_80220A28;
    return;
select_A4C:
    state->value_74 = p02_data_80220A4C;
    return;
select_A46:
    state->value_74 = p02_data_80220A46;
}
