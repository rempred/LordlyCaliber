#include "game/effect_math_e04.h"
#include "common/types.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

typedef struct P01PlacementState {
    u8 *context;
    u8 value_04;
    u8 unknown_05[3];
    P01Vec3 value_08;
    P01Vec3 value_14;
    P01Vec3 value_20;
    s16 index_2C;
} P01PlacementState;

typedef struct P01PlacementObject {
    u8 unknown_00[0x30];
    P01Vec3 value_30;
} P01PlacementObject;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_data_80209BB0[];
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_00262C64(s16, s16);
extern void func_002628E4(s32);
extern void func_00262C80(void);
extern void func_0024C204(u8 *, P01Vec3 *, s32);
extern void func_0024BF04(u8, P01Vec3 *, s32);
extern void func_0024C7A4(void *, void *, void *);
extern P01PlacementObject *func_0024CFAC(void *, s32);
extern P01PlacementObject *func_0024CDD8(void *);
extern void func_0024C7D8(P01Vec3 *, P01PlacementObject *);
extern void func_0024E9BC(s32, u8, s32);
extern void *func_0024D180(P01Vec3 *, void *);

static inline s32 p01_select_index(P01PlacementState *state, const s16 *cells)
{
    /* The halfword flag preserves the observed return/join scheduling. */
    s16 valid = 1;

    switch (cells[1]) {
    case 2:
        state->index_2C = cells[0] + 1;
        break;
    case 1:
        if (cells[0] != 0) {
            state->index_2C = cells[0];
        } else {
            valid = 0;
        }
        break;
    case 0:
        if (cells[0] == 2) {
            state->index_2C = 1;
        } else {
            valid = 0;
        }
        break;
    }
    return valid;
}

s32 func_00254220(u16 message, P01PlacementState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100: {
        P01Vec3 *from = &state->value_08;
        P01Vec3 *to = &state->value_14;
        P01Vec3 *direction = &state->value_20;

        state->context = p01_context_80220F68;
        state->value_04 = *(u8 *)&p01_pointer_80220F6C;
        state->index_2C = 0;
        func_0024C204(state->context, from, 0);
        func_0024BF04(state->value_04, to, 0);
        func_0024C7A4(direction, to, from);
        func_0025EFC8(direction);
        func_00262830(0x1000);
        break;
    }
    case 0x1000: {
        s16 cells[2];
        s32 endX;
        s32 startX;

        endX = (s32)state->value_14.x;
        cells[0] = (endX < 0 ? -endX : endX) / 38 - 2;
        startX = (s32)state->value_08.x;
        cells[1] = (startX < 0 ? -startX : startX) / 38 - 2;
        func_00262830(p01_select_index(state, cells) ? 0x1002 : 0x1004);
        break;
    }
    case 0x1002: {
        P01PlacementObject *object = func_0024CFAC(p01_data_80209BB0, 4);
        P01PlacementObject *auxiliary = func_0024CDD8(p01_data_80209BB0 + 30);

        func_0024C7D8(&state->value_20, object);
        object->value_30.x = state->value_08.x
            + (state->index_2C * 38) * state->value_20.x;
        object->value_30.y = state->value_08.y
            + (state->index_2C * 38) * state->value_20.y;
        object->value_30.z = state->value_08.z
            + (state->index_2C * 38) * state->value_20.z;
        func_0025F058(object, auxiliary);
        func_0024E9BC(0, state->context[0], 0x7A);
        func_00262C64(0xB, 0x1003);
        break;
    }
    case 0x1003: {
        P01PlacementObject *object = func_0024CDD8(p01_data_80209BB0 + 6);
        P01Vec3 position;

        func_0024C7D8(&state->value_20, object);
        position.x = state->value_08.x
            + ((state->index_2C + 2) * 38) * state->value_20.x;
        position.y = state->value_08.y
            + ((state->index_2C + 2) * 38) * state->value_20.y;
        position.z = state->value_08.z
            + ((state->index_2C + 2) * 38) * state->value_20.z;
        object->value_30.x = position.x;
        object->value_30.y = position.y;
        object->value_30.z = position.z;
        func_0024D180(&position, p01_data_80209BB0 + 18);
        func_00262C64(4, 0x1004);
        break;
    }
    case 0x1004: {
        P01PlacementObject *object = func_0024CFAC(p01_data_80209BB0, 4);
        P01PlacementObject *auxiliary = func_0024CDD8(p01_data_80209BB0 + 30);

        func_0024C7D8(&state->value_20, object);
        object->value_30.x = state->value_14.x + -76.0f * state->value_20.x;
        object->value_30.y = state->value_14.y + -76.0f * state->value_20.y;
        object->value_30.z = state->value_14.z + -76.0f * state->value_20.z;
        func_0025F058(object, auxiliary);
        func_0024E9BC(0, state->context[0], 0x7A);
        func_00262C64(0xB, 0x1005);
        break;
    }
    case 0x1005:
        func_00262C80();
        func_002628E4(0x101);
        break;
    }
    return 0;
}
