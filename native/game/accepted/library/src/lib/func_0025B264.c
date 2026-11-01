#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

/* Numeric prefix only; this view does not define the allocation extent. */
typedef struct P05Value0CState {
    u8 *context;
    u8 *values;
    u8 unknown_08[4];
    u8 value_0C;
} P05Value0CState;

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_8020A11E[];
extern u8 D_8020A160[];
extern u8 D_8020A168[];
extern void func_00262B68(void);



s32 func_0025B264(u16 message, P05Value0CState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Retain the observed context store before the shared-pointer load. */
        *(u8 **)state = p01_context_80220F68;
        state->values = p01_pointer_80220F6C;
        state->value_0C = state->values[0];
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025D8A4((s16 *)(D_8020A160), (s16 *)(D_8020A168));
        func_00262830(0x1001);
        /* Retail continues into the next case in this invocation. */
    case 0x1001:
        func_0024DA10(state->value_0C, D_8020A11E);
        func_0024E9BC(0, state->context[0], 0xB4);
        func_0025CAF0(state->value_0C, 0x32, 5);
        func_0025CB60((u32)(state->values + 1), 0x37, 0);
        func_00262C64(0x40, 0x1002);
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
