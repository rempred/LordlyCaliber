#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02Vec2705EC { float x, y, z; } P02Vec2705EC;
typedef struct P02Motion2705EC {
    P02Vec2705EC value_00, value_0C;
    float value_18, value_1C, value_20;
} P02Motion2705EC;
typedef struct P02Render2705EC {
    u8 unknown_00[0xA];
    u16 flags_0A;
    u32 unknown_0C, value_10;
    u8 unknown_14[8];
    u8 value_1C[0x40];
} P02Render2705EC;
typedef struct P02State2705EC {
    P02Vec2705EC value_00, value_0C;
    u8 unknown_18[3], value_1B;
    u32 value_1C, unknown_20;
    s32 value_24;
    P02Render2705EC *value_28;
    s16 value_2C;
    u16 value_2E, value_30, value_32;
    P02Vec2705EC value_34, value_40;
    P02Motion2705EC value_4C;
    s32 value_70;
    s16 *value_74;
} P02State2705EC;

extern void func_00262B68(void), func_00262830(s32);
extern void func_00023780(void *, s32);
extern P02Render2705EC *func_0026BF74(s32);
extern s16 func_0024BCE8(u8);
extern void func_0024E9BC(s32, u8, s32);
extern void func_002623C4(P02Render2705EC *);
extern void *func_0020C478(u32);
extern P02Render2705EC *func_00269798(s32), *func_0026a45c(s32);
extern void func_0020BDCC(s16, s32);
extern s32 func_002700B0(u16, void *);
extern u8 *p01_context_80220F68;
extern u8 D_800EB290[], D_800EB240[];

static __inline__ void p02_phase_1005(P02State2705EC *state)
{
    P02Vec2705EC target;
    P02Render2705EC *object;
    void *pointer;
    s32 value;

    func_002623C4(state->value_28);
    pointer = func_0020C478(state->value_1C);
    func_0026C7C4(pointer, &target);
    object = func_00269798(0x52);
    object->flags_0A |= 0x8000;
    func_0026C684(pointer, object->value_1C);
    pointer = object->value_1C;
    func_0025F150(pointer, &target);
    object = func_0026a45c(0x53);
    object->flags_0A |= 0x8000;
    func_0026C3F8(object->value_1C);
    pointer = object->value_1C;
    func_0025F150(pointer, &target);
    func_0020BDCC((s16)state->value_1C, 10);
    value = 0xEC;
    switch (state->value_24) {
    case 30: value = 0xEE; break;
    case 31: value = 0xF0; break;
    }
    func_0024E9BC((s32)D_800EB240, state->value_1B, value);
    func_00262830(0x1006);
}

s32 func_002705EC(u16 message, P02State2705EC *state)
{
    s32 result = 0;
    s32 phase;
    s32 value;
    float zero;
    P02Vec2705EC position, difference;
    P02Vec2705EC *direction;
    P02Motion2705EC *motion;
    void *matrix;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        func_00023780(state, 0x78);
        state->value_28 = func_0026BF74(0x54);
        state->value_28->flags_0A |= 0x4000;
        state->value_28->value_10 = 13;
        func_00262830(0x1002);
        break;
    case 0x1002:
        if (func_0024BCE8(state->value_1B)) {
            state->value_0C.x = 266.0f;
            state->value_00.x = -1.0f;
        } else {
            state->value_0C.x = -266.0f;
            state->value_00.x = 1.0f;
        }
        state->value_0C.z = 0.0f;
        state->value_0C.y = 25.0f;
        zero = state->value_0C.z;
        state->value_70 = 0;
        state->value_00.z = zero;
        state->value_00.y = zero;
        func_002702A0(state);
        func_00270488(state);
        /* Reuse the loaded-object pointer for its interior address: this
         * preserves the retail allocation with the pinned compiler. */
        matrix = state->value_28;
        matrix = ((P02Render2705EC *)matrix)->value_1C;
        func_0026C230(state, matrix);
        func_0025F150(matrix, &state->value_0C);
        value = 0xEB;
        switch (state->value_24) {
        case 30: value = 0xED; break;
        case 31: value = 0xEF; break;
        }
        func_0024E9BC((s32)D_800EB290, state->value_1B, value);
        func_00262830(0x1003);
        break;
    case 0x1003:
        phase = 0;
        motion = &state->value_4C;
        if (motion->value_1C <= (motion->value_18 += motion->value_20)) {
            motion->value_18 = motion->value_1C;
            if (state->value_74[state->value_70] == -1) {
                phase = 2;
            } else {
                if (++state->value_70 >= 2)
                    state->value_2E = 1;
                phase = 1;
            }
        }
        direction = &motion->value_0C;
        func_0025EE90(direction, motion->value_18, &position);
        position.x += motion->value_00.x;
        position.y += motion->value_00.y;
        position.z += motion->value_00.z;
        state->value_00.x = position.x - state->value_0C.x;
        state->value_00.y = position.y - state->value_0C.y;
        state->value_00.z = position.z - state->value_0C.z;
        matrix = state->value_28;
        state->value_0C.x = position.x;
        state->value_0C.y = position.y;
        state->value_0C.z = position.z;
        matrix = ((P02Render2705EC *)matrix)->value_1C;
        func_0026C230(state, matrix);
        func_0025F150(matrix, &state->value_0C);
        if (++state->value_2C >= 4) {
            matrix = state->value_28;
            p01_context_80220F68 = (u8 *)((P02Render2705EC *)matrix)->value_10;
            matrix = ((P02Render2705EC *)matrix)->value_1C;
            func_0025F058(matrix, (u8 *)func_00262AB0((void *)func_002700B0) + 0x24);
            state->value_2C = 0;
        }
        if (phase == 1) {
            func_00270488(state);
            state->value_32 = 0;
            state->value_30 = 4;
            func_0025EE90(&state->value_00, 10.0f, &state->value_34);
            func_0025EE90(direction, 10.0f, &state->value_40);
            func_00262830(0x1004);
        } else if (phase == 2) {
            func_00262830(0x1005);
        }
        break;
    case 0x1004:
        if (++state->value_32 >= state->value_30) {
            state->value_32 = state->value_30;
            func_00262830(0x1003);
        }
        difference.x = state->value_40.x - state->value_34.x;
        difference.y = state->value_40.y - state->value_34.y;
        difference.z = state->value_40.z - state->value_34.z;
        func_0025EE90(&difference, (float)state->value_32 / state->value_30, &difference);
        state->value_00.x = state->value_34.x + difference.x;
        state->value_00.y = state->value_34.y + difference.y;
        state->value_00.z = state->value_34.z + difference.z;
        matrix = state->value_28;
        matrix = ((P02Render2705EC *)matrix)->value_1C;
        func_0026C230(state, matrix);
        func_0025F150(matrix, &state->value_0C);
        if (++state->value_2C >= 4) {
            matrix = state->value_28;
            p01_context_80220F68 = (u8 *)((P02Render2705EC *)matrix)->value_10;
            matrix = ((P02Render2705EC *)matrix)->value_1C;
            func_0025F058(matrix, (u8 *)func_00262AB0((void *)func_002700B0) + 0x24);
            state->value_2C = 0;
        }
        break;
    case 0x1005:
        p02_phase_1005(state);
        break;
    case 0x1006:
        /* This explicit no-op case preserves the retail dispatch shape. */
        break;
    case 0x1000:
        if (state->value_2E)
            result = 1;
        break;
    case 0x1001:
        if (*(u16 *)((u8 *)state - 0x1C) == 0x1006)
            result = 1;
        break;
    }
    return result;
}
