#include "game/effect_adapters_e08.h"
#include "game/effect_dispatch_e07.h"
#include "game/effect_math_e04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P02Vec3 { float x, y, z; } P02Vec3;
typedef struct P02MotionState {
    P02Vec3 value_00, value_0C;
    float value_18, value_1C, value_20;
    u8 unknown_24[0xC];
    float value_30[4], value_40[4], value_50[4];
} P02MotionState;
typedef struct P02RenderObject {
    u8 unknown_00[0xA];
    u16 flags_0A;
    u8 unknown_0C[4];
    u32 value_10;
    u8 unknown_14[8];
    u8 value_1C[0x40];
} P02RenderObject;
typedef struct P02ChildState {
    P02Vec3 value_00, value_0C, value_18;
    u8 unknown_24[3];
    u8 value_27;
    u32 value_28;
    s32 value_2C;
    P02RenderObject *value_30;
    s16 value_34;
    u16 value_36;
    u32 unknown_38;
    P02MotionState *value_3C;
} P02ChildState;

extern void func_000016C4(void *);
extern void func_00262B68(void);
extern void func_00023780(void *, s32);
extern void *func_00001330(u32);

extern void func_0026241C(P02RenderObject *, s32, s32);
extern void func_00262830(s32);


extern void func_00262434(void *);
extern void func_002623C4(P02RenderObject *);
extern void *func_0020C478(u32);


extern void func_0020BDCC(s16, s32);
extern u8 *p01_context_80220F68;
extern u8 D_800EB290[], D_800EB240[];

/* Keeping the complete phase inline lets the pinned compiler rematerialize
 * the target-vector address at each call, preserving the retail saved registers. */
static __inline__ void p02_message_1004(P02ChildState *state)
{
    s32 value;
    P02Vec3 target;
    void *ptr;
    P02RenderObject *object;
    func_002623C4(state->value_30);
    ptr = func_0020C478(state->value_28);
    func_0026C7C4(ptr, &target);
    object = func_00269798(0x23);
    object->flags_0A |= 0x8000;
    func_0026C684(ptr, object->value_1C);
    ptr = object->value_1C;
    func_0025F150(ptr, &target);
    object = func_0026a45c(0x24);
    object->flags_0A |= 0x8000;
    func_0026C3F8(object->value_1C);
    ptr = object->value_1C;
    func_0025F150(ptr, &target);
    object = func_0026a45c(0x25);
    object->flags_0A |= 0x8000;
    func_0026C3F8(object->value_1C);
    ptr = object->value_1C;
    func_0025F150(ptr, &target);
    func_0020BDCC((s16)state->value_28, 10);
    value = 0xDA;
    switch (state->value_2C) {
    case 6: value = 0xDC; break;
    case 7: value = 0xDE; break;
    }
    func_0024E9BC((s32)D_800EB240, state->value_27, value);
    func_00262830(0x1005);
}

s32 func_0026ce74(u16 message, P02ChildState *state)
{
    s32 result = 0;
    void *tempPtr;
    P02Vec3 position;
    float height;
    float zero;
    s32 value;

    switch (message) {
    case 0x101:
        func_000016C4(state->value_3C);
        func_00262B68();
        break;
    case 0x100:
        func_00023780(state, 0x40);
        state->value_3C = func_00001330(0x60);
        state->value_30 = func_0026BF74(0x28);
        func_0026241C(state->value_30, 1, 15);
        func_00262830(0x1002);
        break;
    case 0x1002: {
        if (func_0024BCE8(state->value_27)) {
            state->value_0C.x = 247.0f;
            state->value_00.x = -1.0f;
        } else {
            state->value_0C.x = -247.0f;
            state->value_00.x = 1.0f;
        }
        state->value_0C.z = 0.0f;
        state->value_0C.y = 60.0f;
        zero = state->value_0C.z;
        state->value_00.z = zero;
        state->value_00.y = zero;
        state->value_18.x = zero;
        state->value_18.y = 1.0f;
        state->value_18.z = zero;
        func_0026cd1c(state);
        tempPtr = state->value_30;
        tempPtr = ((P02RenderObject *)tempPtr)->value_1C;
        func_0025F530(&state->value_00, &state->value_18, tempPtr);
        func_0025F150(tempPtr, &state->value_0C);
        value = 0xD9;
        switch (state->value_2C) {
        case 6: value = 0xDB; break;
        case 7: value = 0xDD; break;
        }
        func_0024E9BC((s32)D_800EB290, state->value_27, value);
        func_00262830(0x1003);
        break;
    }
    case 0x1003: {
        P02MotionState *motion;
        P02RenderObject *object;
        motion = state->value_3C;
        if (motion->value_1C <= (motion->value_18 += motion->value_20)) {
            motion->value_18 = motion->value_1C;
            func_00262830(0x1004);
        }
        height = func_0026CBDC(motion->value_18, 4, motion->value_30,
                               motion->value_40, motion->value_50);
        func_0025EE90(&motion->value_0C, motion->value_18, &position);
        position.x += motion->value_00.x + height * 0.0f;
        position.y += motion->value_00.y + height;
        position.z += motion->value_00.z + height * 0.0f;
        state->value_00.x = position.x - state->value_0C.x;
        state->value_00.y = position.y - state->value_0C.y;
        state->value_00.z = position.z - state->value_0C.z;
        tempPtr = state->value_30;
        state->value_0C.x = position.x;
        state->value_0C.y = position.y;
        state->value_0C.z = position.z;
        tempPtr = ((P02RenderObject *)tempPtr)->value_1C;
        func_0025F530(&state->value_00, &state->value_18, tempPtr);
        func_0025F150(tempPtr, &state->value_0C);
        if (++state->value_34 >= 4) {
            state->value_34 = 0;
            p01_context_80220F68 = (u8 *)state->value_30->value_10;
            func_0025F058(state->value_30->value_1C,
                          (u8 *)func_00262AB0((void *)func_0026d670) + 0x24);
        }
        if (motion->value_1C / 2.0f <= motion->value_18) {
            state->value_36 = 1;
            func_00262434(state->value_30);
            state->value_30->value_10 = 0x10;
        }
        break;
    }
    case 0x1004:
        p02_message_1004(state);
        break;
    case 0x1000:
        if (state->value_36) result = 1;
        break;
    case 0x1001:
        if (*(u16 *)((u8 *)state - 0x1C) == 0x1005) result = 1;
        break;
    }
    return result;
}
