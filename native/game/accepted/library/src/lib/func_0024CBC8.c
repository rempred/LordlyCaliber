#include "game/effect_math_e04.h"
#include "common/types.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

typedef struct P01CurveState {
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
} P01CurveState;

extern void *func_00001330(u32);

void func_0024CBC8(void *stateMemory, s16 count, float scale, s32 flags)
{
    P01CurveState *state = stateMemory;
    P01Vec3 *direction = &state->value_20;
    s32 bytes;

    state->value_20.x = state->value_14.x - state->value_08.x;
    state->value_20.y = state->value_14.y - state->value_08.y;
    state->value_20.z = state->value_14.z - state->value_08.z;
    state->value_2C = func_0025EF78(direction);
    func_0025EFC8(direction);
    bytes = (s16)count * 4;
    state->value_38 = func_00001330(bytes);
    state->value_3C = func_00001330(bytes);
    state->value_40 = func_00001330(bytes);
    state->value_30 = 0.0f;
    state->value_38[0] = 0.0f;
    state->value_38[1] = state->value_2C / 3.0f;
    state->value_38[2] = (state->value_2C + state->value_2C) / 3.0f;
    state->value_38[3] = state->value_2C;
    state->value_3C[0] = 0.0f;
    state->value_3C[1] = state->value_2C * scale * 0.25f;
    state->value_3C[2] = state->value_3C[1];
    if (flags & 0x10) {
        state->value_3C[3] = 20.0f;
    } else {
        state->value_3C[3] = 0.0f;
    }
    func_0026C9CC((s16)count, state->value_38, state->value_3C, state->value_40);
}
