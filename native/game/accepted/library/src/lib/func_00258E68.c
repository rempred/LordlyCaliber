#include "game/effect_commands_e09.h"
#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_math_e04.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_callbacks_p02.h"

/* Observed fields only; this prefix does not define the allocation extent. */
typedef struct P05StagedRootState {
    u8 *context;
    u8 unknown_04[8];
    u8 value_0C;
} P05StagedRootState;


struct P05NineEntryState;
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 p01_byte_80220F68;
/* These numeric views remain within original data_0025e890. */
extern P05FactoryRecord D_80209EFC[];
extern u8 D_80209FD4[];
extern u8 D_80209FD8[];
extern void func_00262B68(void);





s32 func_00258E68(u16 message, P05StagedRootState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Reuse the scalar first-store form of the P05 pointer views. */
        *(u8 **)state = p01_context_80220F68;
        state->value_0C = p01_pointer_80220F6C[0];
        func_0026C90C(state->context[0], 0x28);
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_00262C64(0x11, 0x1001);
        break;
    case 0x1001: {
        /* Capture this byte before resetting the shared halfword. */
        u8 value = state->context[0];

        p05_halfword_80220F6A = 0;
        p01_byte_80220F68 = value;
        func_00262AB0((void *)func_00258A28);
        func_00262C64(-1, 0x1002);
        break;
    }
    case 0x1002:
        func_0024E490(state->value_0C, (const u8 *)&D_80209EFC[3], 2);
        func_0024E9BC(0, state->context[0], 0xA1);
        func_00262C64(3, 0x1003);
        break;
    case 0x1003:
        func_0024DA10(state->value_0C, &D_80209EFC[2]);
        func_00262C64(0x10, 0x1004);
        break;
    case 0x1004:
        func_0024DA50(state->value_0C, &D_80209EFC[1]);
        func_0025CA7C(state->value_0C, D_80209FD4);
        func_0025D678(D_80209FD8);
        func_00262C64(0xC, 0x1005);
        break;
    case 0x1005:
        func_0024DA10(state->value_0C, &D_80209EFC[0]);
        func_0025CAF0(state->value_0C, 0, 0);
        func_00262C64(0xA, 0x1006);
        break;
    case 0x1006:
        func_0026C90C(state->context[0], 0x2A);
        func_00262C64(6, 0x1007);
        break;
    case 0x1007:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
