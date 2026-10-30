#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02Vec3 { float x, y, z; } P02Vec3;
typedef struct P02SlotD918 {
    u8 unknown_00[0x56];
    u16 value_56;
    u16 unknown_58, unknown_5A;
    u16 value_5C;
} P02SlotD918;
typedef struct P02RenderD918 {
    u8 unknown_00[0x1C];
    float value_1C[16];
} P02RenderD918;
typedef struct P02StateD918 {
    /* Retail rereads this counter between arithmetic stages. */
    volatile s32 value_00;
    s32 value_04;
    P02Vec3 value_08;
    P02SlotD918 *value_14;
    P02SlotD918 *value_18;
    void *value_1C;
    P02RenderD918 *value_20;
    float value_24;
    P02Vec3 value_28;
    s32 value_34, value_38;
    u32 value_3C;
    void *value_40;
} P02StateD918;

extern void func_002623C4(void *);
extern void func_002630CC(void *);
extern void func_000016C4(void *);
extern void func_00262B68(void);
extern void func_00023780(void *, s32);
extern void func_00262830(s32);
extern void *D_801D0770;
extern float p02_f32_80220F68[], p02_f32_80220F6C[], p02_f32_80220F70[];
extern s32 p02_word_80220F74[], p02_word_80220F7C[];
extern u32 p02_word_80220F78[];
extern P02Vec3 p02_data_802206E8, p02_data_802206F4;

s32 func_0026d918(u16 message, P02StateD918 *state)
{
    s32 result = 0;
    s32 duration;
    s32 intensity;
    s32 amount;
    s32 count;
    void *matrix;
    P02SlotD918 *first;

    switch (message) {
    case 0x101:
        if (state->value_04) {
            func_002623C4(state->value_20);
            func_002630CC(state->value_14);
            func_002630CC(state->value_18);
            func_002630CC(state->value_1C);
        }
        if (D_801D0770 != state->value_40)
            func_000016C4(state->value_40);
        func_00262B68();
        break;
    case 0x100:
        func_00023780(state, 0x44);
        state->value_08.x = p02_f32_80220F68[0];
        state->value_08.y = p02_f32_80220F6C[0];
        state->value_08.z = p02_f32_80220F70[0];
        state->value_38 = p02_word_80220F74[0];
        state->value_3C = p02_word_80220F78[0];
        state->value_34 = p02_word_80220F7C[0];
        state->value_00 = 0;
        state->value_40 = (void *)func_00200474();
        func_00262830(0x1000);
        break;
    /* Explicit idle cases preserve the retail decision tree. */
    case 0x1000:
    case 0x1001:
    case 0x1002:
        break;
    case 0x1003:
        duration = state->value_38;
        count = state->value_00 + 1;
        state->value_00 = count;
        if (count >= duration) {
            state->value_00 = duration;
            func_00262830(0x1001);
        }
        amount = -160 * state->value_00;
        amount /= duration;
        /* Keep the 255-product in two steps for the retail word registers. */
        count = state->value_00;
        intensity = count * 256;
        intensity -= count;
        intensity /= duration;
        state->value_24 = amount + 200;
        state->value_28.x = state->value_28.z = (float)(state->value_00 + 1) / 64.0f;
        state->value_28.y = state->value_24 / 120.0f;
        /* Joining before argument preparation sinks the integer division;
         * keep this real call in each arm for the pinned compiler. */
        func_0026CCB4(state->value_08.x, state->value_08.y, state->value_08.z, intensity);
        goto update_objects;
    case 0x1004:
        duration = state->value_38;
        count = state->value_00 - 1;
        state->value_00 = count;
        if (count <= 0) {
            state->value_00 = 0;
            func_00262830(0x1002);
        }
        count = state->value_00;
        intensity = count * 256;
        intensity -= count;
        intensity /= duration;
        state->value_28.x = state->value_28.z = (float)(state->value_00 + 1) / 64.0f;
        state->value_28.y = state->value_24 / 120.0f;
        func_0026CCB4(state->value_08.x, state->value_08.y, state->value_08.z, intensity);
update_objects:
        matrix = state->value_20->value_1C;
        func_0025F530(&p02_data_802206E8, &p02_data_802206F4, matrix);
        func_0025F080(&state->value_28, matrix, matrix);
        func_0025F150(matrix, &state->value_08);
        first = state->value_14;
        state->value_18->value_5C = state->value_00 + 2;
        first->value_56 = state->value_00;
        break;
    case 0x1005:
        if (*(u16 *)((u8 *)state - 0x1C) == 0x1001)
            result = 1;
        break;
    case 0x1006:
        if (*(u16 *)((u8 *)state - 0x1C) == 0x1002)
            result = 1;
        break;
    case 0x1007:
    case 0x1008:
        if (!state->value_04) {
            func_0026d770(state);
            state->value_04 = 1;
        }
        if (message == 0x1007)
            func_00262830(0x1003);
        else
            func_00262830(0x1004);
        break;
    }
    return result;
}






