#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_callbacks_p02.h"

typedef struct P05TableRootState {
    u8 *context;
    u8 *values;
    u8 unknown_08[4];
    u8 value_0C;
} P05TableRootState;

struct P05ThreeStepState;
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_byte_80220F69;
extern P05FactoryRecord D_8020A09A[];
extern u8 D_8020A150[];
extern u8 D_8020A158[];
extern void func_00262B68(void);




s32 func_0025AACC(u16 message, P05TableRootState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *(u8 **)state = p01_context_80220F68;
        state->values = p01_pointer_80220F6C;
        state->value_0C = state->values[0];
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025D8A4((s16 *)(D_8020A150), (s16 *)(D_8020A158));
        func_00262830(0x1001);
        /* Retail continues into this case in the same invocation. */
    case 0x1001:
        p01_byte_80220F69 = state->value_0C;
        func_00262AB0((void *)func_0025A9D0);
        func_0024DA10(state->value_0C, &D_8020A09A[2]);
        func_0024E9BC(0, state->context[0], 0xB0);
        func_00262830(0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_0C, &D_8020A09A[0]);
        func_00262C64(3, 0x1003);
        break;
    case 0x1003:
        func_0024DA10(state->value_0C, &D_8020A09A[1]);
        func_00262C64(0x14, 0x1004);
        break;
    case 0x1004: {
        P05FactoryRecord *record = &D_8020A09A[3];

        func_0024DA10(state->value_0C, record);
        func_0024DA50(state->value_0C, record + 1);
        func_0024DA10(state->value_0C, record + 2);
        func_0025CAF0(state->value_0C, 0xF, 5);
        func_0025CB60((u32)(state->values + 1), 0x14, 3);
        func_00262830(0x1005);
        break;
    }
    case 0x1005:
        func_0024DA10(state->value_0C, &D_8020A09A[6]);
        func_00262C64(7, 0x1006);
        break;
    case 0x1006:
        func_0024DA10(state->value_0C, &D_8020A09A[7]);
        func_00262C64(0x19, 0x1007);
        break;
    case 0x1007:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
