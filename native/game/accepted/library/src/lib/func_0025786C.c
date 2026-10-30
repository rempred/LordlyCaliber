#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04Vec3 {
    float x;
    float y;
    float z;
} P04Vec3;

typedef struct P04Pointers0CState {
    u8 *context;
    u8 *pointer;
    u8 *pointer_08;
    u8 value_0C;
    u8 unknown_0D;
    u16 unknown_0E;
} P04Pointers0CState;

extern void func_00262B68(void);
extern s16 func_0024BCE8(u8);
extern void func_0024C0DC(u8, P04Vec3 *, float, float, float);
extern void *func_0024D41C(P04Vec3 *, void *);
extern void func_0024DA50(u8, void *);
extern void func_0024E9BC(s32, u8, s32);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 *D_80220F70;
extern u8 D_80209E84[];
extern u8 D_80209E8A[];
extern u8 D_80209E90[];

s32 func_0025786C(u16 message, P04Pointers0CState *state)
{
    P04Vec3 position;
    P04Vec3 offset;
    u8 *extra;
    u8 *pointer;
    s16 count;
    s16 index;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *(u8 **)state = p01_context_80220F68;
        *(u8 **)((u8 *)state + 4) = p01_pointer_80220F6C;
        extra = D_80220F70;
        /* This access retains the init-only reload at offset 4. The loop uses
         * ordinary accesses, as its condition supplies the body pointer. */
        pointer = *(u8 *volatile *)&state->pointer;
        state->pointer_08 = extra;
        state->value_0C = *pointer;
        state->unknown_0E = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        if (func_0024BCE8(state->context[0]) != 0) {
            offset.x = -30.0f;
            offset.y = 20.0f;
            offset.z = 0.0f;
        } else {
            offset.x = 20.0f;
            offset.y = 5.0f;
            offset.z = -15.0f;
        }
        func_0024C0DC(state->context[0], &position, offset.x, offset.y, offset.z);
        func_0024D41C(&position, D_80209E84);
        func_0024E9BC(0, state->context[0], 0x98);
        func_00262C64(0x13, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_0C, D_80209E8A);
        func_0024E9BC(0, state->context[0], 0x99);
        func_00262830(0x1002);
        break;
    case 0x1002:
        func_0025CAF0(state->value_0C, 3, 0);
        if (state->pointer_08[0] != 0) {
            func_0024DA50(state->value_0C, D_80209E90);
        }
        func_00262C64(0x16, 0x1003);
        break;
    case 0x1003:
        func_0025CB60((u32)(state->pointer + 1), 3, 0);
        count = 1;
        for (index = 1; state->pointer[index] != 0xFF; index++) {
            if (state->pointer_08[index] != 0) {
                func_0024DA50(state->pointer[index], D_80209E90);
                count++;
            }
        }
        if (count == 1) {
            func_00262C64(7, 0x1004);
        } else {
            func_00262C64(0xF, 0x1004);
        }
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
