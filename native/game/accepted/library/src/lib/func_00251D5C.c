#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

typedef struct P05PointerState {
    u8 *context;
    u8 *values;
    u8 first;
} P05PointerState;

struct P05ScaledObject;
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
/* Descriptor 11 views remain inside original data_0025e318. */
extern u8 D_80209A2C[];
extern u8 D_80209A38[];
extern u8 D_80209A40[];
extern void func_00262B68(void);




s32 func_00251D5C(u16 message, P05PointerState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Preserve the observed first store before the shared-pointer load. */
        *(u8 **)state = p01_context_80220F68;
        state->values = p01_pointer_80220F6C;
        state->first = state->values[0];
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001: {
        s16 index;

        func_0025D8A4((s16 *)(D_80209A2C), (s16 *)(D_80209A38));
        for (index = 0; state->context[index] != 0xFF; index++) {
            func_0025CA7C(state->context[index], D_80209A40);
        }
        func_00262830(0x1002);
    }
        /* fall through */
    case 0x1002:
        func_0024E408(0x89);
        func_0024E430(0x8A, 1.2f);
        func_0024E9BC(0, state->context[0], 0x59);
        func_00262C64(0x1B, 0x1003);
        break;
    case 0x1003:
        func_0024E408(0x86);
        func_00262C64(0x30, 0x1004);
        break;
    case 0x1004:
        func_0024E408(0x8B);
        func_0024E9BC(0, state->context[0], 0x5A);
        func_00262C64(0x14, 0x1005);
        break;
    case 0x1005:
        func_0024E430(0x87, 1.2f);
        func_00262C64(0x14, 0x1006);
        break;
    case 0x1006:
        func_0024E430(0x88, 1.1f);
        func_0025CAF0(state->first, 0x32, 0);
        func_0025CB60((u32)(state->values + 1), 0x32, 0);
        func_00262C64(0x36, 0x1007);
        break;
    case 0x1007:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
