#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02Vec3 {
    float x, y, z;
} P02Vec3;

typedef struct P02MotionState {
    P02Vec3 value_00;
    P02Vec3 value_0C;
    float value_18;
    float value_1C;
    float value_20;
    u8 unknown_24[0xC];
    float value_30[4];
    float value_40[4];
    float value_50[4];
} P02MotionState;

typedef struct P02Object {
    u8 unknown_00[0xC];
    P02Vec3 value_0C;
    u8 unknown_18[0x10];
    void *value_28;
    u8 unknown_2C[0x10];
    P02MotionState *value_3C;
} P02Object;


void  func_0026cd1c(void *stateView)
{
    /* This helper uses the documented offset view of its caller's payload. */
    P02Object *object = stateView;
    P02MotionState *state = object->value_3C;
    P02Vec3 position;

    state->value_00.x = object->value_0C.x;
    state->value_00.y = object->value_0C.y;
    state->value_00.z = object->value_0C.z;
    func_0026C834((u32)object->value_28, &position);
    state->value_0C.x = position.x - object->value_0C.x;
    state->value_0C.y = position.y - object->value_0C.y;
    state->value_0C.z = position.z - object->value_0C.z;
    state->value_1C = func_0025EF78(&state->value_0C);
    state->value_18 = 0.0f;
    state->value_20 = 10.0f;
    func_0025EE90(&state->value_0C, 1.0f / state->value_1C, &state->value_0C);
    state->value_30[0] = 0.0f;
    state->value_40[0] = 0.0f;
    state->value_40[3] = 0.0f;
    state->value_30[3] = state->value_1C;
    state->value_30[1] = state->value_1C / 6.0f;
    state->value_30[2] = state->value_1C / 2.0f;
    state->value_40[2] = state->value_1C / 6.0f;
    state->value_40[1] = -state->value_1C / 20.0f;
    func_0026C9CC(4, state->value_30, state->value_40, state->value_50);
    state->value_1C -= 40.0f;
}
