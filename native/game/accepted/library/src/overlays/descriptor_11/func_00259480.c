#include "game/effect_presentation_p06_internal.h"

typedef struct P06Object59480 {
    u8 unknown_00[0x10];
    s32 value_10;
    u8 unknown_14[0x38];
    P06Vec3 value_4C;
} P06Object59480;
typedef struct P06State59480 {
    u8 *value_00;
    u8 value_04, unknown_05[3];
    P06Vec3 value_08, value_14, value_20;
    float value_2C, value_30, value_34;
    float *value_38, *value_3C, *value_40;
    s16 value_44, unknown_46;
    P06Object59480 *value_48, *value_4C;
} P06State59480;

extern u8 *p06_pointer_80220F68;
extern u8 p06_data_80220F6C;
extern u8 p06_data_80209F3E[];
extern const P06Vec3 p06_data_8020A288;
extern const double p06_double_8020A298;
extern float func_001FF7EC(s16, s16);
s32 func_00259480(u16 message, P06State59480 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100: {
        u8 value;
        P06Vec3 *first;
        P06Vec3 *second;
        P06Vec3 *direction;

        state->value_00 = p06_pointer_80220F68;
        state->value_04 = p06_data_80220F6C;
        value = func_0024C3EC(state->value_00[0]);
        if (func_0024BCE8(state->value_00[0]) != 0) {
            value -= 3;
        } else {
            value += 3;
        }
        first = &state->value_08;
        func_0024BF04(value, (struct P01Vec3 *)(first), 0x10);
        second = &state->value_14;
        func_0024C0DC(state->value_04, (struct P01Vec3 *)(second), 0.0f, 75.26275634765625f, 0.0f);
        direction = &state->value_20;
        func_0024C7A4(direction, second, first);
        state->value_2C = func_0025EF78(direction);
        func_0025EFC8(direction);
        state->value_34 = state->value_2C / 14.0f;
        state->value_38 = resource_alloc(16);
        state->value_3C = resource_alloc(16);
        state->value_40 = resource_alloc(16);
        state->value_30 = 0.0f;
        state->value_38[0] = 0.0f;
        state->value_38[1] = state->value_2C / 3.0f;
        state->value_38[2] = (state->value_2C + state->value_2C) / 3.0f;
        state->value_38[3] = state->value_2C;
        state->value_3C[0] = 0.0f;
        state->value_3C[1] = state->value_2C * p06_double_8020A298;
        state->value_3C[2] = state->value_3C[1];
        state->value_3C[3] = 0.0f;
        func_0026C9CC(4, state->value_38, state->value_3C, state->value_40);
        state->value_44 = 0;
        func_00262830(0x1000);
        break;
    }
    case 0x1000: {
        u8 *records = p06_data_80209F3E;
        /* Each result has successive payload and header pointer views. */
        union { u8 *payload; P06Object59480 *header; } first;
        u8 *initial = func_0024CE74((struct Ce74InputRecord *)(records));
        union { u8 *payload; P06Object59480 *header; } second;
        first.payload = initial;
        second.payload = func_0024CDD8(records + 0x1E);

        /* Convert the returned payload views back to their physical headers. */
        first.header = (P06Object59480 *)(first.payload - 0x1C);
        state->value_48 = first.header;
        func_0026241C(first.header, 2, 11);
        second.header = (P06Object59480 *)(second.payload - 0x1C);
        state->value_4C = second.header;
        func_0026241C(second.header, 2, 11);
        func_00262830(0x1001);
        break;
    }
    case 0x1001: {
        P06Object59480 *first = state->value_48;
        P06Object59480 *second = state->value_4C;
        P06Vec3 position;
        P06Vec3 output;
        P06Vec3 basis = p06_data_8020A288;
        void *firstPayload;
        void *secondPayload;

        func_0025EEF8(&basis, &state->value_20, &basis);
        func_0025EE90(&basis, -1.0f, &basis);
        firstPayload = (u8 *)first + 0x1C;
        secondPayload = (u8 *)second + 0x1C;
        if ((state->value_30 += state->value_34) >= state->value_2C) {
            state->value_30 = state->value_2C;
        }
        func_0025EE90(&basis,
            func_0026CBDC(state->value_30, 4, state->value_38, state->value_3C, state->value_40),
            &output);
        first->value_4C.x = output.x + state->value_08.x + state->value_30 * state->value_20.x;
        first->value_4C.y = output.y + state->value_08.y + state->value_30 * state->value_20.y;
        first->value_4C.z = output.z + state->value_08.z + state->value_30 * state->value_20.z;
        func_0025F058(firstPayload, secondPayload);
        second->value_4C.y = func_001FF7EC((s16)second->value_4C.x, (s16)second->value_4C.z);
        position.x = first->value_4C.x;
        position.y = first->value_4C.y - 5.0f;
        position.z = first->value_4C.z;
        func_0024D180(&position, p06_data_80209F3E + 6);
        if (state->value_44++ == 14) {
            func_00262434(state->value_48);
            state->value_48->value_10 = 9;
            func_00262434(state->value_4C);
            state->value_4C->value_10 = 2;
            func_00262C80();
            func_00262830(0x1002);
        }
        break;
    }
    case 0x1002:
        func_0024CD40(state);
        func_002628E4(0x101);
        break;
    }
    return 0;
}
