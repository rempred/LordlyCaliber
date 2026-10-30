#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02Vec270488 { float x, y, z; } P02Vec270488;
typedef struct P02Pair270488 { s16 value_00, value_02; } P02Pair270488;
typedef struct P02Motion270488 {
    P02Vec270488 value_00, value_0C;
    float value_18, value_1C, value_20;
} P02Motion270488;
typedef struct P02State270488 {
    u8 unknown_00[0xC];
    P02Vec270488 value_0C;
    u32 unknown_18;
    void *value_1C;
    u32 value_20;
    u8 unknown_24[0x28];
    P02Motion270488 value_4C;
    s32 value_70;
    s16 *value_74;
} P02State270488;

extern P02Pair270488 p02_pair_802209E0[];

void  func_00270488(void *stateView)
{
    /* This helper uses the documented offset view of its caller's payload. */
    P02State270488 *state = stateView;
    P02Vec270488 position;
    P02Motion270488 *motion;
    P02Vec270488 *delta;
    s16 selected;

    state->value_4C.value_00.x = state->value_0C.x;
    state->value_4C.value_00.y = state->value_0C.y;
    state->value_4C.value_00.z = state->value_0C.z;
    selected = state->value_74[state->value_70];
    motion = &state->value_4C;
    if (selected != -1) {
        position.x = p02_pair_802209E0[selected].value_00;
        position.y = 0.0f;
        position.z = p02_pair_802209E0[selected].value_02;
    } else {
        func_0026C834((u32)state->value_1C, &position);
    }
    motion->value_0C.x = position.x - state->value_0C.x;
    motion->value_0C.y = position.y - state->value_0C.y;
    motion->value_0C.z = position.z - state->value_0C.z;
    delta = &motion->value_0C;
    motion->value_1C = func_0025EF78(delta);
    func_0025EE90(delta, 1.0f / motion->value_1C, delta);
    motion->value_18 = 0.0f;
    motion->value_20 = motion->value_1C * 0.07692308f;
    if (selected == -1)
        motion->value_1C -= 40.0f;
}
