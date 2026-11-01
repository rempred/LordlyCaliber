#include "game/effect_adapters_e08.h"
#include "game/effect_dispatch_e07.h"
#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02Vec3 { float x, y, z; } P02Vec3;
typedef struct P02MotionDCF0 {
    P02Vec3 value_00, value_0C;
    float value_18, value_1C, value_20;
} P02MotionDCF0;
typedef struct P02RenderDCF0 {
    u8 unknown_00[0xA];
    u16 flags_0A;
    u8 unknown_0C[4];
    u32 value_10;
    u8 unknown_14[8];
    float value_1C[16];
} P02RenderDCF0;
typedef struct P02StateDCF0 {
    P02Vec3 value_00, value_0C, value_18;
    u8 unknown_24[3];
    u8 value_27;
    u32 value_28;
    s32 value_2C;
    P02RenderDCF0 *value_30;
    s16 value_34;
    u16 value_36;
    P02MotionDCF0 value_38;
} P02StateDCF0;

extern void func_00262B68(void);
extern void func_00023780(void *, s32);

extern void func_00262830(s32);

extern void func_0026241C(P02RenderDCF0 *, s32, s32);

extern void func_002623C4(P02RenderDCF0 *);
extern void *func_0020C478(u32);

extern void func_0020BDCC(s16, s32);
extern u8 *p01_context_80220F68;
extern u8 D_800EB290[], D_800EB240[];

/* Keep the complete phase inline to preserve its pointer and vector lifetimes. */
static __inline__ void p02_message_1006(P02StateDCF0 *state)
{
    P02Vec3 target;
    void *pointer;
    P02RenderDCF0 *object;
    s32 value;

    func_002623C4(state->value_30);
    pointer = func_0020C478(state->value_28);
    func_0026C7C4(pointer, &target);
    object = func_00269798(0x35);
    object->flags_0A |= 0x8000;
    func_0026C684(pointer, object->value_1C);
    pointer = object->value_1C;
    func_0025F150(pointer, &target);
    object = func_0026a45c(0x36);
    object->flags_0A |= 0x8000;
    func_0026C3F8(object->value_1C);
    pointer = object->value_1C;
    func_0025F150(pointer, &target);
    func_0020BDCC((s16)state->value_28, 10);
    value = 0xE0;
    switch (state->value_2C) {
    case 14: value = 0xE2; break;
    case 15: value = 0xE4; break;
    }
    func_0024E9BC((s32)D_800EB240, state->value_27, value);
    func_00262830(0x1007);
}

s32 func_0026dcf0(u16 message, P02StateDCF0 *state)
{
    s32 result = 0;
    P02Vec3 position;
    void *matrix;
    s32 value;
    s32 i;
    float zero;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        func_00023780(state, 0x5C);
        state->value_30 = func_0026BF74(0x37);
        state->value_00.y = state->value_00.x =
            state->value_18.x = state->value_18.y = state->value_18.z = 0.0f;
        state->value_00.z = 1.0f;
        state->value_0C.x = state->value_0C.y = state->value_0C.z = 1.0f;
        func_00262830(0x1002);
        break;
    case 0x1002:
        func_00262830(0x1003);
        /* Fall through. */
    case 0x1003: {
        P02MotionDCF0 *motion = &state->value_38;
        if (func_0024BCE8(state->value_27))
            state->value_18.x = 75.0f;
        else
            state->value_18.x = -75.0f;
        zero = state->value_18.y = state->value_18.z = 0.0f;
        func_0026C834((u32)state->value_28, &position);
        state->value_00.x = position.x - state->value_18.x;
        state->value_00.y = position.y - state->value_18.y;
        state->value_00.z = position.z - state->value_18.z;
        motion->value_1C = func_0025EF78(&state->value_00);
        func_0025EE90(&state->value_00, 1.0f / motion->value_1C, &motion->value_0C);
        motion->value_18 = zero;
        motion->value_20 = 6.0f;
        motion->value_1C -= 30.0f;
        motion->value_00.x = state->value_18.x;
        motion->value_00.y = state->value_18.y;
        motion->value_00.z = state->value_18.z;
        /* Keep the object-to-matrix pointer in two steps for the retail register. */
        matrix = state->value_30;
        matrix = (u8 *)matrix + 0x1C;
        func_0026C230(state, matrix);
        func_0025F080(&state->value_0C, matrix, matrix);
        func_0025F150(matrix, &state->value_18);
        func_00262830(0x1004);
        break;
    }
    case 0x1004:
        if ((s32)state->value_30->value_10 >= 20) {
            func_0026241C(state->value_30, 0x15, 0x24);
            value = 0xDF;
            switch (state->value_2C) {
            case 14: value = 0xE1; break;
            case 15: value = 0xE3; break;
            }
            func_0024E9BC((s32)D_800EB290, state->value_27, value);
            func_00262830(0x1005);
        }
        matrix = state->value_30;
        matrix = (u8 *)matrix + 0x1C;
        func_0026C230(state, matrix);
        func_0025F080(&state->value_0C, matrix, matrix);
        func_0025F150(matrix, &state->value_18);
        break;
    case 0x1005: {
        P02MotionDCF0 *motion;
        float *values;
        state->value_38.value_18 += state->value_38.value_20;
        motion = &state->value_38;
        values = (float *)&motion->value_00;
        if (motion->value_18 > motion->value_1C) {
            motion->value_18 = motion->value_1C;
            func_00262830(0x1006);
        }
        for (i = 0; i < 3; ++i)
            ((float *)&state->value_18)[i] = values[i + 3] * motion->value_18 + values[i];
        matrix = state->value_30;
        matrix = (u8 *)matrix + 0x1C;
        func_0026C230(state, matrix);
        func_0025F080(&state->value_0C, matrix, matrix);
        func_0025F150(matrix, &state->value_18);
        if (motion->value_18 >= motion->value_1C / 2.0f)
            state->value_36 = 1;
        if (++state->value_34 >= 5) {
            void *created;
            state->value_34 = 0;
            p01_context_80220F68 = (u8 *)state->value_30->value_10;
            created = func_00262AB0((void *)func_0026e520);
            func_0025F058(state->value_30->value_1C, (u8 *)created + 0x24);
        }
        break;
    }
    case 0x1006:
        p02_message_1006(state);
        break;
    /* The explicit idle phase preserves the retail decision tree. */
    case 0x1007:
        break;
    case 0x1000:
        result = state->value_36;
        break;
    case 0x1001:
        if (*(u16 *)((u8 *)state - 0x1C) == 0x1007)
            result = 1;
        break;
    }
    return result;
}



