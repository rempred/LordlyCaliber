#include "game/effect_presentation_p06_internal.h"

typedef struct P06Object56410 {
    u8 unknown_00[0x10];
    s32 value_10;
    u8 unknown_14[0x38];
    P06Vec3 value_4C;
} P06Object56410;
typedef struct P06Payload56410 {
    u8 unknown_00[0x30];
    P06Vec3 value_30;
} P06Payload56410;
typedef struct P06State56410 {
    u8 value_00, value_01;
    u16 unknown_02;
    P06Vec3 value_04;
    float value_10[3];
    P06Vec3 value_1C;
    float value_28;
    s16 value_2C, value_2E;
    u16 value_30, unknown_32;
    P06Object56410 *value_34;
} P06State56410;

extern u8 p06_data_80220F68, p06_data_80220F69;
extern u16 p06_data_80220F6A;
extern u8 p06_data_80209E00[], p06_data_80209E06[], p06_data_80209E0C[];
extern u8 p06_data_80209E12[], p06_data_80209E18[];
s32 func_00256410(u16 message, P06State56410 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100: {
        P06Vec3 *first;
        P06Vec3 *second;
        P06Vec3 *direction;
        float value;
        s32 selected;

        first = &state->value_04;
        state->value_00 = p06_data_80220F68;
        state->value_01 = p06_data_80220F69;
        state->value_30 = p06_data_80220F6A;
        func_0024BF04(state->value_00, (struct P01Vec3 *)(first), 0);
        second = (P06Vec3 *)state->value_10;
        func_0024BF04(state->value_01, (struct P01Vec3 *)(second), 0);
        direction = &state->value_1C;
        func_0024C7A4(direction, second, first);
        state->value_28 = func_0025EF78(direction);
        if (state->value_30 & 2) {
            state->value_2C = 13;
            value = state->value_1C.x / state->value_2C;
            /* Convert to a word before narrowing at the halfword store. */
            selected = (value > 0.0f) ? value - 6.0f : -value - 6.0f;
            state->value_2E = selected;
        } else {
            state->value_2C = 18;
            state->value_2E = 4;
        }
        func_0025EFC8(&state->value_1C);
        func_00262830(0x1000);
        break;
    }
    case 0x1000: {
        P06Payload56410 *payload;
        P06Object56410 *object;

        payload = func_0024CDD8(p06_data_80209E00);
        func_0024C7D8((struct P01Vec3 *)(&state->value_1C), (struct P01PlacementObject *)(payload));
        payload->value_30.x = state->value_04.x + (state->value_2C * state->value_2E) * state->value_1C.x;
        payload->value_30.y = state->value_04.y + (state->value_2C * state->value_2E) * state->value_1C.y;
        payload->value_30.z = state->value_04.z + (state->value_2C * state->value_2E) * state->value_1C.z;
        state->value_28 -= state->value_2C * state->value_2E;
        object = (P06Object56410 *)((u8 *)payload - 0x1C);
        state->value_34 = object;
        func_0026241C(object, 4, 6);
        func_00262830(0x1001);
        break;
    }
    case 0x1001: {
        P06Object56410 *object = state->value_34;
        P06Payload56410 *previous;

        object->value_4C.x += state->value_2C * state->value_1C.x;
        object->value_4C.y += state->value_2C * state->value_1C.y;
        object->value_4C.z += state->value_2C * state->value_1C.z;
        previous = (P06Payload56410 *)((u8 *)object + 0x1C);
        state->value_28 -= state->value_2C;
        if (state->value_28 >= state->value_2C && state->value_28 < state->value_2C * 2) {
            func_0024DA10(state->value_01, p06_data_80209E18);
        }
        if (state->value_28 < state->value_2C) {
            func_00262434(state->value_34);
            state->value_34->value_10 = 6;
            func_00262C80();
            func_00262830(0x1002);
        } else {
            state->value_2E++;
            if (state->value_2E & 1) {
                P06Payload56410 *payload;

                payload = func_0024CDD8(p06_data_80209E06);
                func_0025F058(previous, payload);
                payload->value_30.x -= state->value_2C * state->value_1C.x;
                payload->value_30.y -= state->value_2C * state->value_1C.y;
                payload->value_30.z -= state->value_2C * state->value_1C.z;
            }
        }
        break;
    }
    case 0x1002: {
        P06Payload56410 *payload;

        payload = func_0024CDD8(p06_data_80209E0C);
        func_0024C7D8((struct P01Vec3 *)(&state->value_1C), (struct P01PlacementObject *)(payload));
        func_0025F150(payload, state->value_10);
        func_0025CAF0(state->value_01, 3, 0);
        func_00262C64(3, 0x1003);
        break;
    }
    case 0x1003: {
        s32 step = state->value_2C;
        /* Separate sign arms retain the observed store and call-delay order. */
        if (state->value_10[0] < 0.0f) {
            step = -step;
            state->value_10[0] += step;
        } else {
            state->value_10[0] += step;
        }
        func_0024D180(state->value_10, p06_data_80209E00 + 0x12);
        func_00262C64(0x1C, 0x1004);
        break;
    }
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
