#include "game/effect_commands_e09.h"
#include "game/effect_presentation_p04.h"

typedef struct E09RecordD6C8 {
    u16 value_00, value_02, value_04, value_06, value_08;
} E09RecordD6C8;
typedef struct E09StateD6C8 {
    u16 value_00, value_02, value_04, value_06, value_08;
    s16 value_0A, value_0C, value_0E, value_10;
    u8 unknown_12[2];
    s16 *value_14;
    s16 *volatile value_18;
    s16 value_1C;
} E09StateD6C8;
extern s16 *volatile e09_pointer_80220F68;
extern s16 *e09_pointer_80220F6C;
extern void func_00262B68(void);
extern void func_002600E8(u16, u16 *);
extern void func_00260094(s32, u16 *);

s32 func_0025D6C8(u16 command, E09StateD6C8 *state)
{
    E09RecordD6C8 record;
    s16 next;
    volatile E09StateD6C8 *initial;
    switch (command) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_18 = e09_pointer_80220F6C;
        state->value_14 = e09_pointer_80220F68;
        /* Separate ordered stores retain KMC initialization scheduling. */
        initial = state;
        initial->value_06 = 255;
        initial->value_04 = 255;
        initial->value_02 = 255;
        initial->value_08 = 200;
        initial->value_0E = 0;
        initial->value_0C = 0;
        initial->value_0A = 0;
        initial->value_10 = 0;
        initial->value_1C = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_00262C64(state->value_18[0], 0x1001);
        break;
    case 0x1001:
        record.value_00 = state->value_14[state->value_1C];
        record.value_02 = record.value_04 = record.value_06 = 255;
        record.value_08 = 200;
        func_002600E8(4, (u16 *)&record);
        state->value_1C++;
        func_00262C64(state->value_18[state->value_1C], 0x1002);
        break;
    case 0x1002:
        state->value_00 = state->value_14[state->value_1C];
        func_00260094(4, (u16 *)state);
        next = state->value_18[state->value_1C + 2] == -1 ? 0x1003 : 0x1001;
        state->value_1C++;
        func_00262C64(state->value_18[state->value_1C], next);
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
