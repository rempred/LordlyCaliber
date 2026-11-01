#include "game/effect_callbacks_p02.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_handlers_p07.h"

typedef struct P07Row7351C {
    s16 value_00, value_02;
    float value_04, value_08, value_0C;
} P07Row7351C;
typedef struct P07State7351C {
    u8 *value_00, *value_04;
    u32 value_08, value_0C, value_10, value_14, value_18, value_1C;
    u32 value_20, value_24;
} P07State7351C;
/* Array-style numeric views preserve aliasing with the state during payload
 * setup. Scalar globals let GCC 2.7.2 move the index reads before these stores.
 * These views do not establish array extents or original declaration shapes. */
extern u8 *p07_pointer_80220F68[], *p07_pointer_80220F6C[];
extern u32 p07_word_80220F70[];
extern float p07_float_80220F70[], p07_float_80220F74[], p07_float_80220F78[];
extern u32 p07_word_80220F7C[], p07_word_80220F80[];
extern P07Row7351C p07_data_80220AC0[];
extern void func_00262B68(void);


s32 func_0027351C(u16 message, P07State7351C *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_00 = p07_pointer_80220F68[0];
        state->value_04 = p07_pointer_80220F6C[0];
        state->value_08 = p07_word_80220F70[0];
        state->value_20 = *state->value_00;
        state->value_24 = *state->value_04;
        func_00262830(0x1000);
        break;
    case 0x1000:
        state->value_10 = 0;
        state->value_14 = 0;
        state->value_1C = 0;
        func_00262830(0x1001);
        /* fall through */
    case 0x1001:
        if (state->value_10 < 3) {
            p07_pointer_80220F68[0] = (u8 *)state->value_20;
            p07_pointer_80220F6C[0] = (u8 *)state->value_24;
            p07_float_80220F70[0] = p07_data_80220AC0[state->value_10].value_04;
            p07_float_80220F74[0] = p07_data_80220AC0[state->value_10].value_08;
            p07_float_80220F78[0] = p07_data_80220AC0[state->value_10].value_0C;
            p07_word_80220F7C[0] = state->value_0C;
            p07_word_80220F80[0] = state->value_18;
            func_00262AB0((void *)func_00273180);
            func_00262C64(p07_data_80220AC0[state->value_10].value_02, 0x1001);
            state->value_10++;
        } else {
            func_00262830(0x1002);
        }
        break;
    case 0x1002:
        if (state->value_1C == 4) {
            func_00262830(0x101);
        }
        break;
    case 0x1004:
        state->value_1C++;
        break;
    }
    return 0;
}
