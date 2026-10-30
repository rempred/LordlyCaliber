#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04Vec3 {
    float x;
    float y;
    float z;
} P04Vec3;

typedef struct P04RootState {
    u8 *context;
    u8 *pointer;
    u8 value_08;
} P04RootState;

extern void func_00262B68(void);
extern void func_0024DA50(u8, void *);
extern void func_0024E9BC(s32, u8, s32);
extern void func_0024C57C(u8, P04Vec3 *, s32);
extern void *func_0024D180(P04Vec3 *, void *);
extern s16 func_0024C2F8(P04Vec3 *);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_802098E4[];
extern u8 D_802098F0[];
extern u8 D_8020990E[];
extern u8 D_80209950[];
extern u8 D_80209958[];

s32 func_00250C24(u16 message, P04RootState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Retain the observed context store before the shared-pointer load. */
        *(u8 **)state = p01_context_80220F68;
        state->pointer = p01_pointer_80220F6C;
        state->value_08 = *state->pointer;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_08, D_802098E4);
        func_0024DA10(state->value_08, D_802098E4 + 0x24);
        func_0024E9BC(0, state->context[0], 0x4E);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209950);
        func_00262C64(0xB, 0x1002);
        break;
    case 0x1002: {
        P04Vec3 positions[4];
        s16 positionIndex;

        func_0024C57C(state->value_08, positions, 0);
        func_0024D180(&positions[0], D_802098F0 + 12);
        func_0024D180(&positions[1], D_802098F0);
        func_0024D180(&positions[2], D_802098F0 + 6);
        func_0024D180(&positions[3], D_802098F0 + 18);
        func_0025CAF0(state->value_08, 0xF, 0);
        for (positionIndex = 0; positionIndex < 4; positionIndex++) {
            s16 value = func_0024C2F8(&positions[positionIndex]);

            if (value != -1) {
                s16 index;

                for (index = 1; state->pointer[index] != (u8)value; index++) {
                }
                func_0025CAF0(state->pointer[index], 0xA, 0);
            }
        }
        func_00262C64(0x1A, 0x1003);
        break;
    }
    case 0x1003: {
        P04Vec3 positions[4];
        s16 positionIndex;

        func_0024C57C(state->value_08, positions, 1);
        func_0024DA50(state->value_08, D_8020990E);
        for (positionIndex = 0; positionIndex < 4; positionIndex++) {
            s16 value = func_0024C2F8(&positions[positionIndex]);

            if (value != -1) {
                s16 index;

                for (index = 1; state->pointer[index] != (u8)value; index++) {
                }
                func_0025CAF0(state->pointer[index], 0xA, 0);
            }
        }
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209958);
        func_00262C64(0x28, 0x1004);
        break;
    }
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
