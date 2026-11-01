#include "game/effect_adapters_e08.h"
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



extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_800EB240[];
extern u8 D_802098E4[];
extern u8 D_802098F0[];
extern u8 D_80209940[];
extern u8 D_80209948[];

s32 func_00250A5C(u16 message, P04RootState *state)
{
    P04Vec3 positions[4];

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
        func_0024E490(state->value_08, D_802098E4, 2);
        func_0024E9BC((s32)D_800EB240, state->context[0], 0x4D);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209940);
        func_00262C64(0xB, 0x1002);
        break;
    case 0x1002:
        func_0024C57C(state->value_08, positions, 0);
        func_0024D180(&positions[0], D_802098F0 + 12);
        func_0024D180(&positions[1], D_802098F0);
        func_0024D180(&positions[2], D_802098F0 + 6);
        func_0024D180(&positions[3], D_802098F0 + 18);
        func_0025CAF0(state->value_08, 0xF, 0);
        func_0025CB60((u32)(state->pointer + 1), 0xA, 0);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209948);
        func_00262C64(0x41, 0x1003);
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
