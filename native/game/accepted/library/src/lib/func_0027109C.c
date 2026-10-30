#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02Vec27109C { float x, y, z; } P02Vec27109C;
typedef struct P02Render27109C {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
    u8 unknown_10[0xC];
    float value_1C[16];
} P02Render27109C;

typedef struct P02State27109C {
    u8 unknown_00[0x1B];
    u8 value_1B;
    u32 value_1C, unknown_20;
    s32 value_24;
} P02State27109C;

extern void *func_0020C478(u32);
extern P02Render27109C *func_00269798(s32);
extern P02Render27109C *func_0026abc8(s32);
extern void func_0020BDCC(s16, s32);
extern void func_0024E9BC(s32, u8, s32);
extern float D_80220EE0[16];
extern u8 D_800EB240[];

void  func_0027109C(void *stateView)
{
    /* This helper uses the documented offset view of its caller's payload. */
    P02State27109C *state = stateView;
    float matrix[16];
    P02Vec27109C position;
    P02Render27109C *object;
    float *destination;
    s32 value;

    func_0026C834(state->value_1C, &position);
    func_0020C478(state->value_1C);
    func_0025F058(D_80220EE0, matrix);
    object = func_00269798(0x5A);
    destination = object->value_1C;
    object->value_0C = -1000;
    object->flags_0A |= 0x9000;
    func_0025F058(matrix, destination);
    func_0025F150(destination, &position);
    object = func_00269798(0x5B);
    destination = object->value_1C;
    object->flags_0A |= 0x8000;
    func_0025F058(matrix, destination);
    func_0025F150(destination, &position);
    object = func_0026abc8(0x5C);
    object->flags_0A |= 0x8000;
    object->value_1C[0] = position.x;
    object->value_1C[1] = position.y;
    object->value_1C[2] = position.z;
    func_0020BDCC((s16)state->value_1C, 10);
    value = 0xF9;
    switch (state->value_24) {
    case 37: value = 0xFC; break;
    case 38: value = 0xFF; break;
    }
    func_0024E9BC((s32)D_800EB240, state->value_1B, value);
}
