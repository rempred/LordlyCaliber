#include "game/effect_math_e04.h"
#include "common/types.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

typedef struct P01PositionObject {
    u8 unknown_00[0x1C];
    P01Vec3 value_1C;
} P01PositionObject;

typedef struct P01TrailState {
    u8 *context;
    u8 value_04;
    u8 unknown_05[3];
    P01Vec3 value_08;
    P01Vec3 value_14;
    P01Vec3 value_20;
    float value_2C;
    float value_30;
    float value_34;
    float *value_38;
    float *value_3C;
    float *value_40;
    s16 counter_44;
    u16 unknown_46;
    P01PositionObject *object_48;
} P01TrailState;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_data_80209B02[];
extern u8 p01_data_80209B08[];
extern u8 p01_data_80209B68[];
extern u8 p01_data_80209AFC[];
extern u8 p01_data_80209B0E[];
extern const P01Vec3 p01_vector_8020A25C;
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_002628E4(s32);
extern void func_00262C64(s16, s16);
extern void func_00262C80(void);
extern void func_0024C204(u8 *, P01Vec3 *, s32);
extern void func_0024BF04(u8, P01Vec3 *, s32);
extern void func_0024CBC8(void *, s16, float, s32);
extern P01PositionObject *func_0024D180(P01Vec3 *, void *);
extern void func_0024E9BC(s32, u8, s32);
extern void func_0025DB00(u8 *, void *);
extern void func_0024CD40(P01TrailState *);
extern void *func_0024DA10(u32, void *);
extern void func_0025DAB0(u8, void *);
extern void func_0024E490(u32, const u8 *, u32);
extern void func_0025CAF0(u8, s32, s32);

s32 func_002532A8(u16 message, P01TrailState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Keep the first context store before the following shared byte load. */
        *(u8 **)state = p01_context_80220F68;
        state->value_04 = *(u8 *)&p01_pointer_80220F6C;
        func_0024BF04(0x21, &state->value_08, 0);
        func_0024BF04(state->value_04, &state->value_14, 0);
        func_0024CBC8(state, 4, 2.5f, 0x10);
        state->value_34 = state->value_2C / 29.0f;
        state->counter_44 = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0024DA10(0x21, p01_data_80209AFC);
        func_0024E9BC(0, state->context[0], 0x6F);
        func_0025DAB0(0x21, p01_data_80209B68);
        func_00262C64(0x1E, 0x1001);
        break;
    case 0x1001:
        state->object_48 = func_0024D180(&state->value_08, p01_data_80209B02);
        func_00262830(0x1002);
        /* The retail launch continues into its first update immediately. */
    case 0x1002: {
        P01PositionObject *object = state->object_48;
        P01Vec3 *position = &object->value_1C;
        P01Vec3 output;
        P01Vec3 basis = p01_vector_8020A25C;

        if (state->counter_44 == 29) {
            func_00262830(0x1003);
        } else {
            if (state->value_2C <= (state->value_30 += state->value_34)) {
                state->value_30 = state->value_2C;
            }
            func_0025EE90(&basis,
                func_0026CBDC(state->value_30, 4,
                              state->value_38, state->value_3C, state->value_40),
                &output);
            object->value_1C.x = output.x + state->value_08.x
                + state->value_30 * state->value_20.x;
            object->value_1C.y = output.y + state->value_08.y
                + state->value_30 * state->value_20.y;
            object->value_1C.z = output.z + state->value_08.z
                + state->value_30 * state->value_20.z;
            if ((s16)(state->counter_44 % 3) == 0) {
                func_0024D180(position, p01_data_80209B08);
            }
            func_0026CCB4(object->value_1C.x, object->value_1C.y,
                          object->value_1C.z, 0xFF);
        }
        state->counter_44++;
        break;
    }
    case 0x1003:
        func_0024CD40(state);
        func_0024E490(state->value_04, p01_data_80209B0E, 2);
        func_0024E9BC(0, state->context[0], 0x70);
        func_0025CAF0(state->value_04, 0xA, 0xA);
        func_00262C64(0xE, 0x1004);
        break;
    case 0x1004:
        func_00262C80();
        func_002628E4(0x101);
        break;
    }
    return 0;
}
