#include "game/effect_adapters_e08.h"
#include "game/effect_dispatch_e07.h"
#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02Vec271E5C { float x, y, z; } P02Vec271E5C;
typedef struct P02Motion271E5C {
    P02Vec271E5C value_00, value_0C;
    float value_18, value_1C, value_20;
} P02Motion271E5C;
typedef struct P02Render271E5C {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C, value_10;
    u8 unknown_14[8];
    float value_1C[16];
} P02Render271E5C;
typedef struct P02State271E5C {
    P02Vec271E5C value_00;
    u8 unknown_0C[3], value_0F;
    u32 value_10, value_14;
    s32 value_18;
    P02Render271E5C *value_1C, *value_20;
    s16 value_24;
    u16 value_26;
    P02Motion271E5C value_28;
} P02State271E5C;
typedef struct P02State27267C P02State27267C;

extern void func_00262B68(void);
extern void func_00023780(void *, s32);

extern void func_00262830(s32), func_00262C64(s16, s16);


extern void func_0020BDCC(s16, s32);
extern void func_002623C4(P02Render271E5C *);
extern u8 *p01_context_80220F68;
extern u8 D_800EB290[], D_800EB240[];
extern float D_80220EE0[16];

/* Complete phase scopes make the pinned compiler rematerialize the shared
 * matrix address and preserve the retail saved-register and scheduling shape. */
static __inline__ void p02_update_271E5C(P02State271E5C *state, P02Vec271E5C *position)
{
    void *matrix;
    s32 i;
    P02Motion271E5C *motion;
    state->value_28.value_18 += state->value_28.value_20;
    motion = &state->value_28;
    if (motion->value_1C <= motion->value_18) {
        motion->value_18 = motion->value_1C;
        func_00262830(0x1006);
    }
    for (i = 0; i < 3; ++i)
        ((float *)&state->value_00)[i] = ((float *)&motion->value_0C)[i]
                                      * motion->value_18
                                      + ((float *)&motion->value_00)[i];
    matrix = state->value_1C;
    matrix = (u8 *)matrix + 0x1C;
    func_0025F058(D_80220EE0, matrix);
    func_0025F150(matrix, &state->value_00);
    matrix = state->value_20;
    matrix = (u8 *)matrix + 0x1C;
    func_0025F058(D_80220EE0, matrix);
    position->x = state->value_00.x;
    position->y = 0.0f;
    position->z = state->value_00.z;
    func_0025F150(matrix, position);
    if (motion->value_1C * 0.5f <= motion->value_18)
        state->value_26 = 1;
    if (++state->value_24 >= 4) {
        p01_context_80220F68 = (u8 *)state->value_1C->value_10;
        matrix = func_00262AB0((void *)func_0027267C);
        func_0025F058(state->value_1C->value_1C, (u8 *)matrix + 0x24);
        state->value_24 = 0;
    }
}

static __inline__ void p02_phase_1006_271E5C(P02State271E5C *state)
{
    void *matrix;
    s32 value;
    P02Render271E5C *object;
    u16 flags;
    object = func_00269798(0x6C);
    /* This separate value keeps the flag load before the interior calculation
     * under the pinned compiler, without another read or a volatile access. */
    flags = object->flags_0A;
    matrix = object->value_1C;
    object->value_0C = -10000;
    object->flags_0A = flags | 0x9000;
    func_0025F058(D_80220EE0, matrix);
    func_0025F150(matrix, &state->value_00);
    object = func_00269798(0x6D);
    object->flags_0A |= 0x8000;
    func_0025F058(matrix, object->value_1C);
    object = func_00269798(0x6F);
    object->flags_0A |= 0x8000;
    func_0025F058(matrix, object->value_1C);
    func_0020BDCC((s16)state->value_10, 10);
    value = 0xF2;
    switch (state->value_18) {
    case 45: value = 0xF4; break;
    case 46: value = 0xF6; break;
    }
    func_0024E9BC((s32)D_800EB240, state->value_0F, value);
    func_00262C64(10, 0x1007);
}

s32 func_00271E5C(u16 message, P02State271E5C *state)
{
    s32 result = 0;
    P02Vec271E5C target, position;
    void *matrix;
    s32 value;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        func_00023780(state, 0x4C);
        state->value_1C = func_0026BF74(0x6E);
        state->value_00.x = state->value_00.y = state->value_00.z = 0.0f;
        func_00262830(0x1002);
        break;
    case 0x1002:
        if (func_0024BCE8(state->value_0F))
            state->value_00.x = 95.0f;
        else
            state->value_00.x = -95.0f;
        state->value_00.z = 0.0f;
        state->value_00.y = -10.0f;
        matrix = state->value_1C;
        matrix = (u8 *)matrix + 0x1C;
        func_0025F058(D_80220EE0, matrix);
        func_0025F150(matrix, &state->value_00);
        value = 0xF1;
        switch (state->value_18) {
        case 45: value = 0xF3; break;
        case 46: value = 0xF5; break;
        }
        func_0024E9BC((s32)D_800EB290, state->value_0F, value);
        func_00262830(0x1003);
        break;
    case 0x1003:
        func_0026C834(state->value_10, &target);
        state->value_28.value_0C.x = target.x - state->value_00.x;
        state->value_28.value_0C.y = target.y - state->value_00.y;
        state->value_28.value_0C.z = target.z - state->value_00.z;
        matrix = &state->value_28.value_0C;
        state->value_28.value_1C = func_0025EF78(matrix);
        func_0025EE90(matrix, 1.0f / state->value_28.value_1C, matrix);
        state->value_28.value_18 = 0.0f;
        state->value_28.value_00.x = state->value_00.x;
        state->value_28.value_00.y = state->value_00.y;
        state->value_28.value_00.z = state->value_00.z;
        state->value_28.value_20 = state->value_28.value_1C / 28.0f;
        func_00262C64(2, 0x1004);
        break;
    case 0x1004:
        state->value_20 = func_00269798(0x70);
        state->value_20->value_0C -= 500;
        func_00262830(0x1005);
        /* The original initialization continues through the first update. */
    case 0x1005:
        p02_update_271E5C(state, &position);
        break;
    case 0x1006:
        p02_phase_1006_271E5C(state);
        break;
    case 0x1007:
        func_002623C4(state->value_1C);
        func_002623C4(state->value_20);
        func_00262830(0x1008);
        break;
    case 0x1008:
        /* Explicit idle case preserves the pinned compiler's dispatch shape. */
        break;
    case 0x1000:
        result = state->value_26;
        break;
    case 0x1001:
        if (*(u16 *)((u8 *)state - 0x1C) == 0x1008)
            result = 1;
        break;
    }
    return result;
}
