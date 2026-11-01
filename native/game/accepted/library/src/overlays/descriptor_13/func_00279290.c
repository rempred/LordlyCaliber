#include "game/effect_presentation_p06_internal.h"

#include "game/effect_presentation_p04.h"

typedef struct P06Object79290 {
    u8 unknown_00[0xA];
    u16 value_0A;
    u8 unknown_0C[0x10];
    float value_1C[4][4];
    float value_5C;
    u16 value_60, value_62, value_64;
} P06Object79290;
typedef struct P06State79290 {
    float value_00[4][4];
    float value_40;
    s16 value_44;
    u8 unknown_46[2];
    P06Object79290 *value_48;
} P06State79290;
typedef struct P06Context79290 {
    u16 value_00;
    u8 unknown_02[2];
    float value_04[4][4];
} P06Context79290;
extern P06Context79290 p06_context_80220F68;
extern u16 p06_table_80225840[][3];
s32 func_00279290(u16 message, P06State79290 *state)
{
    s32 result = 0;
    P06Object79290 *object;
    float value;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_44 = p06_context_80220F68.value_00;
        state->value_40 = 0.25f;
        func_0025F058(p06_context_80220F68.value_04, state->value_00);
        state->value_48 = func_0026C010(0xF3);
        func_00262830(0x1000);
        break;
    case 0x1000:
        state->value_48->value_0A |= 0x2000;
        object = state->value_48;
        func_0025F058(state->value_00, object->value_1C);
        object->value_5C = 1.0f;
        object->value_60 = p06_table_80225840[state->value_44][0];
        object->value_62 = p06_table_80225840[state->value_44][1];
        object->value_64 = p06_table_80225840[state->value_44][2];
        func_00262830(0x1001);
        break;
    case 0x1001:
        value = state->value_48->value_5C - state->value_40;
        state->value_48->value_5C = value;
        if (value <= 0.0f) {
            func_002623C4(state->value_48);
            func_00262830(0x1002);
        }
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    default:
        result = 0;
        break;
    }
    return result;
}
