#include "game/effect_adapters_e08.h"
#include "game/effect_handlers_p07.h"
#include "game/effect_callbacks_p02.h"

typedef union P07Word7370C { u32 word; u8 byte[4]; s16 halfword[2]; } P07Word7370C;
typedef struct P07State7370C {
    u32 value_00, value_04, value_08;
    void *value_0C;
    u32 value_10, value_14, value_18, value_1C;
    P07Word7370C value_20, value_24;
} P07State7370C;
extern s32 p07_data_80220AF0[];
extern u8 p07_data_800EB290[], p07_data_800EB240[];
extern u32 p07_word_80220F68[], p07_word_80220F6C[];


extern void func_0020BDCC(s16, s32);


s32 func_0027370C(u16 message, P07State7370C *state)
{
    s32 result = 0;
    switch (message) {
    case 0x100:
        state->value_0C = p07_data_80220AF0;
        state->value_18 = 1;
        func_0027351C(0x100, (struct P07State7351C *)state);
        func_0024E9BC((s32)p07_data_800EB290, state->value_20.byte[3], 0x10D);
        break;
    case 0x1003:
        if (state->value_14 == 0) {
            u32 value;
            state->value_14 = 1;
            func_0020BDCC(state->value_24.halfword[1], 10);
            value = state->value_24.word;
            p07_word_80220F6C[0] = 0;
            p07_word_80220F68[0] = value;
            func_00262AB0((void *)func_00272DFC);
            func_0024E9BC((s32)p07_data_800EB240, state->value_20.byte[3], 0x10E);
        }
        break;
    default:
        result = func_0027351C(message, (struct P07State7351C *)state);
        break;
    }
    return result;
}
