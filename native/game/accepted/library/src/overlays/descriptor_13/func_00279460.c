#include "game/effect_presentation_p06_internal.h"

#include "game/effect_presentation_p04.h"

typedef struct P06Vec79460 { float x, y, z; } P06Vec79460;
typedef struct P06Object79460 {
    u8 unknown_00[0xA];
    u16 value_0A;
    u8 unknown_0C[0x10];
    float value_1C[4][4];
} P06Object79460;
typedef struct P06State79460 {
    u8 value_00, value_01, unknown_02[2];
    P06Vec79460 value_04, value_10, value_1C;
    float value_28;
    s16 value_2C;
    u8 unknown_2E[6];
    P06Object79460 *value_34;
} P06State79460;
typedef struct P06Context79460 {
    u16 value_00;
    u8 unknown_02[2];
    float value_04[4][4];
} P06Context79460;
extern u8 p06_byte_80220F68, p06_byte_80220F69;
extern P06Context79460 p06_context_80220F68;
s32 func_00279460(u16 message, P06State79460 *state)
{
    /* Preserve the three offset components and their zero-word stores. */
    union { P06Vec79460 vector; u32 words[3]; } offset;
    float matrix[4][4];
    P06Object79460 *object;
    float (*source)[4];
    float remaining;
    s16 selection;
    P06Context79460 *context;
    s32 result = 0;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->value_00 = p06_byte_80220F68;
        state->value_01 = p06_byte_80220F69;
        selection = func_0024BCE8(state->value_00);
        if (selection != 0) {
            offset.words[0] = 0;
            offset.vector.y = 16.0f;
            offset.words[2] = 0;
        } else {
            offset.words[0] = 0;
            offset.vector.y = 32.0f;
            offset.words[2] = 0;
        }
        func_0024C0DC(state->value_00, (struct P01Vec3 *)&state->value_04,
                     offset.vector.x, offset.vector.y, offset.vector.z);
        func_0024BF04(state->value_01, (struct P01Vec3 *)&state->value_10, 0x10);
        func_0024C7A4(&state->value_1C, &state->value_10, &state->value_04);
        state->value_28 = func_0025EF78(&state->value_1C);
        func_0025EFC8(&state->value_1C);
        state->value_2C = 45;
        func_00262830(0x1000);
        break;
    case 0x1000:
        object = func_0026BF74(0xF3);
        state->value_34 = object;
        object->value_0A |= 0x2000;
        func_0024C7D8((struct P01Vec3 *)&state->value_1C,
                     (struct P01PlacementObject *)object->value_1C);
        object->value_1C[3][0] = state->value_04.x + state->value_2C * state->value_1C.x;
        object->value_1C[3][1] = state->value_04.y + state->value_2C * state->value_1C.y;
        object->value_1C[3][2] = state->value_04.z + state->value_2C * state->value_1C.z;
        state->value_28 -= state->value_2C;
        func_00262C80();
        func_00262830(0x1001);
        break;
    case 0x1001: {
        /* This case consumes the object only until its matrix is captured. */
        P06Object79460 *current = state->value_34;
        current->value_1C[3][0] += state->value_2C * state->value_1C.x;
        current->value_1C[3][1] += state->value_2C * state->value_1C.y;
        current->value_1C[3][2] += state->value_2C * state->value_1C.z;
        remaining = state->value_28 - state->value_2C;
        source = current->value_1C;
        context = &p06_context_80220F68;
        state->value_28 = remaining;
        if (remaining < state->value_2C)
            func_00262830(0x1002);
        else
            func_00262830(0x1001);
        func_0025F058(source, matrix);
        matrix[3][0] -= state->value_2C * state->value_1C.x;
        matrix[3][1] -= state->value_2C * state->value_1C.y;
        matrix[3][2] -= state->value_2C * state->value_1C.z;
        func_0025F058(matrix, context->value_04);
        context->value_00 = func_0025F74C(state->value_00);
        func_00262AB0(func_00279290);
        break;
    }
    case 0x1002:
        func_002623C4(state->value_34);
        func_002628E4(0x101);
        break;
    default:
        result = 0;
        break;
    }
    return result;
}
