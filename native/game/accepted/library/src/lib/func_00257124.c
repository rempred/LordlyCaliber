#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"

typedef struct P01Vec3 {
    float x, y, z;
} P01Vec3;

/* Same observed placement prefix used by the accepted P01 callers. */
typedef struct P01PlacementObject {
    u8 unknown_00[0x30];
    P01Vec3 value_30;
} P01PlacementObject;

typedef struct P05PlacedNode {
    u8 unknown_00[0xA];
    u16 flags_0A;
    u8 unknown_0C[0x10];
    P01PlacementObject placement_1C;
    u8 unknown_58[4];
    float value_5C;
} P05PlacedNode;

typedef struct P05CoordinateState {
    u8 value_00;
    u8 value_01;
    u8 unknown_02[2];
    P01Vec3 value_04;
    P01Vec3 value_10;
    P01Vec3 value_1C;
    float value_28;
    s16 value_2C;
    u8 unknown_2E[6];
    P05PlacedNode *object_34;
} P05CoordinateState;


extern u8 p01_byte_80220F68;
extern u8 p01_byte_80220F69;
/* Descriptor 11 view remains inside original data_0025e890. */
extern P05FactoryRecord D_80209E42[];
extern void func_00262B68(void);
extern void func_00262C80(void);
extern void func_002623C4(void *);


extern void func_0024C7A4(void *, void *, void *);




s32 func_00257124(u16 message, P05CoordinateState *state)
{
    P01Vec3 offset;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100: {
        P01Vec3 *from;
        P01Vec3 *to;
        P01Vec3 *direction;

        state->value_00 = p01_byte_80220F68;
        state->value_01 = p01_byte_80220F69;
        if (func_0024BCE8(state->value_00) != 0) {
            offset.x = -10.0f;
            offset.y = 10.0f;
            offset.z = 20.0f;
        } else {
            offset.x = 20.0f;
            offset.y = 8.0f;
            offset.z = -10.0f;
        }
        from = &state->value_04;
        func_0024C0DC(state->value_00, from, offset.x, offset.y, offset.z);
        to = &state->value_10;
        func_0024BF04(state->value_01, to, 0x10);
        direction = &state->value_1C;
        func_0024C7A4(direction, to, from);
        state->value_28 = func_0025EF78(direction);
        func_0025EFC8(direction);
        state->value_2C = 0x1A;
        func_00262830(0x1000);
        break;
    }
    case 0x1000:
        func_0024D180(&state->value_04, &D_80209E42[0]);
        func_0024E9BC(0, state->value_00, 0x94);
        func_00262C64(6, 0x1001);
        break;
    case 0x1001: {
        P01PlacementObject *object = func_0024CF10(&D_80209E42[1]);

        func_0024C7D8(&state->value_1C, object);
        object->value_30.x = state->value_04.x
            + state->value_2C * state->value_1C.x;
        object->value_30.y = state->value_04.y
            + state->value_2C * state->value_1C.y;
        object->value_30.z = state->value_04.z
            + state->value_2C * state->value_1C.z;
        state->value_28 -= state->value_2C;
        state->object_34 = (P05PlacedNode *)((u8 *)object - 0x1C);
        func_00262830(0x1002);
        break;
    }
    case 0x1002: {
        P05PlacedNode *object = state->object_34;

        object->placement_1C.value_30.x += state->value_2C * state->value_1C.x;
        object->placement_1C.value_30.y += state->value_2C * state->value_1C.y;
        object->placement_1C.value_30.z += state->value_2C * state->value_1C.z;
        state->value_28 -= state->value_2C;
        if (state->value_2C <= state->value_28 &&
            state->value_28 < state->value_2C * 2) {
            func_00262C80();
        }
        if (state->value_28 < state->value_2C) {
            func_00262830(0x1003);
        }
        break;
    }
    case 0x1003:
        state->object_34->flags_0A |= 0x2000;
        func_00262830(0x1004);
        /* Fall through: the original immediately applies this update. */
    case 0x1004: {
        P05PlacedNode *object = state->object_34;

        if ((object->value_5C -= 0.11f) <= 0.0f) {
            object->value_5C = 0.0f;
            func_002623C4(state->object_34);
            func_00262830(0x1005);
        }
        break;
    }
    case 0x1005:
        func_002628E4(0x101);
        break;
    }
    return 0;
}


