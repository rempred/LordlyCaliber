#include "game/effect_commands_e09.h"
#include "game/effect_presentation_p04.h"

typedef struct E09ObjectC8D0 {
    u8 unknown_00[0xAB];
    u8 value_AB;
    u8 value_AC;
} E09ObjectC8D0;
typedef struct E09StateC8D0 {
    /* Access views preserve measured stores/reloads, not runtime mutability. */
    E09ObjectC8D0 *volatile value_00;
    volatile u8 value_04;
    u8 value_05, value_06, unknown_07;
    s16 value_08;
} E09StateC8D0;

extern volatile u8 e09_byte_80220F68;
extern volatile u8 e09_byte_80220F69;
extern volatile u8 e09_byte_80220F6A;
extern void *func_0020C478(u32);
extern void func_00262B68(void);

s32 func_0025C8D0(u16 command, E09StateC8D0 *state)
{
    s16 index;
    s32 first, last;
    s32 value;
    E09ObjectC8D0 *object;
    switch (command) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_00 = func_0020C478(e09_byte_80220F68);
        state->value_04 = e09_byte_80220F69;
        state->value_05 = e09_byte_80220F6A;
        /* KMC must retain these two initialization stores in this order. */
        ((volatile E09StateC8D0 *)state)->value_06 = 2;
        ((volatile E09StateC8D0 *)state)->value_08 = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        index = state->value_08;
        if (index < state->value_04) {
            first = 255;
            last = 0;
            if (state->value_06 & 1) {
                first = 0;
                last = 255;
            }
            index++;
            value = first + (last - first) * index / state->value_04;
            state->value_08 = index;
            if ((value & 255) != 0) {
                if ((value & 255) >= 255) value = 255;
            }
            object = state->value_00;
            object->value_AC = value;
            object->value_AB = value;
        } else if (state->value_06 & 1) {
            func_00262C64(1, 0x1001);
        } else {
            state->value_08 = 0;
            state->value_06 = 1;
            func_00262C64(state->value_05, 0x1000);
        }
        break;
    case 0x1001:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
