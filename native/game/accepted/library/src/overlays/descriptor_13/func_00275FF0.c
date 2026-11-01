#include "game/effect_presentation_p06_internal.h"

typedef struct P06Record75FF0 {
    u16 value_00, value_02, value_04;
    u8 unknown_06[0x4A];
} P06Record75FF0;
typedef struct P06State75FF0 {
    u8 *value_00;
    u8 value_04, unknown_05;
    s16 value_06;
    u8 unknown_08[0x10];
    u8 value_18, value_19, value_1A, value_1B;
    void *value_1C;
    P06Record75FF0 *value_20;
} P06State75FF0;

extern u8 *p06_pointer_80220F68;
extern u8 p06_data_80220F6C, p06_data_80220F6D;
s32 func_00275FF0(u16 message, P06State75FF0 *state)
{
    float first[3], second[3];

    switch (message) {
    case 0x101:
        if (state->value_20 != 0) {
            resource_free((u8 *)(state->value_20));
        }
        func_00275EB0((struct P06State75EB0 *)(state->value_1C));
        func_00261E30(state->value_1C);
        func_00262B68();
        break;
    case 0x100:
        *(u8 **)state = p06_pointer_80220F68;
        state->value_04 = p06_data_80220F6C;
        state->value_18 = p06_data_80220F6D;
        state->value_20 = 0;
        state->value_1C = 0;
        state->value_1C = func_00261DA8(func_00275EE0);
        func_00275E68((struct P06State75E68 *)(state->value_1C));
        func_00262830(0x1000);
        break;
    case 0x1000:
        if (func_0020C014(func_0020C478(state->value_00[0]))) {
            state->value_19 = 1;
        } else {
            state->value_19 = 0;
        }
        func_0024C204(state->value_00, (struct P01Vec3 *)(first), 0x20);
        if (state->value_04 < 20) {
            func_0024BF04(state->value_04, (struct P01Vec3 *)(second), 0x10);
        } else {
            func_0024BF04(state->value_04, (struct P01Vec3 *)(second), 0);
        }
        first[1] += 20.0f;
        if (state->value_19) {
            second[1] -= 5.0f;
        } else {
            second[1] += 5.0f;
        }
        state->value_1A = 1;
        state->value_06 = 255;
        state->value_20 = resource_alloc(0x9E);
        state->value_20[0].value_00 = 0;
        state->value_20[0].value_02 = 16;
        state->value_20[0].value_04 = 48;
        state->value_20[1].value_00 = 1;
        state->value_20[1].value_02 = 14;
        state->value_20[1].value_04 = 48;
        state->value_1B = func_00279D58((struct P06Vec79A90 *)(first), (struct P06Vec79A90 *)(second), state->value_20);
        func_00275E3C(state->value_1C);
        func_00275D8C(state->value_1C, (s32)state->value_20);
        func_00275DA0(state->value_1C, state->value_06);
        func_00275DC4((struct P06IntegerVector75DC4 *)(state->value_1C), first, second);
        func_00262830(0x1001);
        break;
    case 0x1001:
        func_00275DBC(state->value_1C, state->value_1A);
        if (state->value_1A++ < state->value_1B) {
            func_00262830(0x1001);
        } else {
            func_00262C64(5, 0x1002);
        }
        break;
    case 0x1002:
        func_00275DA0(state->value_1C, state->value_06);
        state->value_06 -= 20;
        if (state->value_06 < 0) {
            func_00275E4C(state->value_1C);
            func_00262C64(1, 0x101);
        }
        break;
    }
    return 0;
}
