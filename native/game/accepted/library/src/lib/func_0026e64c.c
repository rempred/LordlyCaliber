#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02Vec3 { float x, y, z; } P02Vec3;

typedef struct P02MotionE64C {
    P02Vec3 value_00, value_0C;
    float value_18, value_1C, value_20;
    P02Vec3 value_24;
    float value_30[4], value_40[4], value_50[4];
} P02MotionE64C;

typedef struct P02RenderE64C {
    u8 unknown_00[0xA];
    u16 flags_0A;
    u8 unknown_0C[4];
    s32 value_10;
    u8 unknown_14[8];
    float value_1C[16];
} P02RenderE64C;

typedef struct P02StateE64C {
    P02Vec3 value_00, value_0C;
    u8 unknown_18[3];
    u8 value_1B;
    u32 value_1C;
    s32 value_20;
    P02RenderE64C *value_24;
    s16 value_28;
    u16 value_2A;
    P02MotionE64C *value_2C;
} P02StateE64C;

extern void func_000016C4(void *);
extern void func_00262B68(void);
extern void func_00023780(void *, s32);
extern void *func_00001330(u32);
extern P02RenderE64C *func_0026BF74(s32);
extern void func_00262830(s32);
extern s16 func_0024BCE8(u8);
extern void func_0024E9BC(s32, u8, s32);
extern void func_00262C94(P02RenderE64C *, s32, s32);
extern void func_0026241C(P02RenderE64C *, s32, s32);
extern void *func_0020C478(u32);
extern P02RenderE64C *func_00269798(s32), *func_0026a45c(s32);
extern void func_0020BDCC(s16, s32);
extern void func_002623C4(P02RenderE64C *);
extern u8 *p01_context_80220F68;
extern u8 D_800EB290[], D_800EB240[];
extern P02Vec3 p02_data_80220700;
extern float D_80220EE0[16];

/* The complete phase preserves the target-vector and real matrix lifetimes. */
static __inline__ void p02_message_1009(P02StateE64C *state)
{
    P02Vec3 target;
    void *pointer;
    void *matrix;
    P02RenderE64C *object;
    s32 value;

    pointer = func_0020C478(state->value_1C);
    func_0026C7C4(pointer, &target);
    object = func_00269798(0x44);
    matrix = object->value_1C;
    object->flags_0A |= 0x8000;
    func_0025F058(D_80220EE0, matrix);
    func_0025F150(matrix, &target);
    object = func_00269798(0x42);
    matrix = object->value_1C;
    object->flags_0A |= 0x8000;
    func_0026C55C(pointer, matrix);
    func_0025F150(matrix, &target);
    object = func_0026a45c(0x43);
    object->flags_0A |= 0x8000;
    func_0025F058(matrix, object->value_1C);
    func_0020BDCC((s16)state->value_1C, 10);
    value = 0xE6;
    switch (state->value_20) {
    case 22: value = 0xE8; break;
    case 23: value = 0xEA; break;
    }
    func_0024E9BC((s32)D_800EB240, state->value_1B, value);
    func_002623C4(state->value_24);
    func_00262830(0x100A);
}

