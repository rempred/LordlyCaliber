#include "game/effect_callbacks_p02.h"

typedef struct P02ObjectF1E0 {
    u8 unknown_00[0xAB];
    u8 value_AB, value_AC;
} P02ObjectF1E0;

typedef struct P02EntryF1E0 {
    P02ObjectF1E0 *value_00;
    u8 value_04, value_05;
    u16 unknown_06;
} P02EntryF1E0;

typedef struct P02StateF1E0 {
    s32 value_00;
    P02EntryF1E0 value_04[10];
    s32 value_54, value_58;
} P02StateF1E0;

extern void func_00262B68(void);
extern void *func_0020C478(u32);
extern s32 func_0020C014(void *);
extern void func_00262830(s32);
extern s32 p02_word_80220F68[], p02_word_80220F6C[];

s32 func_0026F1E0(u16 message, P02StateF1E0 *state)
{
    s32 result = 0;


    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    /* Keep the real iterators/object values local to each phase; sharing
     * them across cases changes the pinned compiler's register allocation. */
    case 0x100: {
        s32 i, count;
        s32 *parameters;
        P02ObjectF1E0 *object;
        parameters = p02_word_80220F68;
        /* The separate initializations retain the retail setup order. */
        i = 0;
        state->value_54 = p02_word_80220F6C[0];
        count = 0;
        for (; i < 20; ++i) {
            object = func_0020C478(i);
            if (object && func_0020C014(object) == parameters[0]) {
                state->value_04[count].value_00 = object;
                state->value_04[count].value_04 = object->value_AB;
                state->value_04[count].value_05 = object->value_AC;
                ++count;
            }
        }
        state->value_00 = count;
        state->value_58 = 0;
        func_00262830(0x1000);
        break;
    }
    case 0x1000: {
        s32 i;
        P02ObjectF1E0 *object;
        if (++state->value_58 >= state->value_54) {
            state->value_58 = state->value_54;
            func_00262830(0x1001);
        }
        for (i = 0; i < state->value_00; ++i) {
            object = state->value_04[i].value_00;
            object->value_AB = state->value_04[i].value_04
                + (-state->value_04[i].value_04 * state->value_58) / state->value_54;
            object->value_AC = state->value_04[i].value_05
                + (-state->value_04[i].value_05 * state->value_58) / state->value_54;
        }
        break;
    }
    case 0x1002: {
        s32 i;
        P02ObjectF1E0 *object;
        if (++state->value_58 >= state->value_54) {
            for (i = 0; i < state->value_00; ++i) {
                object = state->value_04[i].value_00;
                object->value_AB = state->value_04[i].value_04;
                object->value_AC = state->value_04[i].value_05;
            }
            func_00262830(0x101);
        } else {
            for (i = 0; i < state->value_00; ++i) {
                object = state->value_04[i].value_00;
                object->value_AB = (state->value_04[i].value_04 * state->value_58) / state->value_54;
                object->value_AC = (state->value_04[i].value_05 * state->value_58) / state->value_54;
            }
        }
        break;
    }
    case 0x1003:
        state->value_58 = 0;
        func_00262830(0x1002);
        break;
    case 0x1001:
        break;
    }
    return result;
}
