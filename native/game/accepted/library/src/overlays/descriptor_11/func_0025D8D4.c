#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_math_e04.h"

typedef struct E09StateD8D4 {
    float value_00, value_04, value_08;
    s16 value_0C;
    s16 value_0E, value_10;
    u8 value_12;
} E09StateD8D4;
extern volatile u16 e09_u16_80220F70, e09_u16_80220F72;
extern u8 e09_byte_80220F68;
extern volatile u8 e09_byte_80220F74;
extern s16 *e09_pointer_80220F6C;


extern void func_00262B68(void);

s32 func_0025D8D4(u16 command, E09StateD8D4 *state)
{
    s16 index;
    volatile E09StateD8D4 *initial;
    s32 first, last;
    u8 value;
    s32 guard;
    switch (command) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Ordered access views retain the three retail initialization pairs. */
        initial = state;
        initial->value_0C = e09_u16_80220F70;
        initial->value_0E = e09_u16_80220F72;
        state->value_12 = e09_byte_80220F74;
        if (state->value_12 & 0x10) {
            func_0024BF04(e09_byte_80220F68, (struct P01Vec3 *)state, 0x20);
        } else {
            func_0024C204((u8 *)e09_pointer_80220F6C, (struct P01Vec3 *)state, 0x20);
        }
        state->value_10 = 0;
        func_00262830(0x1001);
        break;
    case 0x1001:
        if (state->value_0E < 0) {
            func_00262830(0x1002);
            break;
        }
        func_00262C64(state->value_0E, 0x1002);
        /* fall through */
    case 0x1002:
        index = state->value_10;
        if (index < state->value_0C) {
            first = 255;
            last = 0;
            if (state->value_12 & 1) {
                first = 0;
                last = 255;
            }
            index++;
            /* Keep this store before the second duration read in KMC. */
            state->value_10 = index;
            value = first + (last - first) * index / state->value_0C;
            /* The word view reuses the byte projection for both tests. */
            guard = value;
            if (guard != 0) {
                if (guard >= 255) value = 255;
            }
            func_0026CCB4(state->value_00, state->value_04, state->value_08, (u8)value);
        } else {
            func_00262830(0x1003);
        }
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