s32 func_0026e64c(u16 message, P02StateE64C *state)
{
    s32 result = 0;
    P02Vec3 target;
    P02Vec3 position;
    void *pointer;
    void *matrix;
    s32 value;
    s32 i;
    float amount;

    switch (message) {
    case 0x101:
        func_000016C4(state->value_2C);
        func_00262B68();
        break;
    case 0x100:
        func_00023780(state, 0x30);
        state->value_2C = func_00001330(0x60);
        state->value_24 = func_0026BF74(0x45);
        state->value_24->flags_0A |= 0x2000;
        state->value_0C.x = state->value_0C.y = state->value_0C.z = 0.0f;
        state->value_00.y = state->value_00.x = 0.0f;
        state->value_00.z = 1.0f;
        func_00262830(0x1002);
        break;

    case 0x1002:
    case 0x1003:
        if (func_0024BCE8(state->value_1B)) {
            state->value_0C.x = 95.0f;
            state->value_0C.z = -10.0f;
            state->value_00.x = -1.0f;
        } else {
            state->value_0C.x = -95.0f;
            state->value_0C.z = 10.0f;
            state->value_00.x = 1.0f;
        }
        state->value_00.y = state->value_00.z = 0.0f;
        state->value_0C.y = -40.0f;
        matrix = state->value_24;
        matrix = (u8 *)matrix + 0x1C;
        func_0026C230(state, matrix);
        func_0025F150(matrix, &state->value_0C);
        value = 0xE5;
        switch (state->value_20) {
        case 22: value = 0xE7; break;
        case 23: value = 0xE9; break;
        }
        func_0024E9BC((s32)D_800EB290, state->value_1B, value);
        func_00262830(0x1004);
        break;
    case 0x1004:
        state->value_0C.y += 5.0f;
        if (30.0f <= state->value_0C.y) {
            state->value_0C.y = 30.0f;
            func_00262830(0x1005);
        }
        matrix = state->value_24;
        matrix = (u8 *)matrix + 0x1C;
        func_0026C230(state, matrix);
        func_0025F150(matrix, &state->value_0C);
        break;
    case 0x1005:
        state->value_24->flags_0A &= ~0x2000;
        func_00262C94(state->value_24, 11, 0x1006);
        break;
    case 0x1006: {
        /* Separate phase views preserve the pinned compiler's saved registers. */
        P02MotionE64C *motion;
        void *actor;
        func_0026241C(state->value_24, 13, 25);
        actor = func_0020C478(state->value_1C);
        func_0026C7C4(actor, &target);
        target.y += (float)(func_0020D590((struct SupplementBoundsRecord *)actor) / 2);
        motion = state->value_2C;
        motion->value_0C.x = target.x - state->value_0C.x;
        motion->value_0C.y = target.y - state->value_0C.y;
        motion->value_0C.z = target.z - state->value_0C.z;
        motion->value_1C = func_0025EF78(&motion->value_0C);
        func_0025EEF8(&p02_data_80220700, &motion->value_0C, &motion->value_24);
        func_0025EE90(&motion->value_24, -1.0f, &motion->value_24);
        func_0025EFC8(&motion->value_24);
        func_0025EE90(&motion->value_0C, 1.0f / motion->value_1C, &motion->value_0C);
        motion->value_00.x = state->value_0C.x;
        motion->value_00.y = state->value_0C.y;
        motion->value_00.z = state->value_0C.z;
        motion->value_18 = 0.0f;
        motion->value_20 = 8.0f;
        /* Keep the endpoint load in this array-index initialization order.
         * The pinned compiler then retains the retail FP allocation. */
        motion->value_30[0] = 0.0f;
        motion->value_30[1] = motion->value_1C / 3.0f;
        motion->value_30[2] = (motion->value_1C * 2.0f) / 3.0f;
        motion->value_30[3] = motion->value_1C;
        motion->value_40[0] = 0.0f;
        motion->value_40[1] = motion->value_1C / 4.0f;
        motion->value_40[2] = motion->value_1C / 4.0f;
        motion->value_40[3] = 0.0f;
        func_0026C9CC(4, motion->value_30, motion->value_40, motion->value_50);
        motion->value_1C -= 40.0f;
        matrix = state->value_24;
        matrix = (u8 *)matrix + 0x1C;
        func_0026C230(state, matrix);
        func_0025F150(matrix, &state->value_0C);
        state->value_2A = 1;
        func_00262830(0x1007);
        break;
    }
    case 0x1007: {
        P02MotionE64C *motion;
        motion = state->value_2C;
        if (state->value_24->value_10 < 21)
            motion->value_20 = 10.0f;
        else
            motion->value_20 = 7.0f;
        motion->value_18 += motion->value_20;
        if (motion->value_1C <= motion->value_18) {
            motion->value_18 = motion->value_1C;
            func_00262830(0x1008);
        }
        amount = func_0026CBDC(motion->value_18, 4, motion->value_30,
                             motion->value_40, motion->value_50);
        for (i = 0; i < 3; ++i) {
            ((float *)&position)[i] = ((float *)&motion->value_0C)[i] * motion->value_18
                                   + ((float *)&motion->value_00)[i];
            ((float *)&position)[i] += amount * ((float *)&motion->value_24)[i];
        }
        for (i = 0; i < 3; ++i) {
            ((float *)&state->value_00)[i] = ((float *)&position)[i]
                                         - ((float *)&state->value_0C)[i];
            ((float *)&state->value_0C)[i] = ((float *)&position)[i];
        }
        matrix = state->value_24;
        matrix = (u8 *)matrix + 0x1C;
        func_0026C230(state, matrix);
        func_0025F150(matrix, &state->value_0C);
        goto update_counter;
    }
    case 0x1008:
        if (state->value_24->value_10 < 15)
            func_00262830(0x1009);
update_counter:
        if (++state->value_28 >= 4) {
            state->value_28 = 0;
            p01_context_80220F68 = (u8 *)state->value_24->value_10;
            pointer = func_00262AB0((void *)func_0026F12C);
            func_0025F058(state->value_24->value_1C, (u8 *)pointer + 0x24);
        }
        break;
    case 0x1009:
        p02_message_1009(state);
        break;
    case 0x100A:
        break;
    /* Keep the query cases after the update phases for retail block layout. */
    case 0x1000:
        result = state->value_2A;
        break;
    case 0x1001:
        if (*(u16 *)((u8 *)state - 0x1C) == 0x100A)
            result = 1;
        break;
    }
    return result;
}
