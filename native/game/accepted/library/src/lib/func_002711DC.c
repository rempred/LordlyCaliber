#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02Vec2711DC { float x, y, z; } P02Vec2711DC;
typedef struct P02Motion2711DC {
    P02Vec2711DC value_00, value_0C;
    float value_18, value_1C, value_20;
    P02Vec2711DC value_24;
    float value_30[4], value_40[4], value_50[4];
} P02Motion2711DC;

typedef struct P02Render2711DC {
    u8 unknown_00[0xA];
    u16 flags_0A;
    u32 unknown_0C, value_10;
    u8 unknown_14[8];
    float value_1C[16];
} P02Render2711DC;

typedef struct P02State2711DC {
    P02Vec2711DC value_00, value_0C;
    u8 unknown_18[3], value_1B;
    u32 value_1C;
    s32 value_20, value_24;
    P02Render2711DC *value_28, *value_2C;
    s16 value_30;
    u16 value_32;
    P02Motion2711DC *value_34;
} P02State2711DC;

typedef struct P02State27109C P02State27109C;
typedef struct P02State271D18 P02State271D18;
extern void func_000016C4(void *), func_00262B68(void);
extern void func_00023780(void *, s32);
extern void *func_00001330(u32);
extern P02Render2711DC *func_0026BF74(s32), *func_0026abc8(s32);
extern void func_0026241C(P02Render2711DC *, s32, s32);
extern void func_00262830(s32);
extern s16 func_0024BCE8(u8);
extern void func_0024E9BC(s32, u8, s32);
extern void func_002623C4(P02Render2711DC *);
extern u8 *p01_context_80220F68;
extern u8 D_800EB290[];
extern P02Vec2711DC p02_data_80220A90;

