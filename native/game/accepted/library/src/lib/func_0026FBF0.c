#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02VecFBF0 { float x, y, z; } P02VecFBF0;
typedef struct P02SlotFBF0 {
    u8 unknown_00[0x56];
    u16 value_56, unknown_58, unknown_5A, value_5C;
} P02SlotFBF0;
typedef struct P02OtherFBF0 {
    u8 unknown_00[0x60];
    u16 value_60;
} P02OtherFBF0;
typedef struct P02RenderFBF0 {
    u8 unknown_00[0x1C];
    float value_1C[16];
} P02RenderFBF0;
/* Separate views retain retail's repeated integer accesses and direct FP
 * conversion with this compiler; original volatile qualification is unproven. */
typedef union P02CounterFBF0 { volatile s32 word; s32 value; } P02CounterFBF0;
typedef struct P02StateFBF0 {
    P02CounterFBF0 value_00;
    s32 value_04, value_08, value_0C;
    P02VecFBF0 value_10, value_1C;
    P02SlotFBF0 *value_28, *value_2C;
    void *value_30;
    P02OtherFBF0 *value_34;
    P02RenderFBF0 *value_38;
    float value_3C;
    P02VecFBF0 value_40;
    s32 value_4C;
    void *value_50;
} P02StateFBF0;

extern void func_002623C4(void *), func_002630CC(void *);
extern void func_00261E30(void *), func_000016C4(void *);
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void *D_801D0770;
extern s32 p02_word_80220F68[];
extern u32 p02_word_80220F78[];
extern float p02_f32_80220F6C[], p02_f32_80220F70[], p02_f32_80220F74[];

s32 func_0026FBF0(u16 message, P02StateFBF0 *state)
{
    s32 result = 0;
    s32 duration, count, amount, intensity, index;
    P02VecFBF0 value;
    void *matrix;
    P02SlotFBF0 *first;

    switch (message) {
    case 0x101:
        if (state->value_0C) {
            func_002623C4(state->value_38);
            func_002630CC(state->value_28);
            func_002630CC(state->value_2C);
            func_002630CC(state->value_30);
            func_00261E30(state->value_34);
        }
        if (D_801D0770 != state->value_50)
            func_000016C4(state->value_50);
        func_00262B68();
        break;
    case 0x100:
        state->value_08 = p02_word_80220F68[0];
        state->value_1C.x = p02_f32_80220F6C[0];
        state->value_1C.y = p02_f32_80220F70[0];
        state->value_1C.z = p02_f32_80220F74[0];
        index = p02_word_80220F78[0];
        state->value_28 = state->value_2C = state->value_30 = 0;
        state->value_34 = 0;
        state->value_00.word = state->value_0C = 0;
        state->value_04 = 0x40;
        state->value_4C = index;
        state->value_50 = (void *)func_00200474();
        func_00262830(0x1000);
        break;
    case 0x1000:
    case 0x1001:
    case 0x1002:
        break;
    case 0x1003:
        count = state->value_00.word + 1;
        duration = state->value_04 - 30;
        state->value_00.word = count;
        if (count >= duration) {
            state->value_00.word = duration;
            func_00262830(0x1001);
        }
        amount = -180 * state->value_00.word;
        amount /= duration;
        count = state->value_00.word;
        intensity = count * 256;
        intensity -= count;
        intensity /= duration;
        state->value_3C = amount + 200;
        state->value_40.x = state->value_40.z = (float)state->value_00.value / 64.0f;
        state->value_40.y = state->value_3C / 120.0f;
        func_0026CCB4(state->value_1C.x, state->value_1C.y, state->value_1C.z, intensity);
        matrix = state->value_38->value_1C;
        if (!state->value_08) {
            value.x = 0.0f;
            value.y = 7.071f;
            value.z = -7.071f;
        } else {
            value.x = 0.0f;
            value.z = 0.0f;
            value.y = 10.0f;
        }
        func_0025F530(&value, &state->value_10, matrix);
        func_0025F080(&state->value_40, matrix, matrix);
        func_0025F150(matrix, &state->value_1C);
        if (!state->value_08) goto update_first;
        goto update_other;
    case 0x1004:
        count = state->value_00.word - 1;
        duration = state->value_04 - 30;
        state->value_00.word = count;
        if (count <= 0) {
            state->value_00.word = 0;
            func_00262830(0x1002);
        }
        count = state->value_00.word;
        intensity = count * 256;
        intensity -= count;
        intensity /= duration;
        state->value_40.x = state->value_40.z = (float)state->value_00.value / 64.0f;
        state->value_40.y = state->value_3C / 120.0f;
        func_0026CCB4(state->value_1C.x, state->value_1C.y, state->value_1C.z, intensity);
        matrix = state->value_38->value_1C;
        if (!state->value_08) {
            value.x = 0.0f;
            value.y = 7.071f;
            value.z = -7.071f;
        } else {
            value.x = 0.0f;
            value.z = 0.0f;
            value.y = 10.0f;
        }
        func_0025F530(&value, &state->value_10, matrix);
        func_0025F080(&state->value_40, matrix, matrix);
        func_0025F150(matrix, &state->value_1C);
        if (state->value_08) goto update_other;
update_first:
        first = state->value_28;
        state->value_2C->value_5C = state->value_00.word + 2;
        first->value_56 = state->value_00.word;
        break;
update_other:
        state->value_34->value_60 = state->value_00.word;
        break;
    case 0x1005:
        if (*(u16 *)((u8 *)state - 0x1C) == 0x1001) result = 1;
        break;
    case 0x1006:
        if (*(u16 *)((u8 *)state - 0x1C) == 0x1002) result = 1;
        break;
    case 0x1007:
    case 0x1008:
        if (!state->value_0C) {
            func_0026F990(state);
            state->value_0C = 1;
        }
        if (message == 0x1007) func_00262830(0x1003);
        else func_00262830(0x1004);
        break;
    }
    return result;
}
