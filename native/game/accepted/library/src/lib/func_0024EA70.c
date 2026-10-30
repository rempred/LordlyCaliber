#include "game/effect_math_e04.h"
#include "common/types.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

typedef struct P01ChildObject {
    u8 unknown_00[0x1C];
    P01Vec3 value_1C;
    u8 unknown_28[0x0C];
    float value_34;
} P01ChildObject;

typedef struct P01ChildState {
    u8 *context;
    u8 value_04;
    u8 unknown_05[3];
    P01Vec3 value_08;
    P01Vec3 value_14;
    float value_20;
    s16 value_24;
    u16 value_26;
    P01ChildObject *object_28;
} P01ChildState;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_data_80209780[];
extern u8 p01_data_8020978C[];
extern u8 p01_data_80209792[];
extern u8 p01_data_802097C8[];
extern const double p01_double_8020A240;

extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_002628E4(s32);
extern void func_00262C64(s16, s16);
extern void func_00262C80(void);
extern void func_0024C204(u8 *, P01Vec3 *, s32);
extern void func_0024BF04(u8, P01Vec3 *, s32);
extern void func_0024C7A4(P01Vec3 *, P01Vec3 *, P01Vec3 *);
extern void func_0024D180(P01Vec3 *, void *);
extern void func_0024E9BC(s32, u8, s32);
extern void func_0025DB00(u8 *, void *);
extern P01ChildObject *func_0024E510(P01Vec3 *, void *);
extern void func_002623C4(P01ChildObject *);
extern u32 func_00022e60(void);

s32 func_0024EA70(u16 message, P01ChildState *state)
{
    P01Vec3 initial;
    P01Vec3 emitted;
    P01ChildObject *object;
    s16 remainder;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state->context = p01_context_80220F68;
        state->value_04 = *(u8 *)&p01_pointer_80220F6C;
        func_0024C204(state->context, &state->value_08, 0x20);
        func_0024BF04(state->value_04, &initial, 0);
        func_0024C7A4(&state->value_14, &initial, &state->value_08);
        state->value_20 = func_0025EF78(&state->value_14);
        func_0025EFC8(&state->value_14);
        state->value_24 = 0x1C;
        state->value_26 = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0024D180(&state->value_08, p01_data_80209780);
        func_0024E9BC(0, state->context[0], 0x27);
        func_0025DB00(state->context, p01_data_802097C8);
        func_00262C64(0xA, 0x1001);
        break;
    case 0x1001:
        state->object_28 = func_0024E510(&state->value_08, p01_data_80209792);
        func_00262C64(8, 0x1002);
        break;
    case 0x1002:
        object = state->object_28;
        if (--state->value_24 < 0) {
            state->value_24 = 0;
        }
        object->value_1C.x += (float)state->value_24 * state->value_14.x;
        object->value_1C.y += (float)state->value_24 * state->value_14.y;
        object->value_1C.z += (float)state->value_24 * state->value_14.z;
        state->value_20 -= (float)state->value_24;
        func_0026CCB4(object->value_1C.x, object->value_1C.y,
                      object->value_1C.z, 0xFF);
        if (state->value_20 < (float)state->value_24) {
            func_00262C80();
            func_00262830(0x1003);
            break;
        }
        if ((state->value_26++ & 1) == 0) {
            s32 paritySign;

            emitted.x = object->value_1C.x;
            emitted.y = object->value_1C.y;
            emitted.z = object->value_1C.z;
            /* This consumer interprets the returned word as signed. */
            remainder = (s16)((s32)func_00022e60() % 7);
            /* Keep the parity sign as an integer multiplier: odd +1, even -1. */
            paritySign = -((remainder & 1) == 0) | 1;
            emitted.z += (float)(paritySign * remainder);
            func_0024D180(&emitted, p01_data_8020978C);
        }
        break;
    case 0x1003: {
        P01ChildObject *current = state->object_28;

        if ((current->value_34 -= p01_double_8020A240) < 0.0f) {
            current->value_34 = 0.0f;
            func_002623C4(state->object_28);
            func_00262830(0x1004);
        }
        break;
    }
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
