#include "game/effect_presentation_p04.h"
#include "game/effect_handlers_p07.h"
#include "game/effect_math_e04.h"
#include "game/effect_dispatch_e07.h"

typedef struct P07Vec3 { float x, y, z; } P07Vec3;
typedef struct P07Render73180 {
    u8 unknown_00[0xA];
    u16 flags_0A;
    u32 value_0C, value_10;
    u8 unknown_14[8];
    float value_1C[16];
} P07Render73180;
typedef struct P07Pair73180 { s32 value_00, value_04; } P07Pair73180;
typedef struct P07Initial73180 {
    u32 value_00, value_04;
    P07Vec3 value_08;
    P07Pair73180 *value_14;
    s32 value_18;
} P07Initial73180;
typedef struct P07State73180 {
    P07Initial73180 value_00;
    P07Render73180 *value_1C, *value_20;
    s32 value_24;
    float value_28, value_2C, value_30;
    P07Vec3 value_34, value_40;
} P07State73180;
typedef struct P07Context73180 {
    u8 unknown_00[0x14];
    void *value_14;
} P07Context73180;
extern P07Initial73180 p07_initial_80220F68;
extern P07Context73180 *p07_context_80220FE8;

extern s32 func_0026285C(void *, u16);
extern void func_00262B68(void);
extern void func_0026241C(void *, s32, s32);
extern void func_00262434(void *);
extern s32 func_002623E0(void *);
extern void func_002623C4(void *);

s32 func_00273180(u16 message, P07State73180 *state)
{
    switch (message) {
    case 0x101:
        func_0026285C(p07_context_80220FE8->value_14, 0x1004);
        func_00262B68();
        break;
    case 0x100:
        state->value_00 = p07_initial_80220F68;
        state->value_24 = 0;
        func_00262830(0x1000);
        break;
    case 0x1000: {
        P07Vec3 first, second;
        P07Vec3 *direction;
        float value;
        func_0026C834(state->value_00.value_00, &first);
        func_0026C834(state->value_00.value_04, &second);
        state->value_34.x = second.x - first.x;
        state->value_34.y = second.y - first.y;
        state->value_34.z = second.z - first.z;
        direction = &state->value_34;
        value = func_0025EF78(direction);
        state->value_28 = value;
        func_0025EE90(direction, 1.0f / value, direction);
        state->value_40.x = first.x + state->value_00.value_08.x;
        state->value_40.y = first.y + state->value_00.value_08.y;
        state->value_40.z = first.z + state->value_00.value_08.z;
        state->value_2C = 0.0f;
        state->value_30 = 24.0f;
        state->value_1C = func_00269798(state->value_00.value_14->value_00);
        func_0026241C(state->value_1C, 5, 11);
        state->value_20 = func_00269798(0x101);
        func_0026241C(state->value_20, 5, 11);
        func_00262830(0x1001);
        /* fall through */
    }
    case 0x1001: {
        P07Vec3 position;
        P07Vec3 *direction;
        void *destination;
        P07Render73180 *object;
        state->value_2C += state->value_30;
        if (state->value_2C >= state->value_28) {
            state->value_2C = state->value_28;
            func_00262830(0x1002);
        } else if (state->value_2C >= state->value_28 - state->value_30) {
            func_0026285C(p07_context_80220FE8->value_14, 0x1003);
        }
        direction = &state->value_34;
        func_0025EE90(direction, state->value_2C, &position);
        position.x += state->value_40.x;
        position.y += state->value_40.y;
        position.z += state->value_40.z;
        destination = state->value_1C->value_1C;
        if (state->value_00.value_18 == 0) {
            func_0026C310(direction, destination);
        } else {
            func_0026C230(direction, destination);
        }
        func_0025F150(destination, &position);
        if (++state->value_24 >= 2) {
            object = func_00269798(state->value_00.value_14->value_04);
            object->flags_0A |= 0x8000;
            func_0025F058(destination, object->value_1C);
            state->value_24 = 0;
        }
        position.y = 0.0f;
        destination = state->value_20->value_1C;
        func_0026C230(&state->value_34, destination);
        func_0025F150(destination, &position);
        break;
    }
    case 0x1002:
        func_00262434(state->value_1C);
        state->value_1C->value_10 = 12;
        func_00262830(0x1003);
        /* fall through */
    case 0x1003:
        if (func_002623E0(state->value_1C)) {
            func_002623C4(state->value_1C);
            func_002623C4(state->value_20);
            func_00262830(0x101);
        }
        break;
    }
    return 0;
}
