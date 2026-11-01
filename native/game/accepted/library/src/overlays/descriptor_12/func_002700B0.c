#include "game/effect_commands_e09.h"
#include "game/effect_dispatch_e07.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"

typedef struct E09Vector700B0 {
    float value_00, value_04, value_08;
    u8 unknown_0C[0x34];
    float value_40;
} E09Vector700B0;
typedef struct E09Object700B0 {
    u8 unknown_00[8];
    u16 value_08, value_0A;
    u8 unknown_0C[4];
    u32 value_10;
    u8 unknown_14[8];
    E09Vector700B0 value_1C;
    u16 value_60, value_62, value_64;
} E09Object700B0;
typedef struct E09State700B0 {
    u8 unknown_00[0x40];
    /* This pointer view retains the measured reload after object stores. */
    E09Object700B0 *volatile value_40;
    u32 value_44;
    s16 value_48, value_4A;
    u16 value_4C, value_4E, value_50, value_52;
    u16 value_54, value_56, value_58, value_5A;
    s32 value_5C;
} E09State700B0;
extern volatile void *g_func_0025C8A4_saved_arg;

extern void func_002623C4(void *);
extern void func_00262B68(void);
extern void func_0025FD90(s16, s16 *, s16 *);

s32 func_002700B0(u16 command, E09State700B0 *state)
{
    E09Object700B0 *object;
    E09Object700B0 *created;
    E09Vector700B0 *vector;
    s16 values[4];
    switch (command) {
    case 0x101:
        func_002623C4(state->value_40);
        func_00262B68();
        break;
    case 0x100:
        state->value_44 = (u32)g_func_0025C8A4_saved_arg;
        created = func_0026C010(0x54);
        state->value_40 = created;
        created->value_10 = state->value_44;
        state->value_40->value_0A |= 0x2000;
        state->value_4C = 64;
        state->value_4E = 255;
        state->value_50 = 128;
        state->value_52 = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        state->value_4A = 12;
        state->value_48 = 0;
        state->value_54 = 255;
        state->value_56 = 255;
        state->value_58 = 255;
        state->value_5A = 255;
        state->value_5C = 0;
        func_00262830(0x1001);
        /* fall through */
    case 0x1001:
        object = state->value_40;
        func_0025FD90(state->value_48, &state->value_4A, values);
        object->value_60 = values[0];
        object->value_62 = values[1];
        object->value_64 = values[2];
        vector = &object->value_1C;
        if (state->value_5C == 0) {
            func_0025F058(state, vector);
            state->value_5C++;
        } else if (state->value_5C == 1) {
            state->value_40->value_08 &= 0xFFFE;
            state->value_5C++;
        }
        vector->value_40 = values[3] / 255.0f;
        state->value_48++;
        if (state->value_48 > state->value_4A) func_00262830(0x101);
        break;
    }
    return 0;
}