s32 func_002711DC(u16 message, P02State2711DC *state)
{
    s32 result = 0;
    P02Vec2711DC target, destination, position, lateral;
    void *matrix;
    s32 value;

    switch (message) {
    case 0x101:
        func_000016C4(state->value_34);
        func_00262B68();
        break;
    case 0x100:
        func_00023780(state, 0x38);
        state->value_34 = func_00001330(0x60);
        state->value_28 = func_0026BF74(0x5D);
        state->value_2C = func_0026abc8(0x59);
        func_0026241C(state->value_2C, 17, 0x38);
        func_00262830(0x1002);
        break;
    case 0x1002: {
        P02Motion2711DC *motion;
        P02Vec2711DC *delta;
        float zero;
        if (func_0024BCE8(state->value_1B)) {
            state->value_0C.x = 209.0f;
            state->value_00.x = -1.0f;
            target.x = 38.0f;
        } else {
            state->value_0C.x = -209.0f;
            state->value_00.x = 1.0f;
            target.x = -38.0f;
        }
        state->value_0C.z = 0.0f;
        state->value_0C.y = 40.0f;
        /* Keep this loaded value live through initialization; replacing later
         * uses with literals removes the retail saved-FP lifetime. */
        zero = state->value_0C.z;
        state->value_00.y = state->value_00.z = zero;
        target.y = 50.0f;
        target.z = zero;
        motion = state->value_34;
        motion->value_00.x = state->value_0C.x;
        motion->value_00.y = state->value_0C.y;
        motion->value_00.z = state->value_0C.z;
        motion->value_0C.x = target.x - state->value_0C.x;
        motion->value_0C.y = target.y - state->value_0C.y;
        motion->value_0C.z = target.z - state->value_0C.z;
        delta = &motion->value_0C;
        motion->value_1C = func_0025EF78(delta);
        func_0025EE90(delta, 1.0f / motion->value_1C, delta);
        motion->value_18 = zero;
        motion->value_20 = 20.0f;
        motion->value_30[0] = zero;
        motion->value_30[1] = motion->value_1C / 4.0f;
        motion->value_30[2] = motion->value_1C / 2.0f;
        motion->value_30[3] = motion->value_1C;
        motion->value_40[0] = zero;
        motion->value_40[1] = 19.0f;
        motion->value_40[2] = 45.600002f; /* Retail bits: 0x42366667. */
        motion->value_40[3] = 30.4f;
        func_0026C9CC(4, motion->value_30, motion->value_40, motion->value_50);
        matrix = state->value_28;
        matrix = (u8 *)matrix + 0x1C;
        func_0026C230(state, matrix);
        func_0025F150(matrix, &state->value_0C);
        {
            P02Render2711DC *other = state->value_2C;
            if (other) {
                other->value_1C[0] = state->value_0C.x;
                other->value_1C[1] = state->value_0C.y;
                other->value_1C[2] = state->value_0C.z;
            }
        }
        value = 0xF8;
        switch (state->value_24) {
        case 37: value = 0xFB; break;
        case 38: value = 0xFE; break;
        }
        func_0024E9BC((s32)D_800EB290, state->value_1B, value);
        func_00262830(0x1003);
        break;
    }
    case 0x1003: {
        P02Motion2711DC *motion = state->value_34;
        float amount;
        P02Render2711DC *render;
        motion->value_18 += motion->value_20;
        if (motion->value_1C <= motion->value_18) {
            motion->value_18 = motion->value_1C;
            func_00262830(0x1004);
        }
        if (12.0f <= motion->value_20)
            motion->value_20 -= 2.0f;
        amount = func_0026CBDC(motion->value_18, 4, motion->value_30,
                             motion->value_40, motion->value_50);
        func_0025EE90(&motion->value_0C, motion->value_18, &target);
        target.x += motion->value_00.x;
        target.y += motion->value_00.y + amount;
        target.z += motion->value_00.z;
        state->value_0C.x = target.x;
        state->value_0C.y = target.y;
        state->value_0C.z = target.z;
        render = state->value_28;
        func_0026C230(state, render->value_1C);
        func_0025F150(render->value_1C, &state->value_0C);
        {
            P02Render2711DC *other = state->value_2C;
            if (other) {
                other->value_1C[0] = state->value_0C.x;
                other->value_1C[1] = state->value_0C.y;
                other->value_1C[2] = state->value_0C.z;
            }
        }
        break;
    }
    case 0x1004: {
        P02Motion2711DC *motion = state->value_34;
        P02Vec2711DC *delta, *normal;
        func_0026C834(state->value_1C, &destination);
        motion->value_00.x = state->value_0C.x;
        motion->value_00.y = state->value_0C.y;
        motion->value_00.z = state->value_0C.z;
        motion->value_0C.x = destination.x - state->value_0C.x;
        motion->value_0C.y = destination.y - state->value_0C.y;
        motion->value_0C.z = destination.z - state->value_0C.z;
        delta = &motion->value_0C;
        normal = &motion->value_24;
        func_0025EEF8(&p02_data_80220A90, delta, normal);
        func_0025EFC8(normal);
        motion->value_1C = func_0025EF78(delta);
        func_0025EE90(delta, 1.0f / motion->value_1C, delta);
        motion->value_1C -= 30.0f;
        motion->value_18 = 0.0f;
        motion->value_20 = (motion->value_1C * 4.0f) / 180.0f;
        motion->value_30[0] = 0.0f;
        motion->value_30[3] = motion->value_1C;
        /* Independent literal stores avoid a floating zero temporary with
         * the pinned compiler, unlike a chained floating assignment. */
        motion->value_40[0] = 0.0f;
        motion->value_40[3] = 0.0f;
        if (state->value_20 & 1) {
            motion->value_30[1] = motion->value_1C / 4.0f;
            motion->value_30[2] = motion->value_1C / 2.0f;
            motion->value_40[1] = -20.0f;
            motion->value_40[2] = 10.0f;
        } else {
            motion->value_30[1] = motion->value_1C / 3.0f;
            motion->value_30[2] = (motion->value_1C * 2.0f) / 3.0f;
            motion->value_40[1] = 15.0f;
            motion->value_40[2] = -15.0f;
        }
        func_0026C9CC(4, motion->value_30, motion->value_40, motion->value_50);
        func_00262830(0x1005);
        /* The setup continues into its first update in the original body. */
    }
    case 0x1005: {
        P02Motion2711DC *motion = state->value_34;
        float amount;
        P02Render2711DC *render;
        motion->value_18 += motion->value_20;
        if (motion->value_1C <= motion->value_18) {
            motion->value_18 = motion->value_1C;
            func_00262830(0x1006);
        }
        amount = func_0026CBDC(motion->value_18, 4, motion->value_30,
                             motion->value_40, motion->value_50);
        func_0025EE90(&motion->value_0C, motion->value_18, &position);
        func_0025EE90(&motion->value_24, amount, &lateral);
        position.x += motion->value_00.x + lateral.x;
        position.y += motion->value_00.y + lateral.y;
        position.z += motion->value_00.z + lateral.z;
        state->value_0C.x = position.x;
        state->value_0C.y = position.y;
        state->value_0C.z = position.z;
        render = state->value_28;
        func_0026C230(state, render->value_1C);
        func_0025F150(render->value_1C, &state->value_0C);
        {
            P02Render2711DC *other = state->value_2C;
            if (other) {
                other->value_1C[0] = state->value_0C.x;
                other->value_1C[1] = state->value_0C.y;
                other->value_1C[2] = state->value_0C.z;
            }
        }
        if (++state->value_30 >= 6) {
            state->value_30 = 0;
            p01_context_80220F68 = (u8 *)state->value_28->value_10;
            matrix = func_00262AB0((void *)func_00271D18);
            func_0025F058(state->value_28->value_1C, (u8 *)matrix + 0x24);
        }
        if (motion->value_1C / 2.0f <= motion->value_18)
            state->value_32 = 1;
        break;
    }
    case 0x1006:
        func_002623C4(state->value_28);
        if (state->value_2C)
            func_002623C4(state->value_2C);
        func_0027109C((P02State27109C *)state);
        func_00262830(0x1007);
        break;
    case 0x1000:
        if (state->value_32)
            result = 1;
        break;
    case 0x1001:
        if (*(u16 *)((u8 *)state - 0x1C) == 0x1007)
            result = 1;
        break;
    case 0x1007:
        break;
    }
    return result;
}
