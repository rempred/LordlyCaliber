#include "game/effect_callbacks_p02.h"

/* This dispatcher supplies a fourth ordinal argument; some retail creators
 * do not consume it. Keep that physical call interface explicit. */
typedef void *(*P02CreateF578)(s16, u32, u32, s32);

typedef struct P02ParametersF578 {
    s16 value_00;
    u8 value_02, unknown_03;
    u8 *value_04;
    P02CreateF578 value_08;
} P02ParametersF578;

typedef struct P02SlotF578 {
    u32 value_00;
    void *value_04;
} P02SlotF578;

typedef struct P02StateF578 {
    P02ParametersF578 value_00;
    s32 value_0C, value_10, value_14, value_18;
    P02SlotF578 value_1C[8];
} P02StateF578;

extern void func_00262B68(void);
extern void func_00023780(void *, s32);
extern void func_00262830(s32);
extern s32 func_0026285C(void *, u16);
extern P02ParametersF578 p02_parameters_80220F68;

s32 func_0026F578(u16 message, P02StateF578 *state)
{
    s32 result = 0;
    s32 i, count;
    u32 value;
    P02SlotF578 *slot;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        func_00023780(state, 0x5C);
        state->value_00 = p02_parameters_80220F68;
        state->value_18 = 1;
        func_00262830(0x1000);
        break;
    case 0x1000:
        value = state->value_00.value_04[state->value_10];
        if (value != 0xFF) {
            ++state->value_10;
            slot = &state->value_1C[state->value_14];
            /* Retail tests once, then branches to an unconditional self-loop
             * for an occupied slot; keep that one-test control flow explicit. */
            if (slot->value_04 != 0) {
                for (;;) {
                }
            }
            slot->value_04 = state->value_00.value_08(
                state->value_00.value_00, state->value_00.value_02,
                value, state->value_0C++);
            if (++state->value_14 >= 8)
                state->value_14 = 0;
            --state->value_18;
        } else {
            state->value_18 = 0;
        }
        func_00262830(0x1001);
        /* fall through */
    case 0x1001:
        /* This real initialization order preserves the pinned setup. */
        i = 0;
        count = 0;
        for (; i < 8; ++i) {
            slot = &state->value_1C[i];
            if (slot->value_04) {
                ++count;
                if (!(slot->value_00 & 1)) {
                    if (func_0026285C(slot->value_04, 0x1000)) {
                        slot->value_00 |= 1;
                        ++state->value_18;
                        func_00262830(0x1000);
                    }
                }
                if (func_0026285C(slot->value_04, 0x1001)) {
                    func_0026285C(slot->value_04, 0x101);
                    func_00023780(slot, 8);
                }
            }
        }
        if (!count && !state->value_18
            && state->value_00.value_04[state->value_10] == 0xFF)
            func_00262830(0x1002);
        break;
    case 0x1003:
        if (*(u16 *)((u8 *)state - 0x1C) == 0x1002)
            result = 1;
        break;
    case 0x1002:
        break;
    }
    return result;
}
