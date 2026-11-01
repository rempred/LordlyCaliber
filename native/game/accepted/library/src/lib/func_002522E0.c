#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
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

typedef struct P01TransformState {
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
    u32 unknown_44;
    P01PositionObject *object_48;
} P01TransformState;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_data_80209A50[];
extern u8 p01_data_80209A7C[];
extern const P01Vec3 p01_vector_8020A250;
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_002628E4(s32);
extern void func_00262C64(s16, s16);
extern void func_00262C80(void);


extern void func_0024CBC8(void *, s16, float, s32);





s32 func_002522E0(u16 message, P01TransformState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->context = p01_context_80220F68;
        state->value_04 = *(u8 *)&p01_pointer_80220F6C;
        func_0024C204(state->context, &state->value_08, 0x20);
        func_0024BF04(state->value_04, &state->value_14, 0);
        func_0024CBC8(state, 4, 1.5f, 0x10);
        state->value_34 = state->value_2C / 19.0f;
        func_00262830(0x1000);
        break;
    case 0x1000:
        state->object_48 = func_0024D180(&state->value_08, p01_data_80209A50);
        func_0024E9BC(0, state->context[0], 0x5B);
        func_0025DB00(state->context, (const struct Func0025DAB0Input *)(p01_data_80209A7C));
        func_00262C64(9, 0x1001);
        break;
    case 0x1001: {
        P01PositionObject *object = state->object_48;
        P01Vec3 output;
        P01Vec3 basis = p01_vector_8020A250;

        if (state->value_2C <= (state->value_30 += state->value_34)) {
            state->value_30 = state->value_2C;
            func_00262C80();
            func_00262C64(5, 0x1002);
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
        func_0026CCB4(object->value_1C.x, object->value_1C.y,
                      object->value_1C.z, 0xFF);
        break;
    }
    case 0x1002:
        func_0024CD40(state);
        func_002628E4(0x101);
        break;
    }
    return 0;
}
