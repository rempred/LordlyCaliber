#include "game/effect_presentation_p06_internal.h"

typedef struct P06Object56AC8 {
    u8 unknown_00[0x10];
    s32 value_10;
    u8 unknown_14[0x38];
    P06Vec3 value_4C;
} P06Object56AC8;
typedef struct P06Payload56AC8 {
    u8 unknown_00[0x30];
    P06Vec3 value_30;
} P06Payload56AC8;
typedef struct P06State56AC8 {
    u8 value_00, value_01;
    u16 unknown_02;
    P06Vec3 value_04, value_10, value_1C;
    float value_28;
    s16 value_2C, unknown_2E, value_30, unknown_32;
    P06Object56AC8 *value_34, *value_38;
} P06State56AC8;

extern u8 p06_data_80220F68, p06_data_80220F69;
extern u16 p06_data_80220F6A;
extern float p06_data_80209FB8[][2];
extern u8 p06_data_80209E24[], p06_data_80209E2A[], p06_data_80209E36[];
extern float func_001FF7EC(s16, s16);
s32 func_00256AC8(u16 message, P06State56AC8 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100: {
        P06Vec3 *first;
        P06Vec3 *second;
        P06Vec3 *direction;

        state->value_00 = p06_data_80220F68;
        state->value_01 = p06_data_80220F69;
        state->value_30 = p06_data_80220F6A;
        first = &state->value_04;
        func_0024C0DC(state->value_00, (struct P01Vec3 *)(first), 0.0f,
            p06_data_80209FB8[state->value_30][0], p06_data_80209FB8[state->value_30][1]);
        second = &state->value_10;
        func_0024C0DC(state->value_01, (struct P01Vec3 *)(second), 0.0f,
            p06_data_80209FB8[state->value_30][0], p06_data_80209FB8[state->value_30][1]);
        direction = &state->value_1C;
        func_0024C7A4(direction, second, first);
        state->value_28 = func_0025EF78(direction);
        func_0025EFC8(direction);
        state->value_2C = 30;
        func_00262830(0x1000);
        break;
    }
    case 0x1000: {
        u8 *records = p06_data_80209E2A;
        P06Payload56AC8 *first = func_0024CDD8(records);
        P06Payload56AC8 *second = func_0024CDD8(records + 6);
        P06Object56AC8 *firstObject;
        P06Object56AC8 *secondObject;

        func_0024C7D8((struct P01Vec3 *)(&state->value_1C), (struct P01PlacementObject *)(first));
        first->value_30.x = state->value_04.x + state->value_2C * state->value_1C.x;
        first->value_30.y = state->value_04.y + state->value_2C * state->value_1C.y;
        first->value_30.z = state->value_04.z + state->value_2C * state->value_1C.z;
        func_0025F058(first, second);
        second->value_30.y = func_001FF7EC((s16)second->value_30.x, (s16)second->value_30.z);
        state->value_28 -= state->value_2C;
        firstObject = (P06Object56AC8 *)((u8 *)first - 0x1C);
        state->value_34 = firstObject;
        func_0026241C(firstObject, 5, 11);
        secondObject = (P06Object56AC8 *)((u8 *)second - 0x1C);
        state->value_38 = secondObject;
        func_0026241C(secondObject, 5, 11);
        func_00262830(0x1001);
        break;
    }
    case 0x1001: {
        P06Object56AC8 *first = state->value_34;
        P06Object56AC8 *second = state->value_38;
        P06Payload56AC8 *firstPayload;

        first->value_4C.x += state->value_2C * state->value_1C.x;
        first->value_4C.y += state->value_2C * state->value_1C.y;
        first->value_4C.z += state->value_2C * state->value_1C.z;
        firstPayload = (P06Payload56AC8 *)((u8 *)first + 0x1C);
        func_0025F058(firstPayload, (u8 *)second + 0x1C);
        second->value_4C.y = func_001FF7EC((s16)second->value_4C.x, (s16)second->value_4C.z);
        state->value_28 -= state->value_2C;
        if (state->value_30 == 0 && state->value_28 >= 38.0f && state->value_28 < state->value_2C + 38) {
            func_0024DA10(state->value_01, p06_data_80209E24);
        }
        if (state->value_28 < 38.0f) {
            func_00262434(state->value_34);
            state->value_34->value_10 = 12;
            func_00262434(state->value_38);
            state->value_38->value_10 = 12;
            if (state->value_30 == 2) {
                func_00262C80();
            } else if (state->value_30 == 0) {
                func_0025CAF0(state->value_01, 3, 5);
            }
            func_00262830(0x1002);
        } else {
            func_0025F058(firstPayload, func_0024CDD8(p06_data_80209E36));
        }
        break;
    }
    case 0x1002:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
